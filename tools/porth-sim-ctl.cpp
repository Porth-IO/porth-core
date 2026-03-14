#include "porth/PorthDeviceLayout.hpp"
#include "porth/PorthSimDevice.hpp"
#include "porth/PorthSimPHY.hpp"
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

    // Sovereign Dashboard Initialization: Clear screen and draw header
    std::cout << "\033[2J\033[H"; // ANSI escape code to clear terminal and home cursor
    std::cout << "========================================================\n";
    std::cout << "          PORTH-IO: SOVEREIGN LOGIC DASHBOARD           \n";
    std::cout << "========================================================\n";


    for (int i = 0; i < iterations; ++i) {

        if (dev->status.load() == 0) {
            std::cout << "\n[System] Chip Offline (Safety Trip/Shutdown). Demo Finished.\n";
            return; 
        }

        // 1. Read Physics Registers from the Sovereign Logic Layer
        const uint32_t temp_mc = sim.read_reg(dev->laser_temp);
        const uint32_t volt_mv = sim.read_reg(dev->gan_voltage);
        const int32_t snr_raw  = sim.read_reg(dev->rf_snr);
        const uint64_t count   = sim.read_reg(dev->counter);

        // 1. Clear the line before printing to prevent ghosting/glitches
        // \r moves to start, \033[K clears from cursor to end of line
        std::cout << "\r\033[K" << std::flush; 

        // 2. Format Split-Screen View: NET on left, HW on right
        std::cout << std::format("[NET] Packets: {:<10} | [HW] Temp: {:.1f}C  Volt: {:>3}V  SNR: {:.2f}dB",
                                 count,
                                 static_cast<double>(temp_mc) / static_cast<double>(scale_factor),
                                 volt_mv / static_cast<uint32_t>(scale_factor),
                                 static_cast<double>(snr_raw) / 100.0);

        // 3. Visual "Trip Wire" Alert
        if (temp_mc > 40000) { 
            std::cout << "  !! THERMAL WARNING !!";
        }
        std::cout << std::flush;

        // Timing delay so the human eye can see the dashboard
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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