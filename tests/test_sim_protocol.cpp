/**
 * @file test_sim_protocol.cpp
 * @brief Formal verification of the PCIe Gen 6 FLIT and TLP protocol simulation.
 *
 * Porth-IO: The Sovereign Logic Layer
 * Copyright (c) 2026 Porth-IO Contributors
 */

#include "../include/porth/PorthClock.hpp"
#include "../include/porth/PorthDeviceLayout.hpp"
#include "../include/porth/PorthMetric.hpp"
#include "../include/porth/PorthSimDevice.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <string>

auto main() -> int {
    using namespace porth;

    // Constants to resolve magic number and literal suffix warnings
    constexpr uint32_t start_command = 0x1U;
    constexpr int flit_iterations    = 500;
    constexpr double calibrated_cpns = 2.4;

    try {
        std::cout << "--- Porth-Sim: Task 2 (Advanced Protocol) ---\n";

        // Initialize the Digital Twin for protocol-level verification
        PorthSimDevice sim("porth_sim_proto_test", true);
        auto* dev = sim.view();

        /**
         * @brief Simulated FLIT-mode Write.
         * We write to the control register using the protocol-aware interface,
         * which simulates the specific TLP overhead of PCIe Gen 6.
         */
        std::cout << "[Sim] Executing FLIT-mode Write to Control Register...\n";
        sim.write_flit(dev->control, offsetof(PorthDeviceLayout, control), start_command);

        PorthMetric protocol_metric(static_cast<size_t>(flit_iterations));
        std::cout << "[Sim] Measuring 500 FLIT reads with simulated FEC noise...\n";

        for (int i = 0; i < flit_iterations; ++i) {
            const uint64_t t1 = PorthClock::now_precise();

            /**
             * @note read_flit is marked [[nodiscard]] to ensure protocol
             * completions are never ignored in production. We cast to void
             * here for the benchmark.
             */
            (void)sim.read_flit(dev->status, offsetof(PorthDeviceLayout, status));

            const uint64_t t2 = PorthClock::now_precise();
            protocol_metric.record(t2 - t1);
        }

        // Output the protocol-specific jitter analysis
        protocol_metric.print_stats(calibrated_cpns);
        std::cout << "\n[Success] Protocol Simulation Complete.\n";

    } catch (const std::exception& e) {
        std::cerr << std::format("Fatal: {}\n", e.what());
        return 1;
    }

    return 0;
}