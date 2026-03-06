#include "../include/porth/PorthSimDevice.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    using namespace porth;

    try {
        std::cout << "--- Porth-Sim: Task 4 (Chaos Engineering) ---" << std::endl;
        PorthSimDevice sim("porth_sim_chaos", true);
        auto* dev = sim.view();

        // 1. Test Register Corruption (4.1)
        std::cout << "[Chaos] Enabling Status Register Corruption..." << std::endl;
        dev->status.write(0x2); // Set to READY
        sim.trigger_corruption(true);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        uint32_t corrupted = sim.read_reg(dev->status);
        std::cout << "  - Status (Expected 0x2): 0x" << std::hex << corrupted << std::dec
                  << std::endl;
        sim.trigger_corruption(false);

        // 2. Test Deadlock & Watchdog (4.2)
        std::cout << "\n[Chaos] Injecting Hardware Deadlock..." << std::endl;
        sim.trigger_deadlock(true);

        // Simple Watchdog Logic
        uint32_t last_temp = sim.read_reg(dev->laser_temp);
        bool recovered     = false;

        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            uint32_t current_temp = sim.read_reg(dev->laser_temp);

            if (current_temp == last_temp) {
                std::cout << "  - Watchdog Alert: Hardware is HUNG. Attempting Reset..."
                          << std::endl;
                sim.trigger_deadlock(false); // Simulated Hardware Reset
                recovered = true;
                break;
            }
        }

        if (recovered)
            std::cout << "[Success] Watchdog recovered the system." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}