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
    std::vector<uint64_t> m_samples; ///< Pre-allocated sample buffer.
    size_t m_capacity;               ///< Maximum number of samples.
    size_t m_count = 0;              ///< Current number of recorded samples.

    /** @brief Prepares a sorted copy of the recorded samples for analysis. */
    [[nodiscard]] auto get_sorted_samples() const -> std::vector<uint64_t> {
        std::vector<uint64_t> sorted(m_samples.begin(),
                                     m_samples.begin() + static_cast<std::ptrdiff_t>(m_count));
        std::ranges::sort(sorted);
        return sorted;
    }

    /** @brief Calculates the arithmetic mean of the sample set. */
    [[nodiscard]] auto calculate_mean(const std::vector<uint64_t>& samples) const noexcept
        -> double {
        const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        return sum / static_cast<double>(m_count);
    }

    /** @brief Calculates the standard deviation using the inner product for precision. */
    [[nodiscard]] auto calculate_stdev(const std::vector<uint64_t>& samples,
                                       double mean) const noexcept -> double {
        const double sq_sum =
            std::inner_product(samples.begin(), samples.end(), samples.begin(), 0.0);
        return std::sqrt(std::abs((sq_sum / static_cast<double>(m_count)) - (mean * mean)));
    }

    /** @brief Retrieves a specific percentile and converts it to nanoseconds. */
    [[nodiscard]] auto get_percentile_ns(const std::vector<uint64_t>& samples,
                                         double percentile,
                                         double cpns) const noexcept -> double {
        auto idx = static_cast<size_t>(percentile * static_cast<double>(m_count) / 100.0);
        if (idx >= m_count) {
            idx = m_count - 1;
        }
        return static_cast<double>(samples[idx]) / cpns;
    }

    /**
     * @brief Internal helper to write the formatted markdown table to a stream.
     * Separates formatting logic from file I/O for better testing and lower complexity.
     */
    void write_markdown_table(std::ostream& out,
                              const std::string& label,
                              double min_ns,
                              double median_ns,
                              double p999_ns,
                              double max_ns) const {
        out << "### Benchmark: " << label << "\n";
        out << "| Metric | Latency (ns) |\n";
        out << "| :--- | :--- |\n";
        out << std::format("| Minimum | {:.2f} |\n", min_ns);
        out << std::format("| Median (P50) | {:.2f} |\n", median_ns);
        out << std::format("| P99.9 | {:.2f} |\n", p999_ns);
        out << std::format("| Maximum | {:.2f} |\n\n", max_ns);
    }

public:
    /**
     * @brief Construct a new Metric engine with a fixed capacity.
     * @param max_samples Total samples to pre-allocate.
     */
    explicit PorthMetric(size_t max_samples = DEFAULT_METRIC_CAPACITY) : m_capacity(max_samples) {
        m_samples.resize(m_capacity, 0);
    }

    /**
     * @brief record(): Stores a latency sample. No allocations here.
     * * This is the hot-path for telemetry. It performs a bounds check
     * and a direct array write to ensure zero-jitter recording.
     * * @param latency The raw cycle count delta to record.
     */
    void record(uint64_t latency) noexcept {
        if (m_count < m_capacity) {
            m_samples[m_count++] = latency;
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

        for (size_t i = 0; i < m_count; ++i) {
            out << i << " " << m_samples[i] << "\n";
        }
    }

    /**
     * @brief print_stats(): Analyzes the collected data and outputs a jitter report.
     * Orchestrates data sorting, statistical modeling, and C++23 formatted output.
     */
    void print_stats(double cycles_per_ns) {
        if (m_count == 0)
            return;

        // 1. Prepare Data
        const auto sorted_samples = get_sorted_samples();

        // 2. Compute Statistics (in cycles)
        const double mean_cycles  = calculate_mean(sorted_samples);
        const double stdev_cycles = calculate_stdev(sorted_samples, mean_cycles);

        // 3. Compute Percentiles (converted to ns)
        const double q1_ns  = get_percentile_ns(sorted_samples, PERCENTILE_Q1, cycles_per_ns);
        const double q3_ns  = get_percentile_ns(sorted_samples, PERCENTILE_Q3, cycles_per_ns);
        const double p99_ns = get_percentile_ns(sorted_samples, PERCENTILE_P99_99, cycles_per_ns);

        // 4. Final Jitter Analysis Output
        std::cout << "\n--- Porth-IO Jitter Analysis (ns) ---\n";
        std::cout << std::format("Mean:    {:.2f} ns\n", mean_cycles / cycles_per_ns);
        std::cout << std::format("StDev:   {:.2f} ns\n", stdev_cycles / cycles_per_ns);
        std::cout << std::format("IQR:     {:.2f} ns\n", q3_ns - q1_ns);
        std::cout << std::format("P99.99:  {:.2f} ns\n", p99_ns);
    }

    /**
     * @brief save_markdown_report(): Generates an automated summary table for documentation.
     * * Appends a benchmark summary to a markdown file, ideal for CI/CD artifacts.
     * * @param filename File path to append to.
     * @param label The name of the benchmark run.
     * @param cycles_per_ns Frequency for cycle-to-ns conversion.
     */
    void save_markdown_report(const std::string& filename,
                              const std::string& label,
                              double cycles_per_ns) {
        std::ofstream out(filename, std::ios::app);
        if (!out.is_open() || m_count == 0) {
            return;
        }

        // 1. Prepare Data using existing helpers
        const auto sorted_samples = get_sorted_samples();

        // 2. Extract specific metrics required for the report
        const double min_ns    = get_percentile_ns(sorted_samples, 0.0, cycles_per_ns);
        const double median_ns = get_percentile_ns(sorted_samples, PERCENTILE_P50, cycles_per_ns);
        const double p999_ns   = get_percentile_ns(sorted_samples, PERCENTILE_P99_9, cycles_per_ns);
        const double max_ns    = get_percentile_ns(sorted_samples, 100.0, cycles_per_ns);

        // 3. Delegate the formatting and writing
        write_markdown_table(out, label, min_ns, median_ns, p999_ns, max_ns);
    }

    /** @brief Resets the sample count for a new run. */
    void reset() noexcept { m_count = 0; }
};

} // namespace porth