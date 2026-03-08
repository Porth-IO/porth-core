/**
 * @file PorthSimDevice.hpp
 * @brief High-fidelity Digital Twin for Cardiff Photonics hardware simulation.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <iomanip>
#include <random>
#include <string>
#include <thread>

#include "PorthMockDevice.hpp"
#include "PorthRingBuffer.hpp"
#include "PorthShuttle.hpp"
#include "PorthSimPHY.hpp"
#include "PorthUtil.hpp"

namespace porth {

/** @brief Simulation Constants representing physical hardware limits of the Newport Cluster. */
constexpr uint32_t SIM_BASE_TEMP_MC       = 25000;  ///< 25.0°C Ambient/Start temp.
constexpr uint32_t SIM_TEMP_INC_MC        = 100;    ///< Heating rate per step (0.1°C).
constexpr uint32_t SIM_TEMP_DEC_MC        = 50;     ///< Passive cooling rate per step (0.05°C).
constexpr uint32_t SIM_STATUS_MAX_BIT     = 31;     ///< Bit-width of the hardware status register.
constexpr size_t SIM_DEFAULT_SHUTTLE_SIZE = 1024;   ///< Number of descriptors in the DMA ring.
constexpr int SIM_PHYSICS_STEP_US         = 100;    ///< Physics update frequency (10kHz).
constexpr int SIM_BUS_HANG_MS             = 100;    ///< Simulated duration of a PCIe TLP timeout.
constexpr int SIM_OVERFLOW_ITERATIONS     = 2048;   ///< Iterations to force RingBuffer saturation.
constexpr uint32_t SIM_DESCRIPTOR_LEN_DEFAULT = 64; ///< Standard flit-aligned descriptor length.

/**
 * @class PorthSimDevice
 * @brief The high-fidelity Digital Twin for Cardiff Photonics.
 *
 * This class simulates the physical, electrical, and protocol-level behaviors
 * of a compound semiconductor device on a PCIe Gen 6 bus. It manages a background
 * physics thread that models thermal drift and power rail noise.
 * * @note This is the Sovereign testing ground; it allows for "Chaos Engineering"
 * by injecting hardware-level failures that would be dangerous or impossible
 * to trigger on physical GaN hardware.
 */
class PorthSimDevice {
public:
    // Hardware simulators represent unique physical units; copying is prohibited
    // to prevent logical aliasing of the "Hardware" identity.
    PorthSimDevice(const PorthSimDevice&)                    = delete;
    auto operator=(const PorthSimDevice&) -> PorthSimDevice& = delete;

    // Moving is deleted to maintain stability of the background physics thread
    // and its association with the 'this' pointer.
    PorthSimDevice(PorthSimDevice&&)                    = delete;
    auto operator=(PorthSimDevice&&) -> PorthSimDevice& = delete;

private:
    PorthMockDevice m_mock_hw; ///< Shared memory backend simulating physical BARs.
    PorthSimPHY m_phy;         ///< Signal propagation model including jitter/attenuation.
    std::ofstream m_tlp_log;   ///< Persistent log for Transaction Layer Packet (TLP) auditing.

    // Physics & Simulation Threads
    std::thread m_physics_thread;      ///< Models the non-linear physics of the InP lattice.
    std::atomic<bool> m_run_sim{true}; ///< Lifecycle guard for the simulation engine.

    // --- Chaos & Error Injection State ---
    std::atomic<bool> m_inject_deadlock{
        false}; ///< Halts the hardware state machine (firmware crash).
    std::atomic<bool> m_corrupt_status{false}; ///< Simulates Single Event Upsets (bit-flips).
    std::atomic<bool> m_bus_hang{false};       ///< Simulates PCIe root-complex congestion/timeouts.

    // Sovereign Guard: Tracks the first valid address to detect pointer corruption.
    std::atomic<uint64_t> m_last_valid_shuttle{0};

    /** * @brief Internal helper to drain the DMA Shuttle.
     * * Models the Newport chip's internal DMA scheduler fetching work items.
     * @param dev Pointer to the register map.
     */
    void process_dma(PorthDeviceLayout* dev) noexcept {
        const uint64_t shuttle_addr = dev->data_ptr.load();
        if (shuttle_addr == 0) {
            return;
        }

        // Sovereign Guard: If the shuttle address changes after init, we assume
        // a corruption event (chaos) and halt processing to protect the host.
        uint64_t expected = m_last_valid_shuttle.load();
        if (expected != 0 && shuttle_addr != expected) {
            return;
        }

        if (expected == 0) {
            m_last_valid_shuttle.store(shuttle_addr);
        }

        // reinterpret_cast: Safe because the software layer guarantees the
        // shuttle_addr points to a PorthRingBuffer constructed via Placement New.
        auto* ring = reinterpret_cast<PorthRingBuffer<SIM_DEFAULT_SHUTTLE_SIZE>*>(shuttle_addr);
        PorthDescriptor desc{};

        // Drain the ring: Simulates flit processing on the Newport Cluster ASIC.
        while (ring->pop(desc)) {
            // Processing logic simulated by 'pop' latency.
        }
    }

    /** * @brief Internal helper to update the Photonics thermal lattice.
     * * Models the Indium Phosphide thermal coefficient where switching
     * activity (control == 0x1) generates heat.
     */
    void update_thermal_model(PorthDeviceLayout* dev, uint32_t& current_temp) noexcept {
        if (dev->control.load() == 0x1) {
            current_temp += SIM_TEMP_INC_MC;
        } else if (current_temp > SIM_BASE_TEMP_MC) {
            current_temp -= SIM_TEMP_DEC_MC;
        }

        // Commit temp to register to simulate real-time hardware telemetry.
        dev->laser_temp.write(current_temp);
        m_phy.update_thermal_load(current_temp);
    }

    /** * @brief Internal helper to simulate Chaos (SEU/Noise).
     * * Randomly flips bits in registers to simulate high-EMI environments
     * common in GaN power-switching stages.
     */
    void apply_chaos_effects(PorthDeviceLayout* dev,
                             std::mt19937& gen,
                             std::uniform_int_distribution<uint32_t>& bit_dist) noexcept {
        if (m_corrupt_status.load(std::memory_order_relaxed)) {
            const uint32_t current_status = dev->status.load();

            // 10% probability of critical DMA pointer corruption vs status bit-flip.
            if (bit_dist(gen) > 28) {
                dev->data_ptr.write(dev->data_ptr.load() ^ (1ULL << bit_dist(gen)));
            } else {
                dev->status.write(current_status ^ (1U << bit_dist(gen)));
            }
        }
    }

    /**
     * @brief Internal Physics Engine Loop.
     * * @note Runs on Core 0 to minimize context-switch jitter during
     * high-fidelity timing simulation.
     */
    void run_physics_loop() {
        // Technical Justification: Pinning to Core 0 ensures the physics steps
        // (100us) remain deterministic and are not preempted by general OS tasks.
        (void)pin_thread_to_core(0);

        PorthDeviceLayout* dev = m_mock_hw.view();
        uint32_t temp          = SIM_BASE_TEMP_MC;

        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<uint32_t> bit_dist(0, SIM_STATUS_MAX_BIT);

        while (m_run_sim.load(std::memory_order_relaxed)) {
            // Signal "Hardware Ready" to the Logic Layer.
            dev->status.write(0x1);
            std::atomic_thread_fence(std::memory_order_release);

            if (!m_inject_deadlock.load(std::memory_order_relaxed)) {
                process_dma(dev);
                update_thermal_model(dev, temp);
                apply_chaos_effects(dev, gen, bit_dist);
            }

            std::this_thread::sleep_for(std::chrono::microseconds(SIM_PHYSICS_STEP_US));
        }
    }

public:
    /**
     * @brief Constructor: Initializes the Digital Twin environment.
     * * @param name The SHM identifier (e.g., "cardiff_0").
     * @param create If true, allocates the mock-BARs (Simulator Mode).
     */
    PorthSimDevice(const std::string& name, bool create = true) : m_mock_hw(name, create) {

        PorthDeviceLayout* dev = m_mock_hw.view();
        dev->laser_temp.write(SIM_BASE_TEMP_MC);
        dev->status.write(0);
        dev->control.write(0);

        m_tlp_log.open("porth_tlp_traffic.log", std::ios::app);
        m_physics_thread = std::thread(&PorthSimDevice::run_physics_loop, this);
    }

    /**
     * @brief Destructor: Orchestrates a graceful "Hardware Power-Down".
     */
    ~PorthSimDevice() {
        m_run_sim.store(false, std::memory_order_relaxed);
        if (m_physics_thread.joinable()) {
            m_physics_thread.join();
        }
        if (m_tlp_log.is_open()) {
            m_tlp_log.close();
        }
    }

    /**
     * @brief apply_scenario: Configures the physics model for specific test cases.
     * @param base_ns The base propagation delay (light-in-fiber simulation).
     * @param jitter_ns Maximum jitter (clock recovery noise).
     * @param chaos Enable SEU/bit-flip simulation.
     */
    void apply_scenario(uint64_t base_ns, uint64_t jitter_ns, bool chaos = false) {
        m_phy.set_config(base_ns, jitter_ns);
        if (chaos) {
            trigger_corruption(true);
        }
    }

    /** @brief Triggers a firmware deadlock (halts physics loop). */
    void trigger_deadlock(bool active) noexcept { m_inject_deadlock.store(active); }
    /** @brief Triggers status register bit-flips. */
    void trigger_corruption(bool active) noexcept { m_corrupt_status.store(active); }
    /** @brief Simulates a PCIe bus hang (read/write timeouts). */
    void set_bus_hang(bool active) noexcept { m_bus_hang.store(active); }

    /**
     * @brief force_buffer_overflow(): Resilience test against saturated links.
     * * Floods the RingBuffer to verify that the Driver handles 'FULL'
     * conditions without crashing or leaking memory.
     * * @param ring The target DMA ring.
     */
    static void force_buffer_overflow(PorthRingBuffer<SIM_DEFAULT_SHUTTLE_SIZE>& ring) {
        const PorthDescriptor junk = {.addr = 0xDEADBEEF, .len = SIM_DESCRIPTOR_LEN_DEFAULT};
        for (int i = 0; i < SIM_OVERFLOW_ITERATIONS; ++i) {
            (void)ring.push(junk);
        }
    }

    /** * @brief MMIO read with simulated protocol latency.
     * @return T The register value after propagation delay.
     */
    template <typename T>
    [[nodiscard]] auto read_reg(PorthRegister<T>& reg) -> T {
        if (m_bus_hang.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SIM_BUS_HANG_MS));
        }
        m_phy.apply_protocol_delay();
        return reg.load();
    }

    /** * @brief MMIO write with simulated protocol latency.
     * @param val The value to write to the physical register.
     */
    template <typename T>
    auto write_reg(PorthRegister<T>& reg, T val) -> void {
        if (m_bus_hang.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SIM_BUS_HANG_MS));
        }
        m_phy.apply_protocol_delay();
        reg.write(val);
    }

    /** * @brief Protocol-aware read simulating a PCIe Gen 6 FLIT completion.
     * * Logs the request/completion to porth_tlp_traffic.log for logic auditing.
     */
    template <typename T>
    [[nodiscard]] auto read_flit(PorthRegister<T>& reg, uint64_t offset) -> T {
        if (m_bus_hang.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SIM_BUS_HANG_MS));
        }
        log_tlp("READ_REQ", offset);
        m_phy.apply_protocol_delay();
        const T val = reg.load();
        log_tlp("COMPLETION", offset, static_cast<uint64_t>(val));
        return val;
    }

    /** * @brief Protocol-aware write simulating a PCIe Gen 6 Memory Write TLP.
     */
    template <typename T>
    auto write_flit(PorthRegister<T>& reg, uint64_t offset, T val) -> void {
        if (m_bus_hang.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SIM_BUS_HANG_MS));
        }
        log_tlp("WRITE_MEM", offset, static_cast<uint64_t>(val));
        m_phy.apply_protocol_delay();
        reg.write(val);
    }

    /** @brief Returns the shared register map view. */
    [[nodiscard]] auto view() noexcept -> PorthDeviceLayout* { return m_mock_hw.view(); }

    /** @brief Returns a reference to the PHY physics configuration. */
    [[nodiscard]] auto get_phy() noexcept -> PorthSimPHY& { return m_phy; }

private:
    /**
     * @brief log_tlp: Internal logger for Transaction Layer Packets.
     * * Critical for identifying protocol-level race conditions in the Newport Cluster.
     */
    void log_tlp(const std::string& type, uint64_t addr, uint64_t val = 0) {
        if (m_tlp_log.is_open()) {
            const auto now = std::chrono::system_clock::now();
            const auto t_c = std::chrono::system_clock::to_time_t(now);

            m_tlp_log << "[" << std::put_time(std::localtime(&t_c), "%H:%M:%S") << "] ";
            m_tlp_log << "TLP_" << type << " | Addr: 0x" << std::hex << addr;
            m_tlp_log << " | Data: 0x" << val << std::dec << '\n';
        }
    }
};

} // namespace porth