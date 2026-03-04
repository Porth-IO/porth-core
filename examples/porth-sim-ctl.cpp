#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <iomanip>
#include "../include/porth/PorthSimDevice.hpp"

void show_usage() {
    std::cout << "Usage: porth-sim-ctl [command] [args]\n"
              << "Commands:\n"
              << "  launch <name> <delay_ns>  Launch a virtual Newport chip\n"
              << "  stress <name>             Inject 50% bus noise (Chaos Mode)\n"
              << "  monitor <name>            Real-time telemetry dashboard\n";
}

int main(int argc, char* argv[]) {
    using namespace porth;

    if (argc < 2) {
        show_usage();
        return 1;
    }

    std::string cmd = argv[1];

    try {
        if (cmd == "launch" && argc == 4) {
            std::string name = argv[2];
            uint64_t delay = std::stoull(argv[3]);
            
            std::cout << "[Orchestrator] Launching Virtual Chip: " << name << "..." << std::endl;
            PorthSimDevice sim(name, true); // Owner
            sim.get_phy().set_config(delay, 25);
            
            std::cout << "[System] Chip online at /dev/shm/" << name << ". Press Ctrl+C to stop." << std::endl;
            // Keep the process alive to own the shared memory
            while(true) std::this_thread::sleep_for(std::chrono::seconds(1));

        } else if (cmd == "stress" && argc == 3) {
            std::string name = argv[2];
            std::cout << "[Orchestrator] Connecting to " << name << " for Stress Test..." << std::endl;
            PorthSimDevice sim(name, false); // Client
            
            // Sub-task 5.2: Scenario - Heavy Bus Noise
            std::cout << "[Chaos] Injecting 50% Bus Jitter and Register Corruption..." << std::endl;
            sim.apply_scenario(500, 250, true); 
            std::cout << "[Success] Stress scenario applied. Monitor the 'Tail Latency' in your driver." << std::endl;

        } else if (cmd == "monitor" && argc == 3) {
            std::string name = argv[2];
            PorthSimDevice sim(name, false); // Client
            auto* dev = sim.view();

            // Sub-task 5.3: TUI Dashboard
            std::cout << "\n--- [ NEWPORT CLUSTER TELEMETRY: " << name << " ] ---" << std::endl;
            std::cout << "TIME     | TEMP (C) | RAIL (V) | STATUS | COUNTER" << std::endl;
            std::cout << "------------------------------------------------" << std::endl;

            for(int i = 0; i < 20; ++i) {
                float temp = sim.read_reg(dev->laser_temp) / 1000.0f;
                float volt = sim.read_reg(dev->gan_voltage) / 1000.0f;
                uint32_t stat = sim.read_reg(dev->status);
                uint64_t count = sim.read_reg(dev->counter);

                std::cout << i << "s       | " 
                          << std::fixed << std::setprecision(1) << temp << "      | " 
                          << volt << "      | 0x" << std::hex << stat << "    | " << std::dec << count << std::endl;
                
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

        } else {
            show_usage();
        }
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] Orchestrator Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}