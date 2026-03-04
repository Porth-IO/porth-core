#pragma once

#include <type_traits>
#include <atomic>
#include <cstdint>

namespace porth {

/**
 * @class PorthRegister
 * @brief The atomic "brick" of the Porth-IO Hardware Abstraction Layer.
 * * Enforces 64-byte cache alignment to prevent false sharing and utilizes
 * Acquire/Release semantics for jitter-free hardware synchronization.
 * * @tparam T The integral type of the register (must be trivially copyable).
 */
template <typename T>
class alignas(64) PorthRegister {
    // Task 1.3: Compile-Time Verification
    static_assert(std::is_integral<T>::value, "PorthRegister only accepts integer types.");
    static_assert(std::is_trivially_copyable<T>::value, "PorthRegister types must be trivially copyable for MMIO.");

private:
    std::atomic<T> value;
    unsigned char padding[64 - sizeof(std::atomic<T>)];

public: 
    PorthRegister() = default;

    // Hardware registers cannot be copied or moved.
    PorthRegister(const PorthRegister&) = delete;
    PorthRegister& operator=(const PorthRegister&) = delete;

    /**
     * @brief Reads the register value using Acquire semantics.
     * Ensures all previous hardware writes are visible before the read.
     * @return The current value of the register.
     */
    T load() const {
        return value.load(std::memory_order_acquire);
    }

    /**
     * @brief Writes a value to the register using Release semantics.
     * Ensures data buffers are committed to RAM before the register update.
     * @param val The value to write.
     */
    void write(T val) {
        value.store(val, std::memory_order_release);
    }

    static_assert(std::atomic<T>::is_always_lock_free, "PorthRegister must be lock-free on this architecture.");
};

} // namespace porth