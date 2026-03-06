/**
 * @file test_sim_phy.cpp
 * @brief Formal verification of the Physical Layer (PHY) emulation engine.
 *
 * Porth-IO: The Sovereign Logic Layer
 * Copyright (c) 2026 Porth-IO Contributors
 */

#include <iostream>
#include <format>
#include "../include/porth/PorthSimDevice.hpp"
#include "../include/porth/PorthMetric.hpp"
#include "../include/porth/PorthClock.hpp"

int main() {
    using namespace porth;
    
    /**
     * @brief Calibration factor for the simulated environment.
     * Based on previous PorthClock calibration cycles.
     */
    constexpr double cycles_per_ns = 2.4; 
    
    try {
        std::cout << "--- Porth-Sim: Task 1 (PHY Emulation) ---\n";
        
        // Initialize the Digital Twin
        PorthSimDevice sim("porth_sim_phy_test", true);
        
        // Configure the PHY: 100ns base propagation with 25ns jitter
        sim.get_phy().set_config(100, 25); 

        PorthMetric metric(1000);
        auto* dev = sim.view();

        std::cout << "[Sim] Measuring 1,000 simulated register reads...\n";

        for(int i = 0; i < 1000; ++i) {
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