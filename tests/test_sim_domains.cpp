#include "../include/porth/PorthSimDevice.hpp"
#include <iostream>
#include <thread>

auto main() -> int {
    using namespace porth;

    // Constants to resolve magic number and literal suffix warnings
    constexpr int monitor_iterations = 5;
    constexpr double scale_factor    = 1000.0;
    constexpr auto monitor_delay     = std::chrono::milliseconds(500);
    constexpr uint32_t start_command = 0x1U;

    try {
        std::cout << "--- Porth-Sim: Task 3 (Domain-Specific Models) ---" << '\n';
        PorthSimDevice sim("porth_sim_domains", true);
        auto* dev = sim.view();

        std::cout << "[Photonics] Monitoring Laser Temperature..." << '\n';
        for (int i = 0; i < monitor_iterations; ++i) {
            uint32_t t = sim.read_reg(dev->laser_temp);
            std::cout << "  - Laser Temp: " << (static_cast<double>(t) / scale_factor) << " C"
                      << '\n';

            if (i == 1) {
                std::cout << "[Driver] Starting Hardware (Heating Laser)..." << '\n';
                sim.write_reg(dev->control, start_command);
            }

            std::this_thread::sleep_for(monitor_delay);
        }

        uint32_t v = sim.read_reg(dev->gan_voltage);
        std::cout << "[Power] GaN Rail Voltage: " << (static_cast<double>(v) / scale_factor) << " V"
                  << '\n';

        std::cout << "\n[Success] Domain Models Operational." << '\n';

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }

    return 0;
}