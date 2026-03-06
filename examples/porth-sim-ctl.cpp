#include "../include/porth/PorthSimDevice.hpp"
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

void show_usage() {
    std::cout << "Usage: porth-sim-ctl [command] [args]\n"
              << "Commands:\n"
              << "  launch <name> <delay_ns>  Launch a virtual Newport chip\n"
              << "  stress <name>             Inject 50% bus noise (Chaos Mode)\n"
              << "  monitor <name>            Real-time telemetry dashboard\n";
}

auto main(int argc, char* argv[]) -> int {
    using namespace porth;

    // Constants to resolve magic number and formatting warnings
    constexpr int default_temp_c     = 25;
    constexpr uint64_t base_ns       = 500;
    constexpr uint64_t jitter_ns     = 250;
    constexpr int monitor_iterations = 20;
    constexpr float scale_factor     = 1000.0F;

    if (argc < 2) {
        show_usage();
        return 1;
    }

    std::string cmd = argv[1];

    try {
        if (cmd == "launch" && argc == 4) {
            std::string name = argv[2];
            uint64_t delay   = std::stoull(argv[3]);

            std::cout << "[Orchestrator] Launching Virtual Chip: " << name << "..." << '\n';
            PorthSimDevice sim(name, true); // Owner
            sim.get_phy().set_config(delay, default_temp_c);

            std::cout << "[System] Chip online at /dev/shm/" << name << ". Press Ctrl+C to stop."
                      << '\n';
            // Keep the process alive to own the shared memory
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

        } else if (cmd == "stress" && argc == 3) {
            std::string name = argv[2];
            std::cout << "[Orchestrator] Connecting to " << name << " for Stress Test..." << '\n';
            PorthSimDevice sim(name, false); // Client

            // Sub-task 5.2: Scenario - Heavy Bus Noise
            std::cout << "[Chaos] Injecting 50% Bus Jitter and Register Corruption..." << '\n';
            // Fix: Names now match the function signature to avoid suspicious-call-argument warning
            sim.apply_scenario(base_ns, jitter_ns, true);
            std::cout
                << "[Success] Stress scenario applied. Monitor the 'Tail Latency' in your driver."
                << '\n';

        } else if (cmd == "monitor" && argc == 3) {
            std::string name = argv[2];
            PorthSimDevice sim(name, false); // Client
            auto* dev = sim.view();

            // Sub-task 5.3: TUI Dashboard
            std::cout << "\n--- [ NEWPORT CLUSTER TELEMETRY: " << name << " ] ---" << '\n';
            std::cout << "TIME     | TEMP (C) | RAIL (V) | STATUS | COUNTER" << '\n';
            std::cout << "------------------------------------------------" << '\n';

            for (int i = 0; i < monitor_iterations; ++i) {
                float temp     = static_cast<float>(sim.read_reg(dev->laser_temp)) / scale_factor;
                float volt     = static_cast<float>(sim.read_reg(dev->gan_voltage)) / scale_factor;
                uint32_t stat  = sim.read_reg(dev->status);
                uint64_t count = sim.read_reg(dev->counter);

                std::cout << i << "s       | " << std::fixed << std::setprecision(1) << temp
                          << "      | " << volt << "      | 0x" << std::hex << stat << "    | "
                          << std::dec << count << '\n';

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

        } else {
            show_usage();
        }
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] Orchestrator Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}