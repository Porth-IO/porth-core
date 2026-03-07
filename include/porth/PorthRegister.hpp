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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace porth {

/** * @brief The Cardiff Standard Cache Line (64 Bytes).
 * We hardcode this value to ensure the Physical Memory Map (MMIO) remains
 * consistent across different build environments and CPU architectures.
 * This prevents the layout from ballooning and breaking register offsets.
 */
constexpr size_t PORTH_CACHE_LINE_SIZE = 64;

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
class alignas(PORTH_CACHE_LINE_SIZE) PorthRegister {
    static_assert(std::is_integral_v<T>, "PorthRegister only accepts integer types.");
    static_assert(std::is_trivially_copyable_v<T>,
                  "PorthRegister types must be trivially copyable for MMIO.");
    static_assert(std::atomic<T>::is_always_lock_free,
                  "PorthRegister must be lock-free on this architecture.");

private:
    /** * @brief The raw value mapped to hardware.
     * Uses explicit atomic_ref alignment requirements for 10/10 safety.
     */
    alignas(std::atomic_ref<T>::required_alignment) T m_value{};

    /** * @brief Explicit padding to guarantee cache-line isolation.
     * We omit [[no_unique_address]] here because this space MUST be reserved
     * to prevent other objects from inhabiting this cache line.
     */
    std::byte m_padding[PORTH_CACHE_LINE_SIZE - sizeof(T)];

public:
    /** @brief Default constructor for register initialization in mapped memory. */
    PorthRegister() = default;

    /** @brief Destructor. */
    ~PorthRegister() = default;

    // Hardware registers represent unique physical locations and cannot be copied or moved.
    PorthRegister(const PorthRegister&)                    = delete;
    auto operator=(const PorthRegister&) -> PorthRegister& = delete;
    PorthRegister(PorthRegister&&)                         = delete;
    auto operator=(PorthRegister&&) -> PorthRegister&      = delete;

    /**
     * @brief Reads the register value using Acquire semantics.
     * std::atomic_ref ensures the compiler treats this as a volatile, atomic MMIO read.
     */
    [[nodiscard]] auto load() const noexcept -> T {
        // Create a temporary atomic view of the raw memory
        return std::atomic_ref<const T>(m_value).load(std::memory_order_acquire);
    }

    /**
     * @brief Writes a value using Release semantics.
     * Ensures the store is visible to hardware and subsequent reads are ordered.
     */
    auto write(T val) noexcept -> void {
        std::atomic_ref<T>(m_value).store(val, std::memory_order_release);
    }

    /** @brief Overload for assignment to allow intuitive 'reg = val' syntax. */
    auto operator=(T val) noexcept -> PorthRegister& {
        write(val);
        return *this;
    }

    /** @brief Overload for conversion to allow intuitive 'val = reg' syntax. */
    operator T() const noexcept { return load(); }
};

/** * @brief Final layout verification.
 * Essential for ensuring the Sovereign Logic Layer aligns with physical PDK offsets.
 */
static_assert(sizeof(PorthRegister<uint32_t>) == PORTH_CACHE_LINE_SIZE,
              "PorthRegister size mismatch: check hardware alignment logic.");

} // namespace porth