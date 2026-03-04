#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cmath>

namespace porth {

class PorthMetric {
private:
    std::vector<uint64_t> samples;
    size_t capacity;
    size_t count = 0;

public:
    explicit PorthMetric(size_t max_samples = 1000000) : capacity(max_samples) {
        // PRE-ALLOCATION: We reserve memory now so we don't do it during the test.
        samples.resize(capacity, 0);
    }

    /**
     * record(): Stores a latency sample.
     * CRITICAL: No allocations, no locks, no console output here.
     */
    inline void record(uint64_t latency) {
        if (count < capacity) {
            samples[count++] = latency;
        }
    }

    /**
     * print_stats(): Analyzes the collected data.
     */
    void print_stats(double cycles_per_ns) {
        if (count == 0) return;

        // Sort only the recorded portion
        std::sort(samples.begin(), samples.begin() + count);

        auto get_p = [&](double percentile) {
            size_t idx = static_cast<size_t>(percentile * count / 100.0);
            if (idx >= count) idx = count - 1;
            return samples[idx] / cycles_per_ns;
        };

        std::cout << "\n--- Latency Distribution (ns) ---" << std::endl;
        std::cout << "Count:  " << count << std::endl;
        std::cout << "Min:    " << (samples[0] / cycles_per_ns) << " ns" << std::endl;
        std::cout << "P50:    " << get_p(50) << " ns (Median)" << std::endl;
        std::cout << "P90:    " << get_p(90) << " ns" << std::endl;
        std::cout << "P99:    " << get_p(99) << " ns" << std::endl;
        std::cout << "P99.9:  " << get_p(99.9) << " ns" << std::endl;
        std::cout << "P99.99: " << get_p(99.99) << " ns (The Tail)" << std::endl;
        std::cout << "Max:    " << (samples[count-1] / cycles_per_ns) << " ns" << std::endl;
    }

    /**
     * save_to_file(): Exports data for Gnuplot visualization.
     */
    void save_to_file(const std::string& filename) {
        std::ofstream out(filename);
        for (size_t i = 0; i < count; ++i) {
            out << i << " " << samples[i] << "\n";
        }
        std::cout << "[Info] Samples saved to " << filename << std::endl;
    }
};

} // namespace porth