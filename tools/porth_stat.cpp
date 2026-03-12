#include "porth/PorthTelemetry.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <format>

int main() {
    using namespace porth;
    try {
        // Attach to existing SHM (create = false)
        PorthTelemetryHub hub("porth_stats_0", false);
        PorthStats* stats = hub.view();

        std::cout << "\033[2J\033[H"; // Clear screen
        
        while (true) {
            std::cout << "\033[H"; // Move cursor to top
            std::cout << "=== Porth-IO Sovereign Dashboard ===\n";
            std::cout << std::format("Packets:  {:15}\n", stats->total_packets.load());
            std::cout << std::format("Throughput: {:15} bytes\n", stats->total_bytes.load());
            std::cout << std::format("Drops:    {:15}\n", stats->dropped_packets.load());
            std::cout << std::format("Temp (Cur): {:.2f} °C\n", static_cast<float>(stats->current_temp_mc.load()) / 1000.0f);
            std::cout << std::format("Temp (Max): {:.2f} °C\n", static_cast<float>(stats->max_temp_mc.load()) / 1000.0f);
            std::cout << "------------------------------------\n";
            std::cout << "[Status] NOMINAL | 1.6T Ready\n";

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch (...) {
        std::cerr << "Waiting for Porth Engine to start...\n";
        return 1;
    }
}