#include "../include/porth/PorthSimDevice.hpp"
#include <iostream>
#include <thread>

int main() {
    using namespace porth;

    try {
        std::cout << "--- Porth-Sim: Task 3 (Domain-Specific Models) ---" << std::endl;
        PorthSimDevice sim("porth_sim_domains", true);
        auto* dev = sim.view();

        std::cout << "[Photonics] Monitoring Laser Temperature..." << std::endl;
        for (int i = 0; i < 5; ++i) {
            uint32_t t = sim.read_reg(dev->laser_temp);
            std::cout << "  - Laser Temp: " << (t / 1000.0) << " C" << std::endl;

            if (i == 1) {
                std::cout << "[Driver] Starting Hardware (Heating Laser)..." << std::endl;
                sim.write_reg(dev->control, 0x1u);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        uint32_t v = sim.read_reg(dev->gan_voltage);
        std::cout << "[Power] GaN Rail Voltage: " << (v / 1000.0) << " V" << std::endl;

        std::cout << "\n[Success] Domain Models Operational." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}