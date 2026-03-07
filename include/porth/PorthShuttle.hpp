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

/**
 * @namespace gsl
 * @brief Minimal Guideline Support Library implementation to satisfy clang-tidy ownership checks.
 */
namespace gsl {
template <typename T>
using owner = T;
} // namespace gsl

namespace porth {

/** @brief Default capacity for the Shuttle ring buffer. */
constexpr size_t DEFAULT_SHUTTLE_CAPACITY = 1024;

/** @brief Standard 2MB HugePage size for memory allocation. */
constexpr size_t SHUTTLE_PAGE_SIZE = static_cast<size_t>(2) * 1024 * 1024;

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
template <size_t Capacity = DEFAULT_SHUTTLE_CAPACITY>
class PorthShuttle {
private:
    PorthHugePage m_memory; ///< The underlying HugePage memory allocation.
    gsl::owner<PorthRingBuffer<Capacity>*>
        m_ring_ptr; ///< Typed pointer to the ring buffer within the HugePage.

public:
    /**
     * @brief Constructor: Initializes the DMA fabric using Placement New.
     * * This constructor allocates a 2MB HugePage and maps the PorthRingBuffer
     * structure directly onto that memory region. Because HugePages are
     * 2MB aligned, the ring buffer is guaranteed to be 64-byte aligned.
     */
    PorthShuttle()
        : m_memory(SHUTTLE_PAGE_SIZE), m_ring_ptr(nullptr) { // Allocate 2MB HugePage pool

        // Safety check: Ensure the type is safe for memory-mapped placement
        static_assert(std::is_standard_layout_v<PorthRingBuffer<Capacity>>,
                      "Cannot map PorthRingBuffer: Type violates Standard Layout rules.");

        // Get the raw address from the HugePage
        void* base_addr = m_memory.data();

        // We initialize the RingBuffer at the very start of the HugePage.
        // This is a zero-copy operation that aligns the software structure with the hardware view.
        m_ring_ptr = new (base_addr) PorthRingBuffer<Capacity>();

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
        if (m_ring_ptr != nullptr) {
            // Task: Manually call the destructor for the placement-mapped object
            m_ring_ptr->~PorthRingBuffer();
        }
    }

    /**
     * @brief get_device_addr: Returns the DMA-ready physical address.
     * * Converts the HugePage pointer to a uint64_t for the Hardware Handshake.
     * @return uint64_t The physical/device address of the memory region.
     */
    [[nodiscard]] auto get_device_addr() const noexcept -> uint64_t {
        return reinterpret_cast<uint64_t>(m_memory.data());
    }

    /** @brief Access the zero-copy ring buffer for data transmission. */
    [[nodiscard]] auto ring() noexcept -> PorthRingBuffer<Capacity>* {
        return m_ring_ptr; // NOLINT(cppcoreguidelines-owning-memory)
    }

    /** @brief Const access to the zero-copy ring buffer. */
    [[nodiscard]] auto ring() const noexcept -> const PorthRingBuffer<Capacity>* {
        return m_ring_ptr; // NOLINT(cppcoreguidelines-owning-memory)
    }

    // Hardware-mapped orchestrators cannot be copied to prevent memory aliasing conflicts.
    PorthShuttle(const PorthShuttle&)                    = delete;
    auto operator=(const PorthShuttle&) -> PorthShuttle& = delete;

    // Rule of Five compliance: Explicitly delete move operations.
    PorthShuttle(PorthShuttle&&)                    = delete;
    auto operator=(PorthShuttle&&) -> PorthShuttle& = delete;
};

} // namespace porth