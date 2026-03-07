#include "../include/porth/PorthClock.hpp"
#include <algorithm>
#include <bits/chrono.h>
#include <cstdint>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

auto main() -> int {
    using namespace porth;
    using namespace std::chrono;

    // Constants to resolve clang-tidy magic number and conversion warnings
    constexpr double mhz_conversion = 1000.0;
    constexpr int sample_count      = 1000;
    constexpr int median_index      = 500;

    std::cout << "--- Porth-Metric: Task 1.2 Calibration ---" << '\n';

    // 1. CALIBRATION: Measure TSC frequency
    auto start_time       = steady_clock::now();
    uint64_t start_cycles = PorthClock::now_precise();

    // Sleep for 1 second to get a stable frequency reading
    std::this_thread::sleep_for(seconds(1));

    uint64_t end_cycles = PorthClock::now_precise();
    auto end_time       = steady_clock::now();

    auto elapsed_ns         = duration_cast<nanoseconds>(end_time - start_time).count();
    uint64_t elapsed_cycles = end_cycles - start_cycles;

    // Explicit cast of elapsed_ns to double to avoid narrowing conversion warnings
    double cycles_per_ns = static_cast<double>(elapsed_cycles) / static_cast<double>(elapsed_ns);

    std::cout << "[Info] TSC Frequency: " << (cycles_per_ns * mhz_conversion) << " MHz" << '\n';
    std::cout << "[Info] Cycles per Nanosecond: " << cycles_per_ns << '\n';

    // 2. TEST: Measure the overhead of the clock itself
    std::vector<uint64_t> samples;
    samples.reserve(sample_count);

    for (int i = 0; i < sample_count; ++i) {
        PorthClock::fence();
        uint64_t t1 = PorthClock::now_precise();

        // Measuring "nothing" to find the base overhead

        uint64_t t2 = PorthClock::now_precise();
        PorthClock::fence();
        samples.push_back(t2 - t1);
    }

    // Use C++20 ranges version of sort
    std::ranges::sort(samples);
    uint64_t median_cycles = samples[median_index];

    // Cast median_cycles to double to avoid narrowing conversion warnings
    std::cout << "[Success] Clock Overhead: ~"
              << (static_cast<double>(median_cycles) / cycles_per_ns) << " ns (" << median_cycles
              << " cycles)" << '\n';

    return 0;
}