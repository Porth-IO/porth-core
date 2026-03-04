#include <iostream>
#include <iomanip>
#include "../include/porth/PorthSimDevice.hpp"
#include "../include/porth/PorthMetric.hpp"

int main() {
    using namespace porth;
    
    // Use the 2.4GHz calibration from your earlier tests
    const double cycles_per_ns = 2.4; 
    
    try {
        std::cout << "--- Porth-Sim: Task 1 (PHY Emulation) ---" << std::endl;
        
        PorthSimDevice sim("porth_sim0", true);
        sim.get_phy().set_config(100, 25); // 100ns base, 25ns jitter

        PorthMetric metric(1000);
        auto* dev = sim.view();

        std::cout << "[Sim] Measuring 1,000 simulated register reads..." << std::endl;

        for(int i = 0; i < 1000; ++i) {
            uint64_t t1 = PorthClock::now_precise();
            sim.read_reg(dev->status);
            uint64_t t2 = PorthClock::now_precise();
            metric.record(t2 - t1);
        }

        metric.print_stats(cycles_per_ns);
        
        std::cout << "\n[Success] PHY Jitter modeled. Notice the Min/Max spread." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}