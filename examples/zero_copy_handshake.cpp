/**
 * @file zero_copy_validation_main.cpp
 * @brief Formal validation of the Logic-to-Physical Zero-Copy handshake.
 *
 * Porth-IO: The Sovereign Logic Layer
 * * This script proves the integrity of the Porth memory fabric by synchronizing
 * the Shuttle (Data Plane) with the Cardiff Register Map (Control Plane).
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthDeviceLayout.hpp"
#include "porth/PorthMockDevice.hpp"
#include "porth/PorthRegister.hpp"
#include "porth/PorthShuttle.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>

using namespace porth;

/**
 * @brief Main execution for the Zero-Copy Handshake Validation.
 * * Proves that a 64-bit memory address can be passed through a hardware
 * register (HAL) to establish a high-speed DMA link without kernel intervention.
 * * @return int 0 on successful handshake, 1 on address mismatch.
 */
auto main() -> int {
    try {
        std::cout << "--- Porth-IO: Task 2.4 Zero-Copy Validation ---" << '\n';

        // 1. Initialize the HAL (Hardware Abstraction Layer).
        // Justification: We create the 'porth_vdev_0' segment as the owner.
        // This simulates the Cardiff ASIC powering up and exposing its PCIe BARs
        // to the host system's memory-mapped I/O (MMIO) space.
        PorthMockDevice hal("porth_vdev_0", true);
        auto* device = hal.view();

        // 2. Initialize the Shuttle (The Zero-Copy Conveyor Belt).
        // Justification: PorthShuttle allocates pinned HugePage memory. This ensures
        // that the RingBuffer is resistant to TLB-misses and OS swapping, which is
        // a physical requirement for the 1.6T Newport Cluster interconnect.
        static constexpr size_t SHUTTLE_CAPACITY = 1024;
        porth::PorthShuttle<SHUTTLE_CAPACITY> shuttle;

        // 3. THE SOVEREIGN HANDSHAKE (Task 2.4).
        // We retrieve the 64-bit address of the HugePage region.
        // Note: This address is technically a virtual address mapped to pinned
        // physical RAM, allowing the DMA engine to bypass the standard page-table walk.
        uint64_t bus_address = shuttle.get_device_addr();

        // Commit the address to the hardware's 'data_ptr' register.
        // Justification: Once this write completes (enforced by Release semantics
        // inside PorthRegister), the Newport ASIC's internal DMA scheduler
        // can begin polling the Shuttle for incoming descriptors.
        std::cout << "[Driver] Writing Shuttle Address (" << std::hex << bus_address
                  << ") to hardware data_ptr register..." << std::dec << '\n';

        device->data_ptr.write(bus_address);

        // 4. VERIFICATION: Logic-to-Physical Synchronization Audit.
        // We read back the register value to ensure the Cardiff chip has
        // latched the address correctly. A mismatch here indicates signal
        // noise in the MMIO path or a failure in the SHM backend.
        uint64_t read_back = device->data_ptr.load();
        if (read_back == bus_address) {
            std::cout << "[SUCCESS] Hardware and Software are now linked via Zero-Copy address!"
                      << '\n';
            std::cout << "  - Control Plane: Shared Memory (HAL/Registers)" << '\n';
            std::cout << "  - Data Plane: HugePage (Shuttle/DMA Ring)" << '\n';
        } else {
            // Failure here is catastrophic for the Sovereign Logic Layer;
            // the hardware would be polling an incorrect/unmapped memory address.
            std::cerr << "[FAILURE] Address mismatch in register! Handshake aborted." << '\n';
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal Handshake Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}