/**
 * @file PorthShuttle.hpp
 * @brief Hardware-abstracted access to high-precision CPU cycle counters.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "PorthHugePage.hpp"
#include "PorthRingBuffer.hpp"
#include <cstdint>
#include <format>
#include <iostream>
#include <new>

namespace porth {

/**
 * @class PorthShuttle
 * @brief The Zero-Copy Orchestrator for high-performance DMA.
 *
 * Handles the "Placement New" logic to ensure the RingBuffer lives inside
 * pinned HugePage memory. This eliminates memory copy overhead between
 * the hardware logic layer and user-space applications.
 *
 * @tparam Capacity The number of descriptors in the ring. Defaults to 1024.
 */
template <size_t Capacity = 1024>
class PorthShuttle {
private:
    PorthHugePage memory;                ///< The underlying HugePage memory allocation.
    PorthRingBuffer<Capacity>* ring_ptr; ///< Typed pointer to the ring buffer within the HugePage.

public:
    /**
     * @brief Constructor: Initializes the DMA fabric using Placement New.
     * * This constructor allocates a 2MB HugePage and maps the PorthRingBuffer
     * structure directly onto that memory region. Because HugePages are
     * 2MB aligned, the ring buffer is guaranteed to be 64-byte aligned.
     */
    PorthShuttle() : memory(2 * 1024 * 1024) { // Allocate 2MB HugePage pool

        // Task 2.4: Get the raw address from the HugePage
        void* base_addr = memory.data();

        // We initialize the RingBuffer at the very start of the HugePage.
        // This is a zero-copy operation that aligns the software structure with the hardware view.
        ring_ptr = new (base_addr) PorthRingBuffer<Capacity>();

        // Professional logging using C++23 std::format for zero-jitter reporting
        std::cout << std::format("[Porth-Shuttle] Zero-Copy Placement New successful at: {}\n",
                                 base_addr);
    }

    /**
     * @brief Destructor: Manually releases the placement-new object.
     * * Since we used placement new, we must explicitly invoke the destructor
     * of the PorthRingBuffer before the PorthHugePage object cleans up the
     * underlying memory mapping.
     */
    ~PorthShuttle() {
        if (ring_ptr) {
            // Task: Manually call the destructor for the placement-mapped object
            ring_ptr->~PorthRingBuffer();
        }
    }

    /**
     * @brief get_device_addr: Returns the DMA-ready physical address.
     * * Converts the HugePage pointer to a uint64_t for the Hardware Handshake.
     * @return uint64_t The physical/device address of the memory region.
     */
    [[nodiscard]] uint64_t get_device_addr() const noexcept {
        return reinterpret_cast<uint64_t>(memory.data());
    }

    /** @brief Access the zero-copy ring buffer for data transmission. */
    [[nodiscard]] PorthRingBuffer<Capacity>* ring() noexcept { return ring_ptr; }

    /** @brief Const access to the zero-copy ring buffer. */
    [[nodiscard]] const PorthRingBuffer<Capacity>* ring() const noexcept { return ring_ptr; }

    // Hardware-mapped orchestrators cannot be copied to prevent memory aliasing conflicts.
    PorthShuttle(const PorthShuttle&)            = delete;
    PorthShuttle& operator=(const PorthShuttle&) = delete;
};

} // namespace porth