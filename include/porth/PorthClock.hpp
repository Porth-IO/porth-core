#pragma once

#include <cstdint>

// Only include x86 headers if we are actually on an x86 machine
#if defined(__x86_64__) || defined(__i386__)
    #include <x86intrin.h>
#endif

namespace porth {

/**
 * PorthClock: Hardware-abstracted access to the CPU cycle counter.
 * Automatically switches between Intel (TSC) and ARM (Generic Timer).
 */
class PorthClock {
public:
    /**
     * now(): Fast, potentially unordered read of the cycle counter.
     */
    static inline uint64_t now() {
#if defined(__x86_64__) || defined(__i386__)
        return __rdtsc(); // Intel Path
#elif defined(__aarch64__)
        uint64_t virtual_timer_value;
        // ARM64 Path: Read the physical count register
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
        return virtual_timer_value;
#else
        #error "Porth-IO: Unsupported CPU architecture for high-precision timing."
#endif
    }

    /**
     * now_precise(): Serializing read of the counter.
     * Ensures all previous instructions are finished before reading.
     */
    static inline uint64_t now_precise() {
#if defined(__x86_64__) || defined(__i386__)
        unsigned int aux;
        return __rdtscp(&aux); // Intel Path
#elif defined(__aarch64__)
        uint64_t val;
        // ARM64 Path: ISB (Instruction Synchronization Barrier) ensures 
        // that all previous instructions are completed before the MRS read.
        asm volatile("isb; mrs %0, cntvct_el0" : "=r"(val));
        return val;
#endif
    }

    /**
     * fence(): Hardware memory barrier to prevent instruction reordering.
     */
    static inline void fence() {
#if defined(__x86_64__) || defined(__i386__)
        asm volatile("lfence" ::: "memory"); // Intel Path
#elif defined(__aarch64__)
        // ARM64 Path: Data Memory Barrier (Inner Shareable)
        asm volatile("dmb ish" ::: "memory");
#endif
    }
};

} // namespace porth