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
    uint64_t base_delay_ns; ///< Base propagation delay in nanoseconds.
    uint64_t jitter_ns;     ///< Maximum configurable jitter.
    double cycles_per_ns;   ///< Calibrated CPU cycles per nanosecond.

    // Task 2.2: FEC Simulation constants
    double fec_error_rate =
        DEFAULT_FEC_ERROR_RATE; ///< Probability of a Forward Error Correction retry.
    uint64_t fec_penalty_ns = DEFAULT_FEC_PENALTY_NS; ///< Base latency penalty for FEC processing.

    // Task 4.3: Thermal Feedback (milli-Celsius)
    std::atomic<uint32_t> current_temp_mc{DEFAULT_BASE_TEMP_MC};

    std::mt19937 gen;
    std::uniform_int_distribution<int64_t> jitter_dist;
    std::uniform_real_distribution<double> error_dist;

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
        : base_delay_ns(base_ns), jitter_ns(jitter_init), cycles_per_ns(cpns),
          gen(std::random_device{}()),
          jitter_dist(-static_cast<int64_t>(jitter_init), static_cast<int64_t>(jitter_init)),
          error_dist(0.0, 1.0) {}

    /**
     * @brief update_thermal_load: Allows the SimDevice to push temperature updates.
     * @param temp_mc Current laser temperature in milli-Celsius.
     */
    auto update_thermal_load(uint32_t temp_mc) noexcept -> void {
        current_temp_mc.store(temp_mc, std::memory_order_relaxed);
    }

    /**
     * @brief apply_protocol_delay: Busy-waits to simulate hardware propagation time.
     * * This method implements the physics-aware delay logic:
     * 1. Calculates thermal jitter (1ns increase per 1°C above 40°C).
     * 2. Adds random jitter and FEC penalties.
     * 3. Injects a 500ns spike if a simulated FEC error occurs.
     * 4. Performs a high-precision busy-wait using the 'pause' instruction.
     */
    auto apply_protocol_delay() noexcept -> void {
        // Task 4.3: Calculate thermal jitter multiplier
        // Jitter increases by 1ns for every 1 degree above 40C (40000 mC)
        uint64_t thermal_jitter = 0;
        const uint32_t temp     = current_temp_mc.load(std::memory_order_relaxed);
        if (temp > THERMAL_THRESHOLD_MC) {
            thermal_jitter = (temp - THERMAL_THRESHOLD_MC) / MC_TO_C_DIVISOR;
        }

        const int64_t current_jitter = (jitter_ns > 0) ? jitter_dist(gen) : 0;
        uint64_t total_delay_ns =
            base_delay_ns + static_cast<uint64_t>(current_jitter) + fec_penalty_ns + thermal_jitter;

        // Simulate FEC retry spike (Task 2.2)
        if (error_dist(gen) < fec_error_rate) {
            total_delay_ns += FEC_RETRY_SPIKE_NS;
        }

        const auto target_cycles =
            static_cast<uint64_t>(static_cast<double>(total_delay_ns) * cycles_per_ns);
        const uint64_t start = PorthClock::now_precise();

        // High-precision busy wait using architecture-specific hints
        while (PorthClock::now_precise() - start < target_cycles) {
#if defined(__i386__) || defined(__x86_64__)
            asm volatile("pause" ::: "memory");
#elif defined(__aarch64__)
            asm volatile("isb" ::: "memory");
#endif
        }
    }

    /**
     * @brief Hot-swappable configuration for different hardware scenarios.
     * @param base New base delay in ns.
     * @param jitter New maximum jitter in ns.
     */
    auto set_config(uint64_t base, uint64_t jitter)
        -> void { // NOLINT(bugprone-easily-swappable-parameters)
        base_delay_ns = base;
        jitter_ns     = jitter;
        jitter_dist   = std::uniform_int_distribution<int64_t>(-static_cast<int64_t>(jitter),
                                                             static_cast<int64_t>(jitter));
    }
};

} // namespace porth