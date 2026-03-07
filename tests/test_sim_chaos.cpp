#include "porth/PorthDeviceLayout.hpp"
#include "porth/PorthRegister.hpp"
#include "porth/PorthSimDevice.hpp"
#include <bits/chrono.h> // for milliseconds
#include <cstdint>       // for uint32_t
#include <exception>     // for exception
#include <iostream>
#include <memory> // for allocator
#include <thread>

auto main() -> int {
    using namespace porth;

    // Constants to resolve magic number warnings
    constexpr auto corruption_wait_ms   = std::chrono::milliseconds(50);
    constexpr int watchdog_max_retries  = 5;
    constexpr auto watchdog_interval_ms = std::chrono::milliseconds(200);

    try {
        std::cout << "--- Porth-Sim: Task 4 (Chaos Engineering) ---" << '\n';
        PorthSimDevice sim("porth_sim_chaos", true);
        auto* dev = sim.view();

        // 1. Test Register Corruption (4.1)
        std::cout << "[Chaos] Enabling Status Register Corruption..." << '\n';
        dev->status.write(0x2); // Set to READY
        sim.trigger_corruption(true);

        std::this_thread::sleep_for(corruption_wait_ms);
        uint32_t corrupted = sim.read_reg(dev->status);
        std::cout << "   - Status (Expected 0x2): 0x" << std::hex << corrupted << std::dec << '\n';
        sim.trigger_corruption(false);

        // 2. Test Deadlock & Watchdog (4.2)
        std::cout << "\n[Chaos] Injecting Hardware Deadlock..." << '\n';
        sim.trigger_deadlock(true);

        // Simple Watchdog Logic
        uint32_t last_temp = sim.read_reg(dev->laser_temp);
        bool recovered     = false;

        for (int i = 0; i < watchdog_max_retries; ++i) {
            std::this_thread::sleep_for(watchdog_interval_ms);
            uint32_t current_temp = sim.read_reg(dev->laser_temp);

            if (current_temp == last_temp) {
                std::cout << "   - Watchdog Alert: Hardware is HUNG. Attempting Reset..." << '\n';
                sim.trigger_deadlock(false); // Simulated Hardware Reset
                recovered = true;
                break;
            }
        }

        if (recovered) {
            std::cout << "[Success] Watchdog recovered the system." << '\n';
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }

    return 0;
}