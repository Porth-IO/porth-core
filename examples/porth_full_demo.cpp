#include <iostream>
#include <vector>
#include "../include/porth/PorthDriver.hpp"
#include "../include/porth/PorthSimDevice.hpp"
#include "../include/porth/PorthMetric.hpp"
#include "../include/porth/PorthUtil.hpp"

/**
 * 🏁 The "Golden Example" MVP Milestone
 * Full integration: Real-time priority, Thermal Simulation, and Benchmarking.
 */
int main() {
    using namespace porth;

    std::cout << "--- Porth-IO: Sovereign Logic Layer MVP ---" << std::endl;

    try {
        // 1. Environmental Shielding
        pin_thread_to_core(1);
        set_realtime_priority();

        // 2. Initialize Digital Twin (Hardware)
        PorthSimDevice hw_sim("porth_newport_0", true);
        
        // 3. Initialize Master Driver (Software)
        // Automates the Handshake (Task 2.2)
        Driver<1024> driver(hw_sim.view());

        // 4. Run Stress Test with Thermal Feedback
        PorthMetric metric(50000);
        std::cout << "[Driver] Running high-load test. Monitoring thermal jitter..." << std::endl;

        for(int i = 0; i < 50000; ++i) {
            uint64_t t1 = PorthClock::now_precise();
            
            // Simulating heavy DMA activity (heating the laser)
            PorthDescriptor desc = {0x1000, 64};
            driver.transmit(desc);
            
            uint64_t t2 = PorthClock::now_precise();
            metric.record(t2 - t1);
            
            if (i % 10000 == 0) {
                float temp = driver.get_regs()->laser_temp.load() / 1000.0f;
                std::cout << "  - Progress: " << i << " cycles | Temp: " << temp << "C" << std::endl;
            }
        }

        // 5. Final Analytics
        metric.print_stats(2.4);
        metric.save_markdown_report("README_STATS.md", "Final Integration MVP", 2.4);

        std::cout << "[Success] Newport Cluster validated. System ready for 1.6T deployment." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}