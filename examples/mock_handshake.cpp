/**
 * @file hardware_demo_main.cpp
 * @brief Demonstration of the Logic-to-Physical handshake between Driver and Newport ASIC.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthDeviceLayout.hpp"
#include "porth/PorthMockDevice.hpp"
#include "porth/PorthRegister.hpp"
#include <bits/chrono.h>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

/**
 * @namespace cardiff_chip
 * @brief Hardware-level constants defined by the Newport Cluster Physical Design Kit (PDK).
 *
 * These constants represent the command and status bit-masks for the Cardiff Indium
 * Phosphide (InP) controller logic.
 */
namespace cardiff_chip {
constexpr uint32_t CTRL_IDLE  = 0x0; ///< Reset state: ASIC logic is powered but quiescent.
constexpr uint32_t CTRL_START = 0x1; ///< Trigger: Initiates the photonics processing sequence.
constexpr uint32_t CTRL_RESET = 0x9; ///< Shutdown: Commands the logic layer to unmap.

constexpr uint32_t STATUS_BUSY  = 0x1; ///< Signal: GaN power stage is active/laser is firing.
constexpr uint32_t STATUS_READY = 0x2; ///< Signal: Result is stable in the counter register.

// Simulation Constraints
constexpr auto INIT_DELAY_MS     = std::chrono::milliseconds(100);
constexpr auto SIM_WORK_DELAY_MS = std::chrono::milliseconds(500);
} // namespace cardiff_chip

using namespace porth;

/**
 * @brief The "Hardware" Simulation Thread.
 * * Simulates the Newport Cluster ASIC logic. This thread owns the "Device Side" of
 * the MMIO register map, polling the control register and updating status/telemetry.
 * * @param dev_name The SHM identifier used to attach to the mock BARs.
 */
void run_mock_hardware(const std::string& dev_name) {
    std::cout << "[Hardware] Powering on Cardiff Mock Chip..." << '\n';

    // Justification: We wait for the Driver process to establish the SHM segment (Owner).
    // In a physical system, this delay represents the PCIe link training phase.
    std::this_thread::sleep_for(cardiff_chip::INIT_DELAY_MS);

    try {
        // Attach as Observer (create = false). The ASIC follows the Driver's memory map.
        PorthMockDevice hw(dev_name, false);
        auto* chip = hw.view();

        std::cout << "[Hardware] Chip is online and Polling..." << '\n';

        bool running = true;
        while (running) {
            // Hot-Polling the control register. This mimics the FPGA-level logic gates
            // watching for a specific bit-toggle from the CPU.
            uint32_t ctrl = chip->control.load();

            if (ctrl == cardiff_chip::CTRL_START) {
                std::cout << "[Hardware] START received! Processing photonics data..." << '\n';

                // Hardware Logic: Set status to BUSY to lock the register interface.
                chip->status.write(cardiff_chip::STATUS_BUSY);

                // Physics Modeling: Simulates the InP laser stabilization and lattice
                // settling time required before a coherent result can be sampled.
                std::this_thread::sleep_for(cardiff_chip::SIM_WORK_DELAY_MS);

                // Telemetry Update: Increment the monotonic throughput counter.
                uint64_t current_count = chip->counter.load();
                chip->counter.write(current_count + 1);

                std::cout << "[Hardware] Task complete. Result committed to counter." << '\n';

                // Completion Signal: Visibility of the result is guaranteed by the
                // memory_order_release in the 'status.write' call.
                chip->status.write(cardiff_chip::STATUS_READY);

                // Reset internal command latch to IDLE.
                chip->control.write(cardiff_chip::CTRL_IDLE);

            } else if (ctrl == cardiff_chip::CTRL_RESET) {
                std::cout << "[Hardware] RESET signal received. Powering down ASIC logic." << '\n';
                running = false;
            }

            // Performance Guard: PAUSE prevents the SMT (Simultaneous Multi-Threading)
            // unit from being starved by this high-frequency spin-loop.
#if defined(__i386__) || defined(__x86_64__)
            asm volatile("pause" ::: "memory");
#endif
        }
    } catch (const std::exception& e) {
        std::cerr << "[Hardware] Fatal ASIC Simulation Error: " << e.what() << '\n';
    }
}

/**
 * @brief Driver Logic Entry Point.
 * * This acts as the Sovereign Logic Layer application, creating the hardware
 * context and orchestrating the high-speed polling handshake.
 */
auto main() -> int {
    const std::string device_name = "porth_vdev_0";

    std::cout << "[Driver] Initializing Porth-IO HAL..." << '\n';

    try {
        // 1. Create the Virtual Hardware Device (Owner).
        // This simulates the OS allocating a region of physical memory for the PCIe BAR.
        PorthMockDevice driver(device_name, true);
        auto* dev = driver.view();

        // 2. Start the Hardware Simulator thread to act as the Cardiff chip.
        std::thread hw_thread(run_mock_hardware, device_name);

        // 3. Trigger a Hardware Operation.
        // We write directly to the command register; the hardware ASIC thread is
        // already hot-polling this memory location.
        std::cout << "[Driver] Requesting Cardiff Chip to process packet..." << '\n';
        dev->control.write(cardiff_chip::CTRL_START);

        // 4. Zero-Latency Polling.
        // Justification: We avoid sleep() or interrupts here. In ultra-low-latency
        // HFT environments, we burn CPU cycles to guarantee we detect the 'READY'
        // signal within ~3 nanoseconds of its occurrence.
        std::cout << "[Driver] Polling status register (Spin-Mode)..." << '\n';
        while (dev->status.load() != cardiff_chip::STATUS_READY) {
#if defined(__i386__) || defined(__x86_64__)
            asm volatile("pause" ::: "memory");
#endif
        }

        // 5. Telemetry Retrieval.
        // Once the hardware signals 'READY', we sample the result.
        uint64_t result = dev->counter.load();
        std::cout << "[Driver] SUCCESS! Chip processed data. New Counter: " << result << '\n';

        // 6. Cleanup.
        // Signal the simulator to power down and unlink the shared memory segment.
        std::cout << "[Driver] Sending SHUTDOWN signal..." << '\n';
        dev->control.write(cardiff_chip::CTRL_RESET);

        if (hw_thread.joinable()) {
            hw_thread.join();
        }
        std::cout << "[Driver] All systems offline. SHM segment unlinked." << '\n';

    } catch (const std::exception& e) {
        std::cerr << "[Driver] Fatal Logic Layer Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}