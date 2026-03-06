/**
 * @file PorthMetric.hpp
 * @brief Hardware-abstracted access to high-precision CPU cycle counters.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace porth {

/** @brief Default capacity for the metric buffer. */
constexpr size_t DEFAULT_METRIC_CAPACITY = 1000000;

/** @brief Percentile constants for statistical analysis. */
constexpr double PERCENTILE_Q1     = 25.0;
constexpr double PERCENTILE_Q3     = 75.0;
constexpr double PERCENTILE_P50    = 50.0;
constexpr double PERCENTILE_P99_9  = 99.9;
constexpr double PERCENTILE_P99_99 = 99.99;

/**
 * @class PorthMetric
 * @brief Statistical analysis engine for ultra-low latency timing.
 * * Supports standard metrics, jitter analysis, and automated markdown reporting.
 * This class uses a pre-allocated buffer to ensure recording samples does not
 * trigger heap allocations, preserving the determinism of the test run.
 */
class PorthMetric {
private:
    std::vector<uint64_t> samples; ///< Pre-allocated sample buffer.
    size_t capacity;               ///< Maximum number of samples.
    size_t count = 0;              ///< Current number of recorded samples.

public:
    /**
     * @brief Construct a new Metric engine with a fixed capacity.
     * @param max_samples Total samples to pre-allocate.
     */
    explicit PorthMetric(size_t max_samples = DEFAULT_METRIC_CAPACITY) : capacity(max_samples) {
        samples.resize(capacity, 0);
    }

    /**
     * @brief record(): Stores a latency sample. No allocations here.
     * * This is the hot-path for telemetry. It performs a bounds check
     * and a direct array write to ensure zero-jitter recording.
     * * @param latency The raw cycle count delta to record.
     */
    void record(uint64_t latency) noexcept {
        if (count < capacity) {
            samples[count++] = latency;
        }
    }

    /**
     * @brief save_to_file(): Legacy support for raw data export.
     * Writes space-separated index and value pairs to a text file.
     * @param filename Path to the output file.
     */
    void save_to_file(const std::string& filename) {
        std::ofstream out(filename);
        if (!out.is_open()) {
            return;
        }

        for (size_t i = 0; i < count; ++i) {
            out << i << " " << samples[i] << "\n";
        }
    }

    /**
     * @brief print_stats(): Analyzes the collected data for Mean, StDev, and IQR.
     * * Converts raw cycles to nanoseconds and outputs a professional jitter report.
     * * @param cycles_per_ns Calibration factor (CPU GHz).
     */
    void print_stats(double cycles_per_ns) {
        if (count == 0) {
            return;
        }

        // Copy and sort for percentile analysis
        std::vector<uint64_t> sorted_samples(samples.begin(),
                                             samples.begin() + static_cast<std::ptrdiff_t>(count));
        std::ranges::sort(sorted_samples);

        // Basic statistics
        const double sum  = std::accumulate(sorted_samples.begin(), sorted_samples.end(), 0.0);
        const double mean = sum / static_cast<double>(count);

        // Standard Deviation using inner product for precision
        const double sq_sum = std::inner_product(
            sorted_samples.begin(), sorted_samples.end(), sorted_samples.begin(), 0.0);
        const double stdev =
            std::sqrt(std::abs((sq_sum / static_cast<double>(count)) - (mean * mean)));

        auto get_p = [&](double percentile) {
            auto idx = static_cast<size_t>(percentile * static_cast<double>(count) / 100.0);
            if (idx >= count) {
                idx = count - 1;
            }
            return static_cast<double>(sorted_samples[idx]) / cycles_per_ns;
        };

        const double q1  = get_p(PERCENTILE_Q1);
        const double q3  = get_p(PERCENTILE_Q3);
        const double iqr = q3 - q1;

        // Immaculate C++23 formatted output
        std::cout << "\n--- Porth-IO Jitter Analysis (ns) ---\n";
        std::cout << std::format("Mean:    {:.2f} ns\n", mean / cycles_per_ns);
        std::cout << std::format("StDev:   {:.2f} ns\n", stdev / cycles_per_ns);
        std::cout << std::format("IQR:     {:.2f} ns\n", iqr);
        std::cout << std::format("P99.99:  {:.2f} ns\n", get_p(PERCENTILE_P99_99));
    }

    /**
     * @brief save_markdown_report(): Generates an automated summary table for documentation.
     * * Appends a benchmark summary to a markdown file, ideal for CI/CD artifacts.
     * * @param filename File path to append to.
     * @param label The name of the benchmark run.
     * @param cycles_per_ns Frequency for cycle-to-ns conversion.
     */
    void save_markdown_report(
        const std::string& filename, // NOLINT(bugprone-easily-swappable-parameters)
        const std::string& label,
        double cycles_per_ns) {
        std::ofstream out(filename, std::ios::app);
        if (!out.is_open() || count == 0) {
            return;
        }

        std::vector<uint64_t> sorted_samples(samples.begin(),
                                             samples.begin() + static_cast<std::ptrdiff_t>(count));
        std::ranges::sort(sorted_samples);

        auto get_p = [&](double percentile) {
            auto idx = static_cast<size_t>(percentile * static_cast<double>(count) / 100.0);
            if (idx >= count) {
                idx = count - 1;
            }
            return static_cast<double>(sorted_samples[idx]) / cycles_per_ns;
        };

        // Formatting strictly according to the "Golden Example" standard
        out << "### Benchmark: " << label << "\n";
        out << "| Metric | Latency (ns) |\n";
        out << "| :--- | :--- |\n";
        out << std::format("| Minimum | {:.2f} |\n",
                           static_cast<double>(sorted_samples[0]) / cycles_per_ns);
        out << std::format("| Median (P50) | {:.2f} |\n", get_p(PERCENTILE_P50));
        out << std::format("| P99.9 | {:.2f} |\n", get_p(PERCENTILE_P99_9));
        out << std::format("| Maximum | {:.2f} |\n\n",
                           static_cast<double>(sorted_samples[count - 1]) / cycles_per_ns);
    }

    /** @brief Resets the sample count for a new run. */
    void reset() noexcept { count = 0; }
};

} // namespace porth