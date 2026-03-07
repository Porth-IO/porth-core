#include "../include/porth/PorthDeviceLayout.hpp"
#include "../include/porth/PorthMockDevice.hpp"
#include "../include/porth/PorthRegister.hpp"
#include "../include/porth/PorthShuttle.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>

/**
 * Zero-Copy Handshake Test:
 * This script proves that we can pass a memory address through
 * a hardware register (HAL) to establish a high-speed data link.
 */
using namespace porth;

auto main() -> int {
    try {
        std::cout << "--- Porth-IO: Task 2.4 Zero-Copy Validation ---" << '\n';

        // 1. Initialize the HAL (Mock Register Map)
        // We create it as the owner ("porth_vdev_0")
        PorthMockDevice hal("porth_vdev_0", true);
        auto* device = hal.view();

        // 2. Initialize the Shuttle (HugePage + RingBuffer)
        static constexpr size_t SHUTTLE_CAPACITY = 1024;
        porth::PorthShuttle<SHUTTLE_CAPACITY> shuttle;

        // 3. THE HANDSHAKE (Task 2.4)
        // We take the address of our high-speed conveyor belt...
        uint64_t bus_address = shuttle.get_device_addr();

        // ...and we write it into the hardware's data pointer register.
        std::cout << "[Driver] Writing Shuttle Address (" << std::hex << bus_address
                  << ") to hardware data_ptr register..." << std::dec << '\n';

        device->data_ptr.write(bus_address);

        // 4. Verification
        uint64_t read_back = device->data_ptr.load();
        if (read_back == bus_address) {
            std::cout << "[SUCCESS] Hardware and Software are now linked via Zero-Copy address!"
                      << '\n';
            std::cout << "  - Registers: Shared Memory (HAL)" << '\n';
            std::cout << "  - Data Plane: HugePage (Shuttle)" << '\n';
        } else {
            std::cerr << "[FAILURE] Address mismatch in register!" << '\n';
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}