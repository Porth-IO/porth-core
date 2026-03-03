#pragma once

#include <type_traits>
#include <atomic>
#include <cstdint>

/**
 * PorthRegister: The atomic "brick" of the Porth-IO HAL.
 * * Enforces:
 * 1. 64-byte cache alignment to prevent false sharing.
 * 2. Atomic memory ordering (Acquire/Release) for hardware synchronization.
 * 3. Lock-free operations (mandatory for sub-microsecond latency).
 */
template <typename T>
class alignas(64) PorthRegister {
    static_assert(std::is_integral<T>::value, "PorthRegister only accepts integer types!");

private:
    // The hardware value. std::atomic handles compiler/CPU reordering.
    std::atomic<T> value;

    // Explicit padding to ensure the object occupies exactly one 64-byte cache line.
    unsigned char padding[64 - sizeof(std::atomic<T>)];

public: 
    // Constructors & Assignment
    // We default the constructor to keep the class trivial, allowing memory-mapping.
    PorthRegister() = default;


    // Hardware registers cannot be copied or moved.
    PorthRegister(const PorthRegister&) = delete;
    PorthRegister& operator=(const PorthRegister&) = delete;
    PorthRegister(PorthRegister&&) = delete;
    PorthRegister& operator=(PorthRegister&&) = delete;

    /**
     * load() - Reads the register value using Acquire semantics.
     * Ensures all previous hardware writes are visible before the read.
     */
    T load() const {
        return value.load(std::memory_order_acquire);
    }

    /**
     * write() - Writes a value to the register using Release semantics.
     * Ensures any data buffers are committed to RAM before the register update triggers hardware.
     */
    void write(T val) {
        value.store(val, std::memory_order_release);
    }

    // Verification: Ensure we aren't using a heavy-weight atomic with a hidden mutex.
    static_assert(std::atomic<T>::is_always_lock_free, "PorthRegister must be lock-free!");
};

// Global verification to ensure the layout is exactly as expected by the PCIe bus.
static_assert(sizeof(PorthRegister<uint64_t>) == 64, "PorthRegister size must be exactly 64 bytes (1 cache line)");
static_assert(alignof(PorthRegister<uint64_t>) == 64, "PorthRegister must be aligned to 64 bytes");