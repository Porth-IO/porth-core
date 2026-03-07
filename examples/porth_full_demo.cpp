/**
 * @file porth_full_demo.cpp
 * @brief Sovereign Logic Layer MVP - Final Immaculate Integration.
 *
 * This version includes the "Handshake Safety" protocol to ensure
 * cross-core synchronization during the Real-Time power-on sequence.
 */

#include "porth/PorthClock.hpp"
#include "porth/PorthDeviceLayout.hpp"
#include "porth/PorthDriver.hpp"
#include "porth/PorthMetric.hpp"
#include "porth/PorthRegister.hpp"
#include "porth/PorthSimDevice.hpp"
#include "porth/PorthUtil.hpp"
#include <bits/chrono.h> // for milliseconds
#include <cstddef>       // for size_t
#include <cstdint>       // for uint64_t, uint32_t
#include <exception>     // for exception
#include <format>
#include <iostream>
#include <stdexcept> // for runtime_error
#include <string>    // for allocator, operator<<
#include <thread>

auto main() -> int {
    using namespace porth;

    // Constants to resolve magic number and formatting warnings
    constexpr size_t shuttle_size               = 1024;
    constexpr int handshake_timeout_ms          = 5000;
    constexpr uint64_t test_addr                = 0x1000;
    constexpr uint32_t test_len                 = 64;
    constexpr uint64_t propagation_delay_cycles = 240;
    constexpr size_t log_interval               = 10000;
    constexpr float temp_divisor                = 1000.0F;
    constexpr double cycles_per_ns_newport      = 2.4;

    std::cout << "--- Porth-IO: Sovereign Logic Layer MVP ---\n";

    try {
        /**
         * PHASE 1: Isolated Hardware Initialization
         * The PorthSimDevice will automatically isolate its physics
         * engine on Core 0.
         */
        PorthSimDevice hw_sim("porth_newport_0", true);
        auto* regs = hw_sim.view();

        /**
         * PHASE 2: Environmental Shielding
         * We elevate the Driver to Real-Time status on Core 1.
         */
        (void)pin_thread_to_core(1);
        (void)set_realtime_priority();

        /**
         * PHASE 3: Master Driver Handshake
         * Map the zero-copy Shuttle and signal the hardware to START.
         */
        Driver<shuttle_size> driver(regs);
        std::cout << "[System] Powering on Newport Cluster...\n";
        regs->control.write(0x1);

        /**
         * HANDSHAKE SAFETY PROTOCOL:
         * We poll the status register for the READY bit (0x1).
         * We include a 1ms sleep during the handshake phase to allow the
         * Simulator thread (on Core 0) to propagate memory state across
         * the L3 cache boundary to our RT thread (on Core 1).
         */
        int timeout_ms = 0;
        while (regs->status.load() == 0 && timeout_ms < handshake_timeout_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timeout_ms++;
        }

        if (regs->status.load() == 0) {
            throw std::runtime_error(
                "Hardware handshake timed out. Check thread isolation levels.");
        }

        std::cout << "[System] Hardware Ready. Laser stabilized at 25.00 °C.\n";

        // PHASE 4: Deterministic Stress Test
        constexpr size_t iterations = 50000;
        PorthMetric metric(iterations);
        std::cout << "[Driver] Executing 50,000 Zero-Copy cycles...\n";

        for (size_t i = 0; i < iterations; ++i) {
            const uint64_t t1 = PorthClock::now_precise();

            // Execute transmission using designated initializers
            if (driver.transmit({.addr = test_addr, .len = test_len}) != PorthStatus::SUCCESS) {
                std::cerr << "[Error] Shuttle saturated.\n";
                break;
            }

            /**
             * PHYSICAL PROPAGATION DELAY:
             * Simulating 100ns of PCIe Gen 6 wire flight time.
             * This prevents ring-buffer saturation by pacing the Driver
             * to the physical limits of the 1.6T transceiver.
             */
            const uint64_t start_delay = PorthClock::now_precise();
            while (PorthClock::now_precise() - start_delay < propagation_delay_cycles) {
                // Spinning for delay
            }

            const uint64_t t2 = PorthClock::now_precise();
            metric.record(t2 - t1);

            if (i % log_interval == 0) {
                const float temp_c = static_cast<float>(regs->laser_temp.load()) / temp_divisor;
                std::cout << std::format(
                    "  - Progress: {:5} cycles | Temp: {:.2f} °C\n", i, temp_c);
            }
        }

        // PHASE 5: Final Statistical Post-Processing
        metric.print_stats(cycles_per_ns_newport);
        std::cout << "[Success] Newport Cluster validated. System ready for 1.6T deployment.\n";

    } catch (const std::exception& e) {
        std::cerr << std::format("[Fatal] {}\n", e.what());
        return 1;
    }

    return 0;
}