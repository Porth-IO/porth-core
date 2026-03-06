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

#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace porth {

/** @brief Standard cache line size to avoid magic number warnings. */
constexpr size_t CACHE_LINE_SIZE = 64;

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
class alignas(CACHE_LINE_SIZE) PorthRegister {
    // Task 1.3: Compile-Time Verification
    static_assert(std::is_integral_v<T>, "PorthRegister only accepts integer types.");
    static_assert(std::is_trivially_copyable_v<T>,
                  "PorthRegister types must be trivially copyable for MMIO.");

private:
    std::atomic<T> value; ///< The underlying atomic register value.

    /** * @brief Explicit padding to ensure each register occupies exactly one cache line.
     * Prevents "False Sharing" where multiple registers inhabit the same cache line,
     * which would introduce latency spikes during high-frequency hardware updates.
     */
    std::array<unsigned char, CACHE_LINE_SIZE - sizeof(std::atomic<T>)> padding{};

public:
    /** @brief Default constructor for register initialization in mapped memory. */
    PorthRegister() = default;

    /** @brief Destructor. */
    ~PorthRegister() = default;

    // Hardware registers represent unique physical locations and cannot be copied or moved.
    PorthRegister(const PorthRegister&)                    = delete;
    auto operator=(const PorthRegister&) -> PorthRegister& = delete;

    PorthRegister(PorthRegister&&)                    = delete;
    auto operator=(PorthRegister&&) -> PorthRegister& = delete;

    /**
     * @brief Reads the register value using Acquire semantics.
     * * Ensures all previous hardware writes (e.g., from the Newport chip)
     * are visible to the CPU before the read operation completes.
     * * @return The current value of the register.
     */
    [[nodiscard]] auto load() const noexcept -> T { return value.load(std::memory_order_acquire); }

    /**
     * @brief Writes a value to the register using Release semantics.
     * * Ensures that any data buffers prepared by the CPU are committed
     * to RAM before the register update signals the hardware to begin processing.
     * * @param val The value to write to the register.
     */
    auto write(T val) noexcept -> void { value.store(val, std::memory_order_release); }

    /** @brief Overload for assignment to allow intuitive 'reg = val' syntax. */
    auto operator=(T val) noexcept -> PorthRegister& {
        write(val);
        return *this;
    }

    /** @brief Overload for conversion to allow intuitive 'val = reg' syntax. */
    operator T() const noexcept { return load(); }

    // Task 1.3: Ensure the architecture supports lock-free atomics for the chosen type.
    static_assert(std::atomic<T>::is_always_lock_free,
                  "PorthRegister must be lock-free on this architecture.");
};

} // namespace porth