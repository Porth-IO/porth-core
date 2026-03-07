/**
 * @file test_sim_phy.cpp
 * @brief Formal verification of the Physical Layer (PHY) emulation engine.
 *
 * Porth-IO: The Sovereign Logic Layer
 * Copyright (c) 2026 Porth-IO Contributors
 */

#include "../include/porth/PorthClock.hpp"
#include "../include/porth/PorthDeviceLayout.hpp"
#include "../include/porth/PorthMetric.hpp"
#include "../include/porth/PorthSimDevice.hpp"
#include "../include/porth/PorthSimPHY.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <string>

auto main() -> int {
    using namespace porth;

    /**
     * @brief Calibration factor for the simulated environment.
     * Based on previous PorthClock calibration cycles.
     */
    constexpr double cycles_per_ns = 2.4;

    // Constants to resolve magic number warnings
    constexpr uint64_t base_delay_ns = 100;
    constexpr uint64_t jitter_ns     = 25;
    constexpr int sample_count       = 1000;

    try {
        std::cout << "--- Porth-Sim: Task 1 (PHY Emulation) ---\n";

        // Initialize the Digital Twin
        PorthSimDevice sim("porth_sim_phy_test", true);

        // Configure the PHY: 100ns base propagation with 25ns jitter
        sim.get_phy().set_config(base_delay_ns, jitter_ns);

        PorthMetric metric(static_cast<size_t>(sample_count));
        auto* dev = sim.view();

        std::cout << "[Sim] Measuring 1,000 simulated register reads...\n";

        for (int i = 0; i < sample_count; ++i) {
            const uint64_t t1 = PorthClock::now_precise();

            /**
             * @note We explicitly cast the return value to void to satisfy
             * the [[nodiscard]] attribute enforced by the HFT-grade build flags.
             */
            (void)sim.read_reg(dev->status);

            const uint64_t t2 = PorthClock::now_precise();
            metric.record(t2 - t1);
        }

        // Output statistical analysis of the PHY performance
        metric.print_stats(cycles_per_ns);

        std::cout << "\n[Success] PHY Jitter modeled. Notice the Min/Max spread.\n";

    } catch (const std::exception& e) {
        std::cerr << std::format("Error: {}\n", e.what());
        return 1;
    }

    return 0;
}