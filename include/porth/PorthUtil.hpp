/**
 * @file PorthUtil.hpp
 * @brief System-level utility functions for thread isolation and RT scheduling.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <pthread.h>
#include <sched.h>
#include <iostream>
#include <string>
#include <format>
#include <cstdint>

namespace porth {

/**
 * @enum PorthStatus
 * @brief Explicit status codes for high-speed I/O operations.
 * * Replaces boolean returns for better error tracing in production environments.
 * These codes allow the Sovereign Logic Layer to communicate specific hardware
 * or OS-level failures back to the financial execution engines.
 */
enum class PorthStatus : uint8_t {
    SUCCESS = 0,             ///< Operation completed without error.
    BUSY,                    ///< Hardware is currently processing.
    EMPTY,                   ///< Ring buffer has no available data.
    FULL,                    ///< Ring buffer is at maximum capacity.
    ERROR_AFFINITY,          ///< Failed to isolate thread on the requested core.
    ERROR_PRIORITY,          ///< Failed to elevate to Real-Time priority.
    ERROR_HARDWARE_TIMEOUT   ///< Hardware failed to respond within the PDK limit.
};

/**
 * @brief pin_thread_to_core: Locks a thread to a specific CPU core.
 * * Essential for the "Jitter Shield" to prevent OS context switching.
 * By pinning the Porth-IO driver thread to a dedicated core, we ensure that
 * the CPU cache remains "warm" with semiconductor logic, minimizing latency.
 * * @param core_id The logical CPU index to pin the calling thread to.
 * @return PorthStatus::SUCCESS or PorthStatus::ERROR_AFFINITY.
 */
[[nodiscard]] inline PorthStatus pin_thread_to_core(int core_id) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

    if (result != 0) {
        // Preserving your specific warning message
        std::cerr << std::format("[Porth-Util] Warning: Could not pin thread to core {}\n", core_id);
        return PorthStatus::ERROR_AFFINITY;
    }
    
    // Preserving your specific success message
    std::cout << std::format("[Porth-Util] Thread successfully pinned to core {}\n", core_id);
    return PorthStatus::SUCCESS;
}

/**
 * @brief set_realtime_priority: Elevates the calling thread to SCHED_FIFO.
 * * Ensures the Porth driver is not interrupted by background system tasks.
 * Sets the scheduling priority to 99 (the highest possible for FIFO), 
 * granting the Sovereign Logic Layer total control over the CPU slice.
 * * @return PorthStatus::SUCCESS or PorthStatus::ERROR_PRIORITY.
 */
[[nodiscard]] inline PorthStatus set_realtime_priority() noexcept {
    struct sched_param param;
    param.sched_priority = 99; // Highest possible FIFO priority

    // Using pthread_setschedparam to maintain compatibility with the affinity logic
    int result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    
    if (result != 0) {
        // Preserving your specific requirement note (sudo/root)
        std::cerr << "[Porth-Util] Warning: Could not set SCHED_FIFO (requires sudo/root)\n";
        return PorthStatus::ERROR_PRIORITY;
    }

    std::cout << "[Porth-Util] Thread priority elevated to Real-Time (SCHED_FIFO)\n";
    return PorthStatus::SUCCESS;
}

} // namespace porth