#include <iostream>
#include "../include/porth/PorthSimDevice.hpp"
#include "../include/porth/PorthMetric.hpp"

int main() {
    using namespace porth;
    
    try {
        std::cout << "--- Porth-Sim: Task 2 (Advanced Protocol) ---" << std::endl;
        PorthSimDevice sim("porth_sim_proto", true);
        auto* dev = sim.view();
        
        // FIX: Added 'u' to 0x1u to ensure types match (uint32_t)
        std::cout << "[Sim] Executing FLIT-mode Write to Control Register..." << std::endl;
        sim.write_flit(dev->control, offsetof(PorthDeviceLayout, control), 0x1u);

        PorthMetric protocol_metric(500);
        std::cout << "[Sim] Measuring 500 FLIT reads with simulated FEC noise..." << std::endl;

        for(int i = 0; i < 500; ++i) {
            uint64_t t1 = PorthClock::now_precise();
            sim.read_flit(dev->status, offsetof(PorthDeviceLayout, status));
            uint64_t t2 = PorthClock::now_precise();
            protocol_metric.record(t2 - t1);
        }

        protocol_metric.print_stats(2.4);
        std::cout << "\n[Success] Protocol Simulation Complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}