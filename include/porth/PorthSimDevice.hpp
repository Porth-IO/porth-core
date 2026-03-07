/**
 * @file PorthSimDevice.hpp
 * @brief High-fidelity Digital Twin for Cardiff Photonics.
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

/** @brief Simulation Constants to resolve magic number warnings. */
constexpr uint32_t SIM_BASE_TEMP_MC           = 25000;
constexpr uint32_t SIM_TEMP_INC_MC            = 100;
constexpr uint32_t SIM_TEMP_DEC_MC            = 50;
constexpr uint32_t SIM_STATUS_MAX_BIT         = 31;
constexpr size_t SIM_DEFAULT_SHUTTLE_SIZE     = 1024;
constexpr int SIM_PHYSICS_STEP_US             = 100;
constexpr int SIM_BUS_HANG_MS                 = 100;
constexpr int SIM_OVERFLOW_ITERATIONS         = 2048;
constexpr uint32_t SIM_DESCRIPTOR_LEN_DEFAULT = 64;

/**
 * @class PorthSimDevice
 * @brief The high-fidelity Digital Twin for Cardiff Photonics.
 *
 * This class simulates the physical, electrical, and protocol-level behaviors
 * of a compound semiconductor device on a PCIe Gen 6 bus. It manages a background
 * physics thread that models thermal drift and power rail noise while providing
 * hooks for chaos engineering and protocol-aware register access.
 */
class PorthSimDevice {
public:
    // Move deleted members to public to satisfy modernize-use-equals-delete
    // Simulation orchestrators own unique thread resources and cannot be copied.
    PorthSimDevice(const PorthSimDevice&)                    = delete;
    auto operator=(const PorthSimDevice&) -> PorthSimDevice& = delete;

    // Rule of Five: Explicitly delete move operations.
    PorthSimDevice(PorthSimDevice&&)                    = delete;
    auto operator=(PorthSimDevice&&) -> PorthSimDevice& = delete;

private:
    PorthMockDevice m_mock_hw; ///< Shared memory register backend.
    PorthSimPHY m_phy;         ///< Physical layer propagation and jitter model.
    std::ofstream m_tlp_log;   ///< Persistent log for transaction layer packets.

    // Physics & Simulation Threads
    std::thread m_physics_thread;      ///< Dedicated thread for the hardware physics model.
    std::atomic<bool> m_run_sim{true}; ///< Lifecycle flag for the simulation loop.

    // --- Chaos & Error Injection State ---
    std::atomic<bool> m_inject_deadlock{false}; ///< Artificially halts the physics loop.
    std::atomic<bool> m_corrupt_status{false};  ///< Triggers random bit-flips in status registers.
    std::atomic<bool> m_bus_hang{false};        ///< Simulates PCIe bus timeouts.

    /** @brief Internal helper to drain the DMA Shuttle. */
    void process_dma(PorthDeviceLayout* dev) noexcept {
        const uint64_t shuttle_addr = dev->data_ptr.load();
        if (shuttle_addr == 0)
            return;

        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        auto* shuttle = reinterpret_cast<PorthShuttle<SIM_DEFAULT_SHUTTLE_SIZE>*>(shuttle_addr);
        PorthDescriptor desc{};

        // Drain the shuttle as fast as the "hardware" can process it
        while (shuttle->ring()->pop(desc)) {
            // Logic for firing photons would go here
        }
    }

    /** @brief Internal helper to update the Photonics thermal lattice. */
    void update_thermal_model(PorthDeviceLayout* dev, uint32_t& current_temp) noexcept {
        // Heating logic: Increases temp when active (control == 0x1)
        if (dev->control.load() == 0x1) {
            current_temp += SIM_TEMP_INC_MC;
        } else if (current_temp > SIM_BASE_TEMP_MC) {
            // Cooling logic: Decreases temp until base reached
            current_temp -= SIM_TEMP_DEC_MC;
        }

        dev->laser_temp.write(current_temp);
        m_phy.update_thermal_load(current_temp);
    }

    /** @brief Internal helper to simulate Single-Event Upsets (Chaos). */
    void apply_chaos_effects(PorthDeviceLayout* dev,
                             std::mt19937& gen,
                             std::uniform_int_distribution<uint32_t>& bit_dist) noexcept {
        if (m_corrupt_status.load(std::memory_order_relaxed)) {
            const uint32_t current_status = dev->status.load();
            dev->status.write(current_status ^ (1U << bit_dist(gen)));
        }
    }

    /**
     * @brief Internal Physics Engine: Simulates thermal and electrical behavior.
     * High-level orchestrator for the Digital Twin physics model.
     */
    void run_physics_loop() {
        (void)pin_thread_to_core(0);

        PorthDeviceLayout* dev = m_mock_hw.view();
        uint32_t temp          = SIM_BASE_TEMP_MC;

        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<uint32_t> bit_dist(0, SIM_STATUS_MAX_BIT);

        while (m_run_sim.load(std::memory_order_relaxed)) {
            // Hardware Ready Signal
            dev->status.write(0x1);
            std::atomic_thread_fence(std::memory_order_release);

            // Execute physics steps if not deadlocked
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
     * @brief Constructor: Initializes shared memory and the physics background thread.
     * @param name Name of the device for shared memory allocation.
     * @param create If true, creates the segment; if false, attaches as observer.
     */
    PorthSimDevice(const std::string& name, bool create = true) : m_mock_hw(name, create) {

        // --- PRE-INITIALIZATION ---
        // Ensure registers have valid defaults before the physics thread wakes up.
        PorthDeviceLayout* dev = m_mock_hw.view();
        dev->laser_temp.write(SIM_BASE_TEMP_MC);
        dev->status.write(0);
        dev->control.write(0);

        m_tlp_log.open("porth_tlp_traffic.log", std::ios::app);
        m_physics_thread = std::thread(&PorthSimDevice::run_physics_loop, this);
    }

    /**
     * @brief Destructor: Safely shuts down simulation threads and closes logs.
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

    // --- Task 5: Orchestrator & Scenario Control ---

    /**
     * @brief Configures a specific performance or failure scenario.
     * @param base_ns The base propagation delay.
     * @param jitter_ns The maximum random jitter.
     * @param chaos If true, enables status register corruption.
     */
    void apply_scenario(uint64_t base_ns, uint64_t jitter_ns, bool chaos = false) {
        m_phy.set_config(base_ns, jitter_ns);
        if (chaos) {
            trigger_corruption(true);
        }
    }

    // --- Task 4: Chaos Control Interface ---
    void trigger_deadlock(bool active) noexcept { m_inject_deadlock.store(active); }
    void trigger_corruption(bool active) noexcept { m_corrupt_status.store(active); }
    void set_bus_hang(bool active) noexcept { m_bus_hang.store(active); }

    /**
     * @brief Sub-task 4.3: Buffer Overflow Testing.
     * * Artificially saturates the RingBuffer to test driver resilience.
     * Pushes 2048 junk descriptors into the 1024-slot ring.
     * * @param ring Reference to the ring buffer to overflow.
     */
    static void force_buffer_overflow(PorthRingBuffer<SIM_DEFAULT_SHUTTLE_SIZE>& ring) {
        const PorthDescriptor junk = {.addr = 0xDEADBEEF, .len = SIM_DESCRIPTOR_LEN_DEFAULT};
        for (int i = 0; i < SIM_OVERFLOW_ITERATIONS; ++i) {
            // Explicitly ignore nodiscard return
            (void)ring.push(junk);
        }
    }

    // --- Tasks 1 & 2: Register Access (MMIO & FLIT Simulation) ---

    /** @brief MMIO read with simulated bus hang and protocol latency. */
    template <typename T>
    [[nodiscard]] auto read_reg(PorthRegister<T>& reg) -> T {
        if (m_bus_hang.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SIM_BUS_HANG_MS));
        }
        m_phy.apply_protocol_delay();
        return reg.load();
    }

    /** @brief MMIO write with simulated bus hang and protocol latency. */
    template <typename T>
    auto write_reg(PorthRegister<T>& reg, T val) -> void {
        if (m_bus_hang.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SIM_BUS_HANG_MS));
        }
        m_phy.apply_protocol_delay();
        reg.write(val);
    }

    /** @brief Protocol-aware read simulating a PCIe Gen 6 FLIT completion with logging. */
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

    /** @brief Protocol-aware write simulating a PCIe Gen 6 Memory Write TLP with logging. */
    template <typename T>
    auto write_flit(PorthRegister<T>& reg, uint64_t offset, T val) -> void {
        if (m_bus_hang.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SIM_BUS_HANG_MS));
        }
        log_tlp("WRITE_MEM", offset, static_cast<uint64_t>(val));
        m_phy.apply_protocol_delay();
        reg.write(val);
    }

    /** @brief Access the register layout for handshakes. */
    [[nodiscard]] auto view() noexcept -> PorthDeviceLayout* { return m_mock_hw.view(); }

    /** @brief Access the PHY simulator configuration. */
    [[nodiscard]] auto get_phy() noexcept -> PorthSimPHY& { return m_phy; }

private:
    /**
     * @brief log_tlp: Internal logger for Transaction Layer Packets.
     * * Formats and writes hardware-level activity to porth_tlp_traffic.log.
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