#include "../include/porth/PorthClock.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    using namespace porth;
    using namespace std::chrono;

    std::cout << "--- Porth-Metric: Task 1.2 Calibration ---" << std::endl;

    // 1. CALIBRATION: Measure TSC frequency
    auto start_time       = steady_clock::now();
    uint64_t start_cycles = PorthClock::now_precise();

    // Sleep for 1 second to get a stable frequency reading
    std::this_thread::sleep_for(seconds(1));

    uint64_t end_cycles = PorthClock::now_precise();
    auto end_time       = steady_clock::now();

    auto elapsed_ns         = duration_cast<nanoseconds>(end_time - start_time).count();
    uint64_t elapsed_cycles = end_cycles - start_cycles;

    double cycles_per_ns = static_cast<double>(elapsed_cycles) / elapsed_ns;

    std::cout << "[Info] TSC Frequency: " << (cycles_per_ns * 1000) << " MHz" << std::endl;
    std::cout << "[Info] Cycles per Nanosecond: " << cycles_per_ns << std::endl;

    // 2. TEST: Measure the overhead of the clock itself
    std::vector<uint64_t> samples;
    samples.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        PorthClock::fence();
        uint64_t t1 = PorthClock::now_precise();

        // Measuring "nothing" to find the base overhead

        uint64_t t2 = PorthClock::now_precise();
        PorthClock::fence();
        samples.push_back(t2 - t1);
    }

    std::sort(samples.begin(), samples.end());
    uint64_t median_cycles = samples[500];

    std::cout << "[Success] Clock Overhead: ~" << (median_cycles / cycles_per_ns) << " ns ("
              << median_cycles << " cycles)" << std::endl;

    return 0;
}