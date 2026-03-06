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

#include <cstdint>

// Only include architecture-specific headers if we are on a supported platform
#if defined(__x86_64__) || defined(__i386__)
    #include <x86intrin.h>
#endif

namespace porth {

/**
 * @class PorthClock
 * @brief Provides low-latency access to hardware timing primitives.
 *
 * This class abstracts the differences between Intel TSC (Time Stamp Counter)
 * and the ARM64 Generic Timer (CNTVCT_EL0). It is designed for sub-microsecond
 * latency measurements in HFT and high-speed interconnect environments.
 */
class PorthClock {
public:
    /**
     * @brief Reads the hardware cycle counter without serialization.
     * * This is the fastest method to get a timestamp. Note that on modern
     * out-of-order CPUs, this read may be reordered with respect to nearby
     * instructions. Use now_precise() if strict ordering is required.
     *
     * @return uint64_t Current CPU clock cycles.
     */
    static inline uint64_t now() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        return __rdtsc(); // Intel/AMD Path
#elif defined(__aarch64__)
        uint64_t virtual_timer_value;
        // ARM64 Path: Read the physical count register via system register MRS
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
        return virtual_timer_value;
#else
        #error "Porth-IO: Unsupported CPU architecture for high-precision timing."
#endif
    }

    /**
     * @brief Reads the hardware cycle counter with instruction serialization.
     * * Ensures that all previous instructions have completed before the 
     * counter is read. This is essential for measuring the exact duration
     * of a specific code block.
     *
     * @return uint64_t Current CPU clock cycles (serialized).
     */
    static inline uint64_t now_precise() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        unsigned int aux;
        return __rdtscp(&aux); // Intel Path (RDTSCP is serializing)
#elif defined(__aarch64__)
        uint64_t val;
        // ARM64 Path: ISB (Instruction Synchronization Barrier) ensures 
        // that all previous instructions are completed before the MRS read.
        asm volatile("isb; mrs %0, cntvct_el0" : "=r"(val));
        return val;
#endif
    }

    /**
     * @brief Injects a hardware memory barrier (Fence).
     * * Prevents the CPU and compiler from reordering memory operations
     * across this point. Critical for coordinating between the producer 
     * and consumer in lock-free structures like PorthRingBuffer.
     */
    static inline void fence() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        // LFENCE provides a load fence on x86, preventing instruction reordering.
        asm volatile("lfence" ::: "memory"); 
#elif defined(__aarch64__)
        // ARM64 Path: Data Memory Barrier (Inner Shareable) ensures 
        // global visibility of memory operations across the cluster.
        asm volatile("dmb ish" ::: "memory");
#endif
    }
};

} // namespace porth