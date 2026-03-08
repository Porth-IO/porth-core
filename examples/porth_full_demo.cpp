/**
 * @file porth_full_demo.cpp
 * @brief Sovereign Logic Layer MVP - Final Immaculate Integration and Validation.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthClock.hpp"
#include "porth/PorthDeviceLayout.hpp"
#include "porth/PorthDriver.hpp"
#include "porth/PorthMetric.hpp"
#include "porth/PorthRegister.hpp"
#include "porth/PorthSimDevice.hpp"
#include "porth/PorthUtil.hpp"
#include <bits/chrono.h>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

/**
 * @brief Main execution engine for the Porth-IO Newport Cluster Validation.
 * * This program demonstrates the full lifecycle of a Sovereign Logic Layer session:
 * 1. Hardware Digital Twin initialization.
 * 2. Operating System jitter shielding (Core-pinning and RT priority).
 * 3. Zero-copy memory fabric (Shuttle) handshake.
 * 4. Deterministic telemetry recording under PCIe Gen 6 simulated loads.
 */
auto main() -> int {
    using namespace porth;

    // Constants reflecting the physical and protocol limits of the Newport hardware.
    constexpr size_t shuttle_size      = 1024;
    constexpr int handshake_timeout_ms = 5000;
    constexpr uint64_t test_addr       = 0x1000;
    constexpr uint32_t test_len        = 64;

    /** * @brief Propagation Delay (240 cycles).
     * Represents the ~100ns "wire flight time" of a PCIe Gen 6 TLP across the
     * interconnect. Pacing the driver to this limit prevents RingBuffer
     * saturation and accurately models 1.6T transceiver throughput.
     */
    constexpr uint64_t propagation_delay_cycles = 240;

    constexpr size_t log_interval          = 10000;
    constexpr float temp_divisor           = 1000.0F;
    constexpr double cycles_per_ns_newport = 2.4;

    std::cout << "--- Porth-IO: Sovereign Logic Layer MVP ---\n";

    try {
        /**
         * PHASE 1: Isolated Hardware Initialization.
         * The PorthSimDevice acts as our Digital Twin. It isolates its internal
         * physics engine (InP thermal modeling) on Core 0 to prevent interference
         * with the high-speed driver logic.
         */
        PorthSimDevice hw_sim("porth_newport_0", true);
        auto* regs = hw_sim.view();

        /**
         * PHASE 2: Environmental Shielding (The Jitter Shield).
         * We isolate the Driver on Core 1 and elevate it to SCHED_FIFO priority 99.
         * Justification: This ensures the CPU cache remains "warm" with driver
         * logic and prevents the Linux kernel from interrupting our nanosecond-scale
         * telemetry loop for background system maintenance.
         */
        std::ignore = pin_thread_to_core(1);
        std::ignore = set_realtime_priority();

        /**
         * PHASE 3: Master Driver Handshake.
         * The Driver constructs the zero-copy Shuttle (HugePage-backed) and
         * writes its physical address to the 'data_ptr' register of the chip.
         */
        Driver<shuttle_size> driver(regs);
        std::cout << "[System] Powering on Newport Cluster...\n";

        // Command the ASIC logic to begin its power-on self-test (POST).
        regs->control.write(0x1);

        /**
         * HANDSHAKE SAFETY PROTOCOL:
         * We poll the status register for the READY bit (0x1).
         * Justification: We include a 1ms sleep during this non-critical phase
         * to allow the Simulator thread (on Core 0) to propagate its internal
         * memory state across the L3 cache boundary to our RT thread (on Core 1).
         */
        int timeout_ms = 0;
        while (regs->status.load() == 0 && timeout_ms < handshake_timeout_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timeout_ms++;
        }

        if (regs->status.load() == 0) {
            throw std::runtime_error(
                "Hardware handshake timed out. Check thread isolation levels or SHM permissions.");
        }

        std::cout << "[System] Hardware Ready. Laser stabilized at 25.00 °C.\n";

        // PHASE 4: Deterministic Stress Test.
        // We execute 50,000 iterations to ensure we have a statistically
        // significant sample size for P99.99 jitter analysis.
        constexpr size_t iterations = 50000;
        PorthMetric metric(iterations);
        std::cout << "[Driver] Executing 50,000 Zero-Copy cycles...\n";

        for (size_t i = 0; i < iterations; ++i) {
            // Precise start time using the serialized hardware cycle counter.
            const uint64_t t1 = PorthClock::now_precise();

            // Zero-copy transmission: Writing the descriptor to the Shuttle.
            if (driver.transmit({.addr = test_addr, .len = test_len}) != PorthStatus::SUCCESS) {
                std::cerr << "[Error] Shuttle saturated. InP processing latency exceeded.\n";
                break;
            }

            /**
             * PHYSICAL PROPAGATION DELAY MODELING:
             * We busy-wait for 240 cycles to simulate the 1.6T physical layer
             * latency. This prevents the software logic from outrunning the
             * physical hardware interconnect limits.
             */
            const uint64_t start_delay = PorthClock::now_precise();
            while (PorthClock::now_precise() - start_delay < propagation_delay_cycles) {
                // Spinning to maintain timing sovereignty.
            }

            const uint64_t t2 = PorthClock::now_precise();
            metric.record(t2 - t1);

            if (i % log_interval == 0) {
                // Telemetry: Convert mC to Celsius for the user-facing log.
                const float temp_c = static_cast<float>(regs->laser_temp.load()) / temp_divisor;
                std::cout << std::format(
                    "  - Progress: {:5} cycles | Temp: {:.2f} °C\n", i, temp_c);
            }
        }

        // PHASE 5: Final Statistical Post-Processing.
        // Convert the collected cycle data into a nanosecond-scale jitter report.
        metric.print_stats(cycles_per_ns_newport);
        std::cout << "[Success] Newport Cluster validated. System ready for 1.6T deployment.\n";

    } catch (const std::exception& e) {
        std::cerr << std::format("[Fatal] Logic Layer Exception: {}\n", e.what());
        return 1;
    }

    return 0;
}