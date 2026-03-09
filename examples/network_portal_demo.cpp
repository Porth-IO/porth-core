/**
 * @file network_portal_demo.cpp
 * @brief Verification of the AF_XDP library link and eBPF kernel loading.
 */

#include "porth/PorthNetworkPortal.hpp"
#include <iostream>
#include <exception>

auto main() -> int {
    using namespace porth;

    std::cout << "--- Porth-IO: AF_XDP & eBPF Verification ---\n";

    try {
        PorthNetworkPortal portal("lo");

        // We try to load the bytecode we just compiled.
        // It will be located in your build folder.
        portal.load_kernel_program("./porth_xdp_kern.o");

        if (portal.is_active()) {
            std::cout << "[Success] Sovereign Portal Active and Kernel-Loaded.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "[Fatal] Portal Exception: " << e.what() << "\n";
        std::cout << "[Note] If this failed with 'Failed to open', ensure you are running from build folder.\n";
        return 1;
    }

    return 0;
}