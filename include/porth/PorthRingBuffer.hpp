#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace porth {
    
/**
 * PorthDescriptor: The "Cargo" box.
 * This represents a DMA-ready memory region that the Cardiff chip
 * can read from or write to directly.
 */
struct PorthDescriptor {
    uint64_t addr; // Physical or Virtual memory address
    uint32_t len;  // Length of the data buffer
};

/**
 * PorthRingBuffer: A high-performance SPSC Lock-Free Queue.
 * Optimized for Zero-Copy data transfer between Hardware and Application.
 */
template <size_t SIZE = 1024>
class PorthRingBuffer {
    // We enforce power-of-two for the bitwise wrap-around trick
    static_assert((SIZE & (SIZE - 1)) == 0, "SIZE must be a power of two!");

private:
    PorthDescriptor* buffer;
    bool owns_buffer;

    // Cache-line 1: The Producer's territory
    alignas(64) std::atomic<uint32_t> head{0};
    uint8_t _pad0[64 - sizeof(std::atomic<uint32_t>)];

    // Cache-line 2: The Consumer's territory
    alignas(64) std::atomic<uint32_t> tail{0};
    uint8_t _pad1[64 - sizeof(std::atomic<uint32_t>)];

public:
    /**
     * Constructor: Can wrap existing memory (like a HugePage) or allocate its own.
     */
    explicit PorthRingBuffer(PorthDescriptor* external_buffer = nullptr) 
        : buffer(external_buffer), owns_buffer(external_buffer == nullptr) {
        if (owns_buffer) {
            buffer = new PorthDescriptor[SIZE];
        }
    }

    ~PorthRingBuffer() {
        if (owns_buffer) delete[] buffer;
    }

    /**
     * push() - Executed by the Producer (the "Chip")
     * Adds a new descriptor to the ring.
     */
    inline bool push(const PorthDescriptor& desc) {
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
     * pop() - Executed by the Consumer (the "Driver")
     * Retrieves a descriptor from the ring.
     */
    inline bool pop(PorthDescriptor& out_desc) {
        const uint32_t t = tail.load(std::memory_order_relaxed);
        const uint32_t h = head.load(std::memory_order_acquire);

        // Check if empty: tail caught up to head
        if (h == t) {
            return false;
        }

        // Read the data from the current tail slot
        out_desc = buffer[t];

        // Release: Signals to producer that this slot is now free
        tail.store((t + 1) & (SIZE - 1), std::memory_order_release);
        return true;
    }
};

} // namespace porth