/**
 * @file PorthDriver.hpp
 * @brief Hardware-abstracted access to high-precision CPU cycle counters.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "PorthDeviceLayout.hpp"
#include "PorthShuttle.hpp"
#include "PorthUtil.hpp"
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace porth {

/**
 * @class Driver
 * @brief The high-level Master Driver for the Newport Cluster.
 *
 * This class encapsulates the HAL (Registers) and the Shuttle (Data Plane).
 * It manages the lifecycle of the memory fabric and automates the hardware
 * handshake during initialization.
 *
 * @tparam RingSize Depth of the DMA ring buffer. Must be a power of two.
 */
template <size_t RingSize = 1024>
class Driver {
private:
    PorthDeviceLayout* regs; ///< Pointer to memory-mapped hardware registers.
    std::unique_ptr<PorthShuttle<RingSize>> shuttle; ///< The zero-copy memory shuttle.

public:
    /**
     * @brief Constructor: Automates the Hardware Handshake.
     * * Maps the registers and writes the Shuttle address to the chip.
     * * @param hardware_regs Pointer to the device layout.
     * @throws std::runtime_error If hardware_regs is null.
     */
    explicit Driver(PorthDeviceLayout* hardware_regs) : regs(hardware_regs) {

        if (!regs) {
            throw std::runtime_error("Porth-Driver: Hardware registers pointer is null");
        }

        // Initialize the Shuttle (HugePage memory + RingBuffer)
        shuttle = std::make_unique<PorthShuttle<RingSize>>();

        // Task 2.2: Automated Handshake
        // Write the DMA-ready address of the Shuttle to the hardware's data_ptr
        const uint64_t dma_addr = shuttle->get_device_addr();
        regs->data_ptr.write(dma_addr);

        // Professional logging using C++23 std::format
        std::cout << std::format(
            "[Porth-Driver] Handshake Complete. Shuttle address 0x{:x} committed to hardware.\n",
            dma_addr);
    }

    /**
     * @brief transmit: Sends a descriptor via the Zero-Copy Shuttle.
     * @param desc The descriptor to push to hardware.
     * @return PorthStatus::SUCCESS on success, PorthStatus::FULL if the ring is saturated.
     */
    [[nodiscard]] PorthStatus transmit(const PorthDescriptor& desc) noexcept {
        if (shuttle->ring()->push(desc)) {
            return PorthStatus::SUCCESS;
        }
        return PorthStatus::FULL;
    }

    /**
     * @brief receive: Retrieves a descriptor processed by the hardware.
     * @param out_desc Reference to store the popped descriptor.
     * @return PorthStatus::SUCCESS on success, PorthStatus::EMPTY if no data.
     */
    [[nodiscard]] PorthStatus receive(PorthDescriptor& out_desc) noexcept {
        if (shuttle->ring()->pop(out_desc)) {
            return PorthStatus::SUCCESS;
        }
        return PorthStatus::EMPTY;
    }

    /** @brief Returns the underlying register layout. */
    [[nodiscard]] PorthDeviceLayout* get_regs() const noexcept { return regs; }

    /** @brief Returns the shuttle management object. */
    [[nodiscard]] PorthShuttle<RingSize>* get_shuttle() const noexcept { return shuttle.get(); }
};

} // namespace porth