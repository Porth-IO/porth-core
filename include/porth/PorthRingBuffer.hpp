/**
 * @file PorthRingBuffer.hpp
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <vector>

/**
 * @namespace gsl
 * @brief Minimal Guideline Support Library implementation to satisfy clang-tidy ownership checks.
 */
namespace gsl {
template <typename T>
using owner = T;
} // namespace gsl

namespace porth {

/** @brief Standard cache line size for padding and alignment. */
constexpr size_t RING_CACHE_LINE_SIZE = 64;

/** @brief Default ring buffer capacity. */
constexpr size_t DEFAULT_RING_CAPACITY = 1024;

/**
 * @struct PorthDescriptor
 * @brief The "Cargo" box for DMA transactions.
 * * This represents a DMA-ready memory region that the Cardiff chip
 * can read from or write to directly without CPU intervention.
 */
struct PorthDescriptor {
    uint64_t addr; ///< Physical or Virtual memory address.
    uint32_t len;  ///< Length of the data buffer in bytes.
};

/**
 * @class PorthRingBuffer
 * @brief A high-performance SPSC Lock-Free Queue.
 *
 * Optimized for Zero-Copy data transfer between Hardware and Application.
 * This implementation enforces strict cache-line separation between the
 * producer (head) and consumer (tail) to eliminate cache-coherency bottlenecks.
 *
 * @tparam SIZE Number of descriptors. Must be a power of two for bitwise optimization.
 */
template <size_t SIZE = DEFAULT_RING_CAPACITY>
class PorthRingBuffer {
    // We enforce power-of-two for the bitwise wrap-around trick
    static_assert((SIZE & (SIZE - 1)) == 0, "SIZE must be a power of two!");

private:
    /** * @brief Pointer to the underlying descriptor array.
     * Fixed: Added gsl::owner to satisfy clang-tidy memory ownership requirements.
     */
    gsl::owner<PorthDescriptor*> buffer;
    bool owns_buffer; ///< RAII flag for memory lifecycle management.

    // Cache-line 1: The Producer's territory (Typically the Chip or TX side)
    alignas(RING_CACHE_LINE_SIZE) std::atomic<uint32_t> head{0};
    std::array<uint8_t, RING_CACHE_LINE_SIZE - sizeof(std::atomic<uint32_t>)> pad0{};

    // Cache-line 2: The Consumer's territory (Typically the Driver or RX side)
    alignas(RING_CACHE_LINE_SIZE) std::atomic<uint32_t> tail{0};
    std::array<uint8_t, RING_CACHE_LINE_SIZE - sizeof(std::atomic<uint32_t>)> pad1{};

public:
    /**
     * @brief Constructor: Can wrap existing memory (like a HugePage) or allocate its own.
     * @param external_buffer Optional pointer to pre-allocated hardware-visible memory.
     */
    explicit PorthRingBuffer(PorthDescriptor* external_buffer = nullptr)
        : buffer(external_buffer),
          owns_buffer(external_buffer == nullptr) { // NOLINT(cppcoreguidelines-owning-memory)
        if (owns_buffer) {
            buffer = new PorthDescriptor[SIZE];
        }
    }

    /**
     * @brief Destructor: Ensures owned memory is released back to the system.
     */
    ~PorthRingBuffer() {
        if (owns_buffer) {
            delete[] buffer;
        }
    }

    /**
     * @brief push() - Executed by the Producer.
     * * Adds a new descriptor to the ring. Uses Release semantics to ensure
     * the descriptor data is visible to the consumer before the index update.
     * * @param desc The descriptor to push.
     * @return true if successful, false if the ring is full.
     */
    [[nodiscard]] auto push(const PorthDescriptor& desc) noexcept -> bool {
        const uint32_t h = head.load(std::memory_order_relaxed);
        const uint32_t t = tail.load(std::memory_order_acquire);

        // Check if full: next head would hit tail
        if (((h + 1) & (SIZE - 1)) == t) {
            return false;
        }

        // Write the data to the current head slot
        buffer[h] = desc;

        // Release: Ensures buffer write is visible before the head index moves
        head.store((h + 1) & (SIZE - 1), std::memory_order_release);
        return true;
    }

    /**
     * @brief pop() - Executed by the Consumer.
     * * Retrieves a descriptor from the ring. Uses Acquire semantics to
     * ensure the data is fully visible before processing begins.
     * * @param out_desc Reference to store the retrieved descriptor.
     * @return true if data was retrieved, false if the ring is empty.
     */
    [[nodiscard]] auto pop(PorthDescriptor& out_desc) noexcept -> bool {
        const uint32_t t = tail.load(std::memory_order_relaxed);
        const uint32_t h = head.load(std::memory_order_acquire);

        // Check if empty: tail caught up to head
        if (h == t) {
            return false;
        }

        // Read the data from the current tail slot
        out_desc = buffer[t];

        // Release: Signals to producer that this slot is now free for new data
        tail.store((t + 1) & (SIZE - 1), std::memory_order_release);
        return true;
    }

    // Hardware-visible memory structures are non-copyable to maintain logical identity.
    PorthRingBuffer(const PorthRingBuffer&)                    = delete;
    auto operator=(const PorthRingBuffer&) -> PorthRingBuffer& = delete;

    // Rule of Five compliance: Delete move operations
    PorthRingBuffer(PorthRingBuffer&&)                    = delete;
    auto operator=(PorthRingBuffer&&) -> PorthRingBuffer& = delete;
};

} // namespace porth