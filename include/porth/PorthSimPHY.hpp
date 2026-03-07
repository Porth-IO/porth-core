/**
 * @file PorthSimPHY.hpp
 * @brief Hardware-abstracted access to high-precision CPU cycle counters.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "PorthClock.hpp"
#include <algorithm>
#include <atomic>
#include <random>

namespace porth {

/** @brief PHY Simulation Constants to resolve magic number warnings. */
constexpr double DEFAULT_FEC_ERROR_RATE   = 0.001;
constexpr uint64_t DEFAULT_FEC_PENALTY_NS = 5;
constexpr uint32_t DEFAULT_BASE_TEMP_MC   = 25000;
constexpr uint64_t DEFAULT_BASE_DELAY_NS  = 100;
constexpr uint64_t DEFAULT_JITTER_INIT_NS = 25;
constexpr double DEFAULT_CPNS             = 2.4;
constexpr uint32_t THERMAL_THRESHOLD_MC   = 40000;
constexpr uint32_t MC_TO_C_DIVISOR        = 1000;
constexpr uint64_t FEC_RETRY_SPIKE_NS     = 500;

/**
 * @class PorthSimPHY
 * @brief Emulates PCIe physical layer effects for compound semiconductor interconnects.
 * * This class implements the Task 4.3 Thermal/Power Feedback Loop, providing a
 * high-fidelity simulation of propagation delays, jitter, and signal degradation
 * caused by physics-level fluctuations in InP/GaN hardware.
 */
class PorthSimPHY {
private:
    uint64_t m_base_delay_ns; ///< Base propagation delay in nanoseconds.
    uint64_t m_jitter_ns;     ///< Maximum configurable jitter.
    double m_cycles_per_ns;   ///< Calibrated CPU cycles per nanosecond.

    // Task 2.2: FEC Simulation constants
    double m_fec_error_rate =
        DEFAULT_FEC_ERROR_RATE; ///< Probability of a Forward Error Correction retry.
    uint64_t m_fec_penalty_ns =
        DEFAULT_FEC_PENALTY_NS; ///< Base latency penalty for FEC processing.

    // Task 4.3: Thermal Feedback (milli-Celsius)
    std::atomic<uint32_t> m_current_temp_mc{DEFAULT_BASE_TEMP_MC};

    std::mt19937 m_gen;
    std::uniform_int_distribution<int64_t> m_jitter_dist;
    std::uniform_real_distribution<double> m_error_dist;

    /** @brief Calculates the added jitter based on current laser temperature. */
    [[nodiscard]] auto calculate_thermal_jitter() const noexcept -> uint64_t {
        uint64_t thermal_jitter = 0;
        const uint32_t temp     = m_current_temp_mc.load(std::memory_order_relaxed);

        if (temp > THERMAL_THRESHOLD_MC) {
            thermal_jitter = (temp - THERMAL_THRESHOLD_MC) / MC_TO_C_DIVISOR;
        }
        return thermal_jitter;
    }

    /** @brief Checks for a simulated FEC retry event and returns the spike penalty. */
    [[nodiscard]] auto get_fec_penalty() noexcept -> uint64_t {
        if (m_error_dist(m_gen) < m_fec_error_rate) {
            return FEC_RETRY_SPIKE_NS;
        }
        return 0;
    }

    /** @brief Architecture-specific CPU hint to yield execution during busy-wait. */
    inline void cpu_relax() const noexcept {
#if defined(__i386__) || defined(__x86_64__)
        asm volatile("pause" ::: "memory");
#elif defined(__aarch64__)
        asm volatile("isb" ::: "memory");
#endif
    }

public:
    /**
     * @brief Construct the PHY emulator with specific performance targets.
     * @param base_ns The base floor for signal propagation.
     * @param jitter_init The peak-to-peak jitter range.
     * @param cpns Clock calibration factor (Cycles per Nanosecond).
     */
    explicit PorthSimPHY(
        uint64_t base_ns = DEFAULT_BASE_DELAY_NS, // NOLINT(bugprone-easily-swappable-parameters)
        uint64_t jitter_init = DEFAULT_JITTER_INIT_NS,
        double cpns          = DEFAULT_CPNS)
        : m_base_delay_ns(base_ns), m_jitter_ns(jitter_init), m_cycles_per_ns(cpns),
          m_gen(std::random_device{}()),
          m_jitter_dist(-static_cast<int64_t>(jitter_init), static_cast<int64_t>(jitter_init)),
          m_error_dist(0.0, 1.0) {}

    /**
     * @brief update_thermal_load: Allows the SimDevice to push temperature updates.
     * @param temp_mc Current laser temperature in milli-Celsius.
     */
    auto update_thermal_load(uint32_t temp_mc) noexcept -> void {
        m_current_temp_mc.store(temp_mc, std::memory_order_relaxed);
    }

    /**
     * @brief apply_protocol_delay: Busy-waits to simulate hardware propagation time.
     * High-level orchestrator that combines thermal, jitter, and FEC models.
     */
    auto apply_protocol_delay() noexcept -> void {
        // 1. Accumulate total propagation delay
        const int64_t random_jitter = (m_jitter_ns > 0) ? m_jitter_dist(m_gen) : 0;

        uint64_t total_delay_ns =
            m_base_delay_ns + m_fec_penalty_ns + static_cast<uint64_t>(random_jitter);

        // 2. Add physics-based penalties
        total_delay_ns += calculate_thermal_jitter();
        total_delay_ns += get_fec_penalty();

        // 3. Convert to cycles and execute high-precision wait
        const auto target_cycles =
            static_cast<uint64_t>(static_cast<double>(total_delay_ns) * m_cycles_per_ns);

        const uint64_t start_cycles = PorthClock::now_precise();
        while (PorthClock::now_precise() - start_cycles < target_cycles) {
            cpu_relax();
        }
    }

    /**
     * @brief Hot-swappable configuration for different hardware scenarios.
     * @param base New base delay in ns.
     * @param jitter New maximum jitter in ns.
     */
    auto set_config(uint64_t base,
                    uint64_t jitter) -> void { // NOLINT(bugprone-easily-swappable-parameters)
        m_base_delay_ns = base;
        m_jitter_ns     = jitter;
        m_jitter_dist   = std::uniform_int_distribution<int64_t>(-static_cast<int64_t>(jitter),
                                                               static_cast<int64_t>(jitter));
    }
};

} // namespace porth