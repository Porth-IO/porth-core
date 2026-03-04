#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cmath>
#include <numeric>
#include <iomanip>

namespace porth {

/**
 * PorthMetric: Statistical analysis engine for ultra-low latency timing.
 * Supports standard metrics, jitter analysis, and automated markdown reporting.
 */
class PorthMetric {
private:
    std::vector<uint64_t> samples;
    size_t capacity;
    size_t count = 0;

public:
    explicit PorthMetric(size_t max_samples = 1000000) : capacity(max_samples) {
        samples.resize(capacity, 0);
    }

    /**
     * record(): Stores a latency sample. No allocations here.
     */
    inline void record(uint64_t latency) {
        if (count < capacity) {
            samples[count++] = latency;
        }
    }

    /**
     * save_to_file(): Legacy support for raw data export.
     */
    void save_to_file(const std::string& filename) {
        std::ofstream out(filename);
        for (size_t i = 0; i < count; ++i) {
            out << i << " " << samples[i] << "\n";
        }
    }

    /**
     * print_stats(): Analyzes the collected data for Mean, StDev, and IQR.
     */
    void print_stats(double cycles_per_ns) {
        if (count == 0) return;

        std::vector<uint64_t> sorted_samples(samples.begin(), samples.begin() + count);
        std::sort(sorted_samples.begin(), sorted_samples.end());

        double sum = std::accumulate(sorted_samples.begin(), sorted_samples.end(), 0.0);
        double mean = sum / count;
        double sq_sum = std::inner_product(sorted_samples.begin(), sorted_samples.end(), sorted_samples.begin(), 0.0);
        double stdev = std::sqrt(std::abs(sq_sum / count - mean * mean));

        auto get_p = [&](double percentile) {
            size_t idx = static_cast<size_t>(percentile * count / 100.0);
            if (idx >= count) idx = count - 1;
            return sorted_samples[idx] / cycles_per_ns;
        };

        double q1 = get_p(25);
        double q3 = get_p(75);
        double iqr = q3 - q1;

        std::cout << "\n--- Porth-IO Jitter Analysis (ns) ---" << std::endl;
        std::cout << "Mean:   " << std::fixed << std::setprecision(2) << (mean / cycles_per_ns) << " ns" << std::endl;
        std::cout << "StDev:  " << (stdev / cycles_per_ns) << " ns" << std::endl;
        std::cout << "IQR:    " << iqr << " ns" << std::endl;
        std::cout << "P99.99: " << get_p(99.99) << " ns" << std::endl;
    }

    /**
     * save_markdown_report(): Generates an automated summary table for documentation.
     */
    void save_markdown_report(const std::string& filename, const std::string& label, double cycles_per_ns) {
        std::ofstream out(filename, std::ios::app);
        if (!out.is_open() || count == 0) return;

        std::vector<uint64_t> sorted_samples(samples.begin(), samples.begin() + count);
        std::sort(sorted_samples.begin(), sorted_samples.end());

        auto get_p = [&](double percentile) {
            size_t idx = static_cast<size_t>(percentile * count / 100.0);
            if (idx >= count) idx = count - 1;
            return sorted_samples[idx] / cycles_per_ns;
        };

        out << "### Benchmark: " << label << "\n";
        out << "| Metric | Latency (ns) |\n";
        out << "| :--- | :--- |\n";
        out << "| Minimum | " << (sorted_samples[0] / cycles_per_ns) << " |\n";
        out << "| Median (P50) | " << get_p(50) << " |\n";
        out << "| P99.9 | " << get_p(99.9) << " |\n";
        out << "| Maximum | " << (sorted_samples[count-1] / cycles_per_ns) << " |\n\n";
    }
};

} // namespace porth