/**
 * @file PorthDriver.hpp
 * @brief Orchestration layer for Newport Cluster hardware control and data planes.
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

/** * @brief Default capacity for the DMA ring buffer.
 * Set to 1024 to balance memory pressure with throughput burst capabilities.
 */
constexpr size_t DEFAULT_RING_SIZE = 1024;

/**
 * @class Driver
 * @brief The high-level Master Driver for the Newport Cluster.
 *
 * This class encapsulates the HAL (Registers) and the Shuttle (Data Plane).
 * It manages the lifecycle of the memory fabric and automates the hardware
 * handshake during initialization.
 *
 * @tparam RingSize Depth of the DMA ring buffer. Must be a power of two to allow
 * bitwise masking for index wrapping, eliminating costly division cycles.
 */
template <size_t RingSize = DEFAULT_RING_SIZE>
class Driver {
private:
    /** @brief Pointer to memory-mapped hardware registers.
     * Expects a pointer within a UIO/VFIO mapped region.
     */
    PorthDeviceLayout* m_regs;

    /** @brief The zero-copy memory shuttle.
     * Managed by unique_ptr to ensure the HugePage memory is unmapped
     * only when the driver is destroyed (RAII).
     */
    std::unique_ptr<PorthShuttle<RingSize>> m_shuttle;

public:
    /**
     * @brief Constructor: Orchestrates the Logic-to-Physical Handshake.
     * * Initializes the zero-copy Shuttle and immediately commits the physical
     * memory address to the hardware's DMA engine.
     * * @param hardware_regs Pointer to the device layout (typically from mmap).
     * @throws std::runtime_error If hardware_regs is null, preventing a segfault
     * during the mandatory hardware handshake.
     */
    explicit Driver(PorthDeviceLayout* hardware_regs) : m_regs(hardware_regs) {

        if (m_regs == nullptr) {
            throw std::runtime_error(
                "Porth-Driver: Hardware registers pointer is null. Initialization aborted.");
        }

        // Initialize the Shuttle: Allocates HugePage memory and constructs the PorthRingBuffer.
        m_shuttle = std::make_unique<PorthShuttle<RingSize>>();

        // Automated Handshake:
        // Commit the 64-bit physical DMA address of the Shuttle to the hardware.
        // Once written, the Newport chip's DMA scheduler begins polling this address.
        const uint64_t dma_addr = m_shuttle->get_device_addr();
        m_regs->data_ptr.write(dma_addr);

        // Telemetry logging using C++23 std::format for compile-time string safety.
        std::cout << std::format(
            "[Porth-Driver] Handshake Complete. Shuttle address 0x{:x} committed to hardware.\n",
            dma_addr);
    }

    /**
     * @brief transmit: Submits a descriptor to the hardware via the Zero-Copy Shuttle.
     * * @param desc The descriptor to push to hardware.
     * @return PorthStatus::SUCCESS on success, PorthStatus::FULL if the ring is
     * saturated and cannot accept new work without dropping packets.
     * * @note This operation is non-blocking. If the ring is full, the caller
     * must implement a backoff strategy or drop the payload to maintain latency.
     */
    [[nodiscard]] auto transmit(const PorthDescriptor& desc) noexcept -> PorthStatus {
        if (m_shuttle->ring()->push(desc)) {
            return PorthStatus::SUCCESS;
        }
        return PorthStatus::FULL;
    }

    /**
     * @brief receive: Retrieves a descriptor processed by the hardware.
     * * @param out_desc Reference to store the popped descriptor.
     * @return PorthStatus::SUCCESS on success, PorthStatus::EMPTY if the
     * hardware has not yet returned any completed work.
     * * @note Polling this method is the preferred way to handle high-frequency
     * event loops in the Sovereign Logic Layer.
     */
    [[nodiscard]] auto receive(PorthDescriptor& out_desc) noexcept -> PorthStatus {
        if (m_shuttle->ring()->pop(out_desc)) {
            return PorthStatus::SUCCESS;
        }
        return PorthStatus::EMPTY;
    }

    /** @brief Returns the underlying register layout for direct telemetry access. */
    [[nodiscard]] auto get_regs() const noexcept -> PorthDeviceLayout* { return m_regs; }

    /** @brief Returns the shuttle management object for manual memory auditing. */
    [[nodiscard]] auto get_shuttle() const noexcept -> PorthShuttle<RingSize>* {
        return m_shuttle.get();
    }
};

} // namespace porth