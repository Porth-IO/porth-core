/**
 * @file network_portal_demo.cpp
 * @brief Sovereign Interconnect - Final Victory Build.
 */

#include "porth/PorthNetworkPortal.hpp"
#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>

struct SignalContext {
    static inline std::atomic<bool> keep_running{true};
    static void handler(int signal_num) {
        (void)signal_num;
        keep_running.store(false, std::memory_order_release);
    }
};

auto main() -> int {
    using namespace porth;
    std::signal(SIGINT, SignalContext::handler);

    try {
        PorthNetworkPortal portal("porth0");

        // Locate the eBPF object. It's usually in the root porth-core or build dir.
        std::string bpf_path = "porth_xdp_kern.o";
        if (!std::filesystem::exists(bpf_path)) {
            bpf_path = "../porth_xdp_kern.o"; // Try parent if running from build/
        }

        if (!std::filesystem::exists(bpf_path)) {
            throw std::runtime_error(
                "Could not find porth_xdp_kern.o. Please ensure it is compiled.");
        }

        portal.initialize_umem();
        portal.create_socket();
        portal.load_kernel_program(bpf_path);
        portal.bind_xsk_map();
        portal.prime_fill_ring();

        std::cout << "--- Sovereign Sandbox Online [porth0] ---\n";
        std::cout << "--- Polling for Sovereign Signals ---\n";

        while (SignalContext::keep_running.load(std::memory_order_acquire)) {
            portal.poll_rx();
        }

        std::cout << "\n[Porth-Portal] Graceful Shutdown.\n";

    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << "\n";
        return 1;
    }
    return 0;
}