#pragma once

#include <pthread.h>
#include <sched.h>
#include <iostream>
#include <string>

namespace porth {

/**
 * PorthStatus: Explicit status codes for high-speed I/O operations.
 * Replaces boolean returns for better error tracing in production.
 */
enum class PorthStatus {
    SUCCESS = 0,
    BUSY,
    EMPTY,
    FULL,
    ERROR_AFFINITY,
    ERROR_PRIORITY,
    ERROR_HARDWARE_TIMEOUT
};

/**
 * pin_thread_to_core: Locks a thread to a specific CPU core.
 * Essential for the "Jitter Shield" to prevent OS context switching.
 */
inline PorthStatus pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

    if (result != 0) {
        std::cerr << "[Porth-Util] Warning: Could not pin thread to core " << core_id << std::endl;
        return PorthStatus::ERROR_AFFINITY;
    }
    
    std::cout << "[Porth-Util] Thread successfully pinned to core " << core_id << std::endl;
    return PorthStatus::SUCCESS;
}

/**
 * set_realtime_priority: Elevates thread to SCHED_FIFO.
 * Ensures the Porth driver isn't interrupted by background system tasks.
 */
inline PorthStatus set_realtime_priority() {
    struct sched_param param;
    param.sched_priority = 99; // Highest possible FIFO priority

    int result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (result != 0) {
        std::cerr << "[Porth-Util] Warning: Could not set SCHED_FIFO (requires sudo/root)" << std::endl;
        return PorthStatus::ERROR_PRIORITY;
    }

    std::cout << "[Porth-Util] Thread priority elevated to Real-Time (SCHED_FIFO)" << std::endl;
    return PorthStatus::SUCCESS;
}

} // namespace porth