#include "../include/porth/PorthDeviceLayout.hpp"
#include "../include/porth/PorthSimDevice.hpp"
#include "../include/porth/PorthSimPHY.hpp"
#include <bits/chrono.h> // for seconds
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

namespace {
using namespace porth;

/**
 * @brief handle_launch: Initializes and maintains a virtual hardware owner.
 */
void handle_launch(const std::string& name, uint64_t delay, int default_temp_c) {
    std::cout << "[Orchestrator] Launching Virtual Chip: " << name << "..." << '\n';
    PorthSimDevice sim(name, true); // Owner
    sim.get_phy().set_config(delay, static_cast<uint64_t>(default_temp_c));

    std::cout << "[System] Chip online at /dev/shm/" << name << ". Press Ctrl+C to stop." << '\n';

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/**
 * @brief handle_stress: Connects to an existing device and injects noise.
 */
void handle_stress(const std::string& name, uint64_t base_ns, uint64_t jitter_ns) {
    std::cout << "[Orchestrator] Connecting to " << name << " for Stress Test..." << '\n';
    PorthSimDevice sim(name, false); // Client

    std::cout << "[Chaos] Injecting 50% Bus Jitter and Register Corruption..." << '\n';
    sim.apply_scenario(base_ns, jitter_ns, true);
    std::cout << "[Success] Stress scenario applied. Monitor the 'Tail Latency' in your driver.\n";
}

/**
 * @brief handle_monitor: Displays real-time register telemetry.
 * @note NOLINT is placed on the first swappable parameter line to survive clang-format wrapping.
 */
void handle_monitor(const std::string& name,
                    int iterations, // NOLINT(bugprone-easily-swappable-parameters)
                    float scale_factor) {
    PorthSimDevice sim(name, false); // Client
    auto* dev = sim.view();

    std::cout << "\n--- [ NEWPORT CLUSTER TELEMETRY: " << name << " ] ---" << '\n';
    std::cout << "TIME     | TEMP (C) | RAIL (V) | STATUS | COUNTER" << '\n';
    std::cout << "------------------------------------------------" << '\n';

    for (int i = 0; i < iterations; ++i) {
        const float temp     = static_cast<float>(sim.read_reg(dev->laser_temp)) / scale_factor;
        const float volt     = static_cast<float>(sim.read_reg(dev->gan_voltage)) / scale_factor;
        const uint32_t stat  = sim.read_reg(dev->status);
        const uint64_t count = sim.read_reg(dev->counter);

        std::cout << i << "s       | " << std::fixed << std::setprecision(1) << temp << "      | "
                  << volt << "      | 0x" << std::hex << stat << "    | " << std::dec << count
                  << '\n';

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
} // namespace

void show_usage() {
    std::cout << "Usage: porth-sim-ctl [command] [args]\n"
              << "Commands:\n"
              << "  launch <name> <delay_ns>  Launch a virtual Newport chip\n"
              << "  stress <name>             Inject 50% bus noise (Chaos Mode)\n"
              << "  monitor <name>            Real-time telemetry dashboard\n";
}

auto main(int argc, char* argv[]) -> int {
    using namespace porth;

    constexpr int default_temp_c     = 25;
    constexpr uint64_t base_ns       = 500;
    constexpr uint64_t jitter_ns     = 250;
    constexpr int monitor_iterations = 20;
    constexpr float scale_factor     = 1000.0F;

    if (argc < 2) {
        show_usage();
        return 1;
    }

    const std::string cmd = argv[1];

    try {
        if (cmd == "launch" && argc == 4) {
            handle_launch(argv[2], std::stoull(argv[3]), default_temp_c);
        } else if (cmd == "stress" && argc == 3) {
            handle_stress(argv[2], base_ns, jitter_ns);
        } else if (cmd == "monitor" && argc == 3) {
            handle_monitor(argv[2], monitor_iterations, scale_factor);
        } else {
            show_usage();
        }
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] Orchestrator Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}