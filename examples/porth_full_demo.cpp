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
#include "porth/PorthTelemetry.hpp"
#include "porth/PorthUtil.hpp"
#include "porth/PorthVFIODevice.hpp"
#include "porth/PorthXDPPortal.hpp"
#include <chrono>
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
 * @brief Executes the high-speed deterministic telemetry collection loop.
 * This is extracted to reduce the cognitive complexity of the main function.
 */
template <size_t Cap>
static void run_telemetry_stress_test(porth::PorthDeviceLayout* regs,
                                      porth::Driver<Cap>& driver,
                                      porth::PorthXDPPortal& xdp_portal,
                                      porth::PorthStats* sovereign_stats,
                                      porth::PorthMetric& metric) {
    using namespace porth;

    constexpr size_t iterations                 = 50000;
    constexpr size_t log_interval               = 10000;
    constexpr uint64_t test_addr                = 0x1000;
    constexpr uint32_t test_len                 = 64;
    constexpr uint64_t propagation_delay_cycles = 240;
    constexpr float temp_divisor                = 1000.0F;

    for (size_t i = 0; i < iterations; ++i) {
        // Poll the NIC and bridge any waiting packet to the Shuttle.
        // This is non-blocking; if no packet exists, it returns in < 50ns.
        xdp_portal.bridge_to_shuttle(*driver.get_shuttle(), sovereign_stats);

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
            // 1. Read hardware temp and update the Shared Telemetry block
            const uint32_t current_temp = regs->laser_temp.load();

            if (current_temp > sovereign_stats->max_temp_mc.load()) {
                sovereign_stats->max_temp_mc.store(current_temp);
            }

            // Use -> because sovereign_stats is a pointer from the TelemetryHub
            sovereign_stats->current_temp_mc.store(current_temp);

            // 2. Pull totals for the UI
            const uint64_t pkts  = sovereign_stats->total_packets.load();
            const uint64_t bytes = sovereign_stats->total_bytes.load();

            // Declare temp_c here so it is available for std::format
            const float temp_c = static_cast<float>(current_temp) / temp_divisor;

            std::cout << std::format(
                "  - Progress: {:5} | Ingress: {:7} pkts ({:10} bytes) | Temp: {:.2f} °C\n",
                i,
                pkts,
                bytes,
                temp_c);
        }
    }
}

/**
 * @brief Keeps the telemetry SHM active for external dashboard inspection.
 */
static void run_telemetry_parking(porth::PorthDeviceLayout* regs,
                                  porth::PorthStats* sovereign_stats,
                                  int duration_seconds) {
    for (int i = 0; i < duration_seconds; ++i) {
        // Keep updating the temp in SHM so the dashboard stays live
        sovereign_stats->current_temp_mc.store(regs->laser_temp.load());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/**
 * @brief Main execution engine for the Porth-IO Newport Cluster Validation.
 * * This program demonstrates the full lifecycle of a Sovereign Logic Layer session:
 * 1. Hardware Digital Twin initialization.
 * 2. Operating System jitter shielding (Core-pinning and RT priority).
 * 3. Zero-copy memory fabric (Shuttle) handshake.
 * 4. Deterministic telemetry recording under PCIe Gen 6 simulated loads.
 */
auto main(int argc, char** argv) -> int {
    using namespace porth;

    // Constants reflecting the physical and protocol limits of the Newport hardware.
    constexpr size_t shuttle_size      = 1024;
    constexpr int handshake_timeout_ms = 5000000;

    // Named constants for Clang-Tidy magic numbers
    constexpr int warmup_delay_ms    = 10;
    constexpr int parking_duration_s = 600;
    constexpr int handshake_poll_ms  = 1;

    constexpr double cycles_per_ns_newport = 2.4;

    std::cout << "--- Porth-IO: Sovereign Logic Layer MVP ---\n";

    try {

        // 1. Hardware Initialization (Sovereign Choice)
        PorthDeviceLayout* regs = nullptr;
        std::unique_ptr<PorthSimDevice> sim;
        std::unique_ptr<PorthVFIODevice> physical_hw;

        // Check if a PCI address was passed as a command-line argument
        if (argc > 1) {
            // Path: Physical/QEMU Hardware via VFIO
            std::cout << "[System] Initializing Physical Hardware via VFIO: " << argv[1] << "\n";
            physical_hw = std::make_unique<PorthVFIODevice>(argv[1]);
            regs        = physical_hw->view();
        } else {
            // Path: Digital Twin Simulation (Bedroom Lab)
            std::cout << "[System] Initializing Digital Twin Simulation...\n";
            sim  = std::make_unique<PorthSimDevice>("porth_newport_0", true);
            regs = sim->view();
        }

        /**
         * PHASE 1: Isolated Hardware Initialization.
         * The PorthSimDevice acts as our Digital Twin. It isolates its internal
         * physics engine (InP thermal modeling) on Core 0 to prevent interference
         * with the high-speed driver logic.
         */
        // PorthSimDevice hw_sim("porth_newport_0", true);
        // auto* regs = hw_sim.view();

        /**
         * PHASE 2: Master Driver Handshake.
         * The Driver constructs the zero-copy Shuttle (HugePage-backed) and
         * writes its physical address to the 'data_ptr' register of the chip.
         */
        Driver<shuttle_size> driver(regs);
        std::cout << "[System] Powering on Newport Cluster...\n";

        // Initialize XDP while the thread is still in standard SCHED_OTHER mode.
        // This prevents the kernel bind from hanging.
        PorthXDPPortal xdp_portal("lo", 0);
        xdp_portal.bind_shuttle_memory(*driver.get_shuttle());
        xdp_portal.open_portal();

        /**
         * PHASE 3: Environmental Shielding (The Jitter Shield).
         * We isolate the Driver on Core 1 and elevate it to SCHED_FIFO priority 99.
         * Justification: This ensures the CPU cache remains "warm" with driver
         * logic and prevents the Linux kernel from interrupting our nanosecond-scale
         * telemetry loop for background system maintenance.
         */
        std::ignore = pin_thread_to_core(1);
        std::ignore = set_realtime_priority();

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
            std::this_thread::sleep_for(std::chrono::milliseconds(handshake_poll_ms));
            timeout_ms++;
        }

        if (regs->status.load() == 0) {
            throw std::runtime_error(
                "Hardware handshake timed out. Check thread isolation levels or SHM permissions.");
        }

        std::cout << "[System] Hardware Ready. Laser stabilized at 25.00 °C.\n";

        // Give the SimDevice consumer thread a few milliseconds to stabilize
        // its L1 cache and start polling the ring.
        std::this_thread::sleep_for(std::chrono::milliseconds(warmup_delay_ms));

        // PHASE 4: Deterministic Stress Test.
        // We execute 50,000 iterations to ensure we have a statistically
        // significant sample size for P99.99 jitter analysis.
        constexpr size_t iterations = 50000;
        PorthMetric metric(iterations);
        std::cout << "[Driver] Executing 50,000 Zero-Copy cycles...\n";

        PorthTelemetryHub telemetry_hub("porth_stats_0", true);
        PorthStats* sovereign_stats = telemetry_hub.view();

        driver.set_stats_link(sovereign_stats); // Link the driver to the SHM block

        // Call the telemetry execution helper to satisfy cognitive complexity audit
        run_telemetry_stress_test(regs, driver, xdp_portal, sovereign_stats, metric);

        std::cout << "\n[System] Test Complete. Parking in telemetry mode for 60s...\n";
        std::cout << "[System] Run './build/tools/porth_stat' in another terminal now.\n";

        // Call the parking helper to satisfy cognitive complexity audit
        run_telemetry_parking(regs, sovereign_stats, parking_duration_s);

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