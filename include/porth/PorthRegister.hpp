/**
 * @file PorthClock.hpp
 * @brief Hardware-abstracted access to high-precision CPU cycle counters.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <type_traits>
#include <atomic>
#include <cstdint>

namespace porth {

/**
 * @class PorthRegister
 * @brief The atomic "brick" of the Porth-IO Hardware Abstraction Layer.
 *
 * Enforces 64-byte cache alignment to prevent false sharing and utilizes
 * Acquire/Release semantics for jitter-free hardware synchronization.
 *
 * @tparam T The integral type of the register (must be trivially copyable).
 */
template <typename T>
class alignas(64) PorthRegister {
    // Task 1.3: Compile-Time Verification
    static_assert(std::is_integral<T>::value, "PorthRegister only accepts integer types.");
    static_assert(std::is_trivially_copyable<T>::value, "PorthRegister types must be trivially copyable for MMIO.");

private:
    std::atomic<T> value; ///< The underlying atomic register value.
    
    /** * @brief Explicit padding to ensure each register occupies exactly one cache line.
     * Prevents "False Sharing" where multiple registers inhabit the same cache line,
     * which would introduce latency spikes during high-frequency hardware updates.
     */
    unsigned char padding[64 - sizeof(std::atomic<T>)];

public: 
    /** @brief Default constructor for register initialization in mapped memory. */
    PorthRegister() = default;

    // Hardware registers represent unique physical locations and cannot be copied or moved.
    PorthRegister(const PorthRegister&) = delete;
    PorthRegister& operator=(const PorthRegister&) = delete;

    /**
     * @brief Reads the register value using Acquire semantics.
     * * Ensures all previous hardware writes (e.g., from the Newport chip) 
     * are visible to the CPU before the read operation completes.
     * * @return The current value of the register.
     */
    [[nodiscard]] T load() const noexcept {
        return value.load(std::memory_order_acquire);
    }

    /**
     * @brief Writes a value to the register using Release semantics.
     * * Ensures that any data buffers prepared by the CPU are committed 
     * to RAM before the register update signals the hardware to begin processing.
     * * @param val The value to write to the register.
     */
    void write(T val) noexcept {
        value.store(val, std::memory_order_release);
    }

    /** @brief Overload for assignment to allow intuitive 'reg = val' syntax. */
    T operator=(T val) noexcept {
        write(val);
        return val;
    }

    /** @brief Overload for conversion to allow intuitive 'val = reg' syntax. */
    operator T() const noexcept {
        return load();
    }

    // Task 1.3: Ensure the architecture supports lock-free atomics for the chosen type.
    static_assert(std::atomic<T>::is_always_lock_free, "PorthRegister must be lock-free on this architecture.");
};

} // namespace porth