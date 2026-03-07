/**
 * @file PorthRegister.hpp
 * @brief The atomic "brick" of the Porth-IO Hardware Abstraction Layer.
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
    std::atomic<T> m_value; ///< The underlying atomic register value.

    /** * @brief Explicit padding to ensure each register occupies exactly one cache line.
     * Prevents "False Sharing" where multiple registers inhabit the same cache line,
     * which would introduce latency spikes during high-frequency hardware updates.
     */
    std::array<unsigned char, CACHE_LINE_SIZE - sizeof(std::atomic<T>)> m_padding{};

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
     * @brief Reads the register value using Volatile-Acquire semantics.
     * Ensures the compiler never elides a read and that hardware writes
     * are visible before the CPU continues.
     */
    [[nodiscard]] auto load() const noexcept -> T {
        // 1. Force a volatile read to prevent compiler 'redundant read' optimization
        T val = *reinterpret_cast<const volatile T*>(&m_value);

        // 2. Insert an Acquire fence to ensure subsequent loads are not reordered
        std::atomic_thread_fence(std::memory_order_acquire);
        return val;
    }

    /**
     * @brief Writes a value using Volatile-Release semantics.
     * Ensures the value is committed to the bus and not optimized away.
     */
    auto write(T val) noexcept -> void {
        // 1. Insert a Release fence to ensure prior stores are visible to HW
        std::atomic_thread_fence(std::memory_order_release);

        // 2. Force a volatile write so the hardware always sees the signal
        *reinterpret_cast<volatile T*>(&m_value) = val;
    }

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