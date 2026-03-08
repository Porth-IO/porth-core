/**
 * @file calibration_main.cpp
 * @brief Hardware-software timing synchronization and clock-overhead calibration utility.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthClock.hpp"
#include <algorithm>
#include <bits/chrono.h>
#include <cstdint>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

/**
 * @brief Calibration Utility for the Sovereign Logic Layer.
 * * This program establishes the physical baseline for all Porth-IO timing. It maps
 * abstract CPU cycles to real-world nanoseconds by benchmarking the Time Stamp
 * Counter (TSC) against the system's steady_clock.
 */
auto main() -> int {
    using namespace porth;
    using namespace std::chrono;

    // Unit conversion: 1 GHz = 1000 MHz. Used to present frequency telemetry.
    constexpr double mhz_conversion = 1000.0;

    // Sample count selected to provide a statistically significant median (P50)
    // without triggering excessive cache misses in the results vector.
    constexpr int sample_count = 1000;
    constexpr int median_index = 500;

    std::cout << "--- Porth-Metric: Task 1.2 Calibration ---" << '\n';

    // 1. CALIBRATION: Synchronize Software Logic with Hardware Cycles.
    // We sample the precise hardware cycle count before and after a known
    // wall-clock interval to derive the frequency constant.
    auto start_time       = steady_clock::now();
    uint64_t start_cycles = PorthClock::now_precise();

    // Justification: 1 second is the minimum stable window to minimize the
    // impact of the OS scheduler's sleep jitter on frequency calculation.
    std::this_thread::sleep_for(seconds(1));

    uint64_t end_cycles = PorthClock::now_precise();
    auto end_time       = steady_clock::now();

    auto elapsed_ns         = duration_cast<nanoseconds>(end_time - start_time).count();
    uint64_t elapsed_cycles = end_cycles - start_cycles;

    // Derived Constant: This factor is used by PorthMetric to convert cycle
    // deltas into physical time units (ns) for the Newport Cluster reports.
    double cycles_per_ns = static_cast<double>(elapsed_cycles) / static_cast<double>(elapsed_ns);

    std::cout << "[Info] TSC Frequency: " << (cycles_per_ns * mhz_conversion) << " MHz" << '\n';
    std::cout << "[Info] Cycles per Nanosecond: " << cycles_per_ns << '\n';

    // 2. OVERHEAD TEST: Quantify the Logic Layer's "Measurement Tax."
    // Every call to now_precise() consumes cycles. To provide accurate
    // telemetry, we must identify this floor and subtract it from results.
    std::vector<uint64_t> samples;
    samples.reserve(sample_count);

    for (int i = 0; i < sample_count; ++i) {
        // Fence: Prevents the Out-of-Order (OoO) engine from speculatively
        // executing the timing reads together, which would result in 0 cycles.
        PorthClock::fence();
        uint64_t t1 = PorthClock::now_precise();

        // Measuring the execution delta of the clock mechanism itself.

        uint64_t t2 = PorthClock::now_precise();
        PorthClock::fence();
        samples.push_back(t2 - t1);
    }

    // Sort to extract the P50 (Median). We prefer Median over Mean here to
    // eliminate the influence of OS-induced timing spikes (Sovereign Outliers).
    std::ranges::sort(samples);
    uint64_t median_cycles = samples[median_index];

    // Final Verification: Converts the cycle tax into nanoseconds to ensure
    // the measurement overhead is significantly below the expected logic latency.
    std::cout << "[Success] Clock Overhead: ~"
              << (static_cast<double>(median_cycles) / cycles_per_ns) << " ns (" << median_cycles
              << " cycles)" << '\n';

    return 0;
}