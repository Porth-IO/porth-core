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

#include <fstream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <atomic>
#include <random>
#include <format>
#include <string>

#include "PorthMockDevice.hpp"
#include "PorthSimPHY.hpp"
#include "PorthRingBuffer.hpp"
#include "PorthUtil.hpp"
#include "PorthShuttle.hpp"

namespace porth {

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
private:
    PorthMockDevice mock_hw;            ///< Shared memory register backend.
    PorthSimPHY phy;                    ///< Physical layer propagation and jitter model.
    std::ofstream tlp_log;              ///< Persistent log for transaction layer packets.
    
    // Physics & Simulation Threads
    std::thread physics_thread;         ///< Dedicated thread for the hardware physics model.
    std::atomic<bool> run_sim{true};    ///< Lifecycle flag for the simulation loop.

    // --- Chaos & Error Injection State ---
    std::atomic<bool> inject_deadlock{false}; ///< Artificially halts the physics loop.
    std::atomic<bool> corrupt_status{false};  ///< Triggers random bit-flips in status registers.
    std::atomic<bool> bus_hang{false};        ///< Simulates PCIe bus timeouts.

    /**
     * @brief Internal Physics Engine: Simulates thermal and electrical behavior.
     * * This loop runs at 100Hz (10ms steps) to model the real-time physics of 
     * InP/GaN hardware, including thermal fluctuations based on operation load 
     * and 48V power rail noise.
     */
    void run_physics_loop() {
        /**
         * Porth-IO Isolation Protocol:
         * We pin the hardware simulator to Core 0 to prevent starvation
         * from the Real-Time Driver running on Core 1.
         */
        (void)pin_thread_to_core(0);

        PorthDeviceLayout* dev = mock_hw.view();
        
        uint32_t temp = 25000; // Base operating temperature of 25.0 C (milli-Celsius)
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<uint32_t> bit_dist(0, 31);

        while (run_sim.load(std::memory_order_relaxed)) {
            // Signal to the driver that hardware logic is active and ready
            dev->status.write(0x1);
            // Use release semantics to ensure the write is visible to other cores
            std::atomic_thread_fence(std::memory_order_release);

            if (!inject_deadlock.load(std::memory_order_relaxed)) {

                /**
                 * DMA CONSUMER ENGINE
                 * Drains the Shuttle ring buffer to simulate hardware processing.
                 */
                uint64_t shuttle_addr = dev->data_ptr.load();
                if (shuttle_addr != 0) {
                    auto* shuttle = reinterpret_cast<PorthShuttle<1024>*>(shuttle_addr);
                    PorthDescriptor desc;
                    // Drain the shuttle as fast as the "hardware" can process it
                    while (shuttle->ring()->pop(desc)) {
                        // In a real device, this is where photons are fired.
                    }
                }

                // Task 3: Photonics/GaN Model
                // Heating logic: Increases temp by 0.1C when active (control == 0x1)
                if (dev->control.load() == 0x1) {
                    temp += 100; 
                } else {
                    // Cooling logic: Decreases temp by 0.05C until base temp reached
                    if (temp > 25000) temp -= 50; 
                }
                
                dev->laser_temp.write(temp);
                
                // Task 4.3: Thermal Jitter Feedback
                // Pushes current temperature back into the PHY delay engine to calculate jitter
                phy.update_thermal_load(temp);

                // GaN Power Rail Noise (Simulating 48V base + millivolt-scale jitter)
                [[maybe_unused]] const uint32_t noise = (std::rand() % 100); 
                // Note: gan_voltage is modeled in the extended register map
                // dev->gan_voltage.write(48000 + noise); 

                // Sub-task 4.1: Register Corruption (Single-event upset emulation)
                if (corrupt_status.load(std::memory_order_relaxed)) {
                    uint32_t current_status = dev->status.load();
                    dev->status.write(current_status ^ (1 << bit_dist(gen)));
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

public:
    /**
     * @brief Constructor: Initializes shared memory and the physics background thread.
     * @param name Name of the device for shared memory allocation.
     * @param create If true, creates the segment; if false, attaches as observer.
     */
    PorthSimDevice(const std::string& name, bool create = true) 
        : mock_hw(name, create) {
        
        // --- PRE-INITIALIZATION ---
        // Ensure registers have valid defaults before the physics thread wakes up.
        PorthDeviceLayout* dev = mock_hw.view();
        dev->laser_temp.write(25000); 
        dev->status.write(0);
        dev->control.write(0);

        tlp_log.open("porth_tlp_traffic.log", std::ios::app);
        physics_thread = std::thread(&PorthSimDevice::run_physics_loop, this);
    }

    /**
     * @brief Destructor: Safely shuts down simulation threads and closes logs.
     */
    ~PorthSimDevice() {
        run_sim.store(false, std::memory_order_relaxed);
        if (physics_thread.joinable()) {
            physics_thread.join();
        }
        if (tlp_log.is_open()) {
            tlp_log.close();
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
        phy.set_config(base_ns, jitter_ns);
        if (chaos) trigger_corruption(true);
    }

    // --- Task 4: Chaos Control Interface ---
    void trigger_deadlock(bool active) noexcept { inject_deadlock.store(active); }
    void trigger_corruption(bool active) noexcept { corrupt_status.store(active); }
    void set_bus_hang(bool active) noexcept { bus_hang.store(active); }

    /**
     * @brief Sub-task 4.3: Buffer Overflow Testing.
     * * Artificially saturates the RingBuffer to test driver resilience. 
     * Pushes 2048 junk descriptors into the 1024-slot ring.
     * * @param ring Reference to the ring buffer to overflow.
     */
    void force_buffer_overflow(PorthRingBuffer<1024>& ring) {
        const PorthDescriptor junk = {0xDEADBEEF, 64};
        for(int i = 0; i < 2048; ++i) {
            // Explicitly ignore nodiscard return
            (void)ring.push(junk); 
        }
    }

    // --- Tasks 1 & 2: Register Access (MMIO & FLIT Simulation) ---

    /** @brief MMIO read with simulated bus hang and protocol latency. */
    template <typename T>
    [[nodiscard]] T read_reg(PorthRegister<T>& reg) {
        if (bus_hang.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        phy.apply_protocol_delay();
        return reg.load();
    }

    /** @brief MMIO write with simulated bus hang and protocol latency. */
    template <typename T>
    void write_reg(PorthRegister<T>& reg, T val) {
        if (bus_hang.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        phy.apply_protocol_delay();
        reg.write(val);
    }

    /** @brief Protocol-aware read simulating a PCIe Gen 6 FLIT completion with logging. */
    template <typename T>
    [[nodiscard]] T read_flit(PorthRegister<T>& reg, uint64_t offset) {
        if (bus_hang.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        log_tlp("READ_REQ", offset);
        phy.apply_protocol_delay();
        const T val = reg.load();
        log_tlp("COMPLETION", offset, static_cast<uint64_t>(val));
        return val;
    }

    /** @brief Protocol-aware write simulating a PCIe Gen 6 Memory Write TLP with logging. */
    template <typename T>
    void write_flit(PorthRegister<T>& reg, uint64_t offset, T val) {
        if (bus_hang.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        log_tlp("WRITE_MEM", offset, static_cast<uint64_t>(val));
        phy.apply_protocol_delay();
        reg.write(val);
    }

    /** @brief Access the register layout for handshakes. */
    [[nodiscard]] PorthDeviceLayout* view() noexcept { return mock_hw.view(); }

    /** @brief Access the PHY simulator configuration. */
    [[nodiscard]] PorthSimPHY& get_phy() noexcept { return phy; }

private:
    /**
     * @brief log_tlp: Internal logger for Transaction Layer Packets.
     * * Formats and writes hardware-level activity to porth_tlp_traffic.log.
     */
    void log_tlp(const std::string& type, uint64_t addr, uint64_t val = 0) {
        if (tlp_log.is_open()) {
            const auto now = std::chrono::system_clock::now();
            const auto t_c = std::chrono::system_clock::to_time_t(now);
            
            // Strictly preserving your hex-formatted logging logic
            tlp_log << "[" << std::put_time(std::localtime(&t_c), "%H:%M:%S") << "] "
                    << "TLP_" << type << " | Addr: 0x" << std::hex << addr 
                    << " | Data: 0x" << val << std::dec << std::endl;
        }
    }

    // Simulation orchestrators own unique thread resources and cannot be copied.
    PorthSimDevice(const PorthSimDevice&) = delete;
    PorthSimDevice& operator=(const PorthSimDevice&) = delete;
};

} // namespace porth