/**
 * @file porth_bench.cpp
 * @brief Automated high-frequency benchmark suite for the Newport Cluster.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/porth/PorthClock.hpp"
#include "../include/porth/PorthMetric.hpp"
#include "../include/porth/PorthUtil.hpp"
#include <format>
#include <iostream>
#include <string>
#include <vector>

/**
 * @brief Task 3.1: Automated Benchmark Suite.
 * Runs multiple iterations to ensure statistical significance across the
 * semiconductor execution fabric.
 */
int main() {
    using namespace porth;

    // Setup environment: Enforce thread affinity to isolate jitter
    if (pin_thread_to_core(1) != PorthStatus::SUCCESS) {
        std::cerr
            << "[Warning] Could not pin benchmark thread. Results may show OS-induced jitter.\n";
    }

    const double cycles_per_ns = 2.4; // Targeted calibration for Newport hardware
    const int iterations       = 10;
    const int samples_per_run  = 100000;

    PorthMetric global_metric(iterations * samples_per_run);

    std::cout << std::format("[Bench] Starting Newport Cluster Speed Test ({} iterations)...\n",
                             iterations);

    for (int run = 0; run < iterations; ++run) {
        for (int i = 0; i < samples_per_run; ++i) {
            const uint64_t t1 = PorthClock::now_precise();

/**
 * @note Simulating Porth Register Read.
 * This provides a baseline for the logic-layer overhead by
 * executing an architectural synchronization barrier (ISB)
 * followed by a cycle counter read on ARMv8-A.
 */
#if defined(__aarch64__)
            asm volatile("isb; mrs x0, cntvct_el0" ::: "x0");
#elif defined(__x86_64__)
            asm volatile("pause" ::: "memory");
#endif

            const uint64_t t2 = PorthClock::now_precise();
            global_metric.record(t2 - t1);
        }
        std::cout << std::format("  - Iteration {} complete.\n", run + 1);
    }

    // Task 3.2: Final Statistical Analysis
    global_metric.print_stats(cycles_per_ns);

    // Task 3.3: Generate Automated Sovereign Report
    std::cout << "[Report] Appending results to BENCHMARKS.md...\n";
    global_metric.save_markdown_report(
        "BENCHMARKS.md", "Porth-IO Newport Production Driver", cycles_per_ns);

    return 0;
}