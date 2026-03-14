/**
 * @file PorthSimPHY.hpp
 * @brief Physics-based emulator for the Newport Cluster physical layer (PHY).
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

/** @brief PHY Simulation Constants for Newport Cluster hardware modeling. */
constexpr double DEFAULT_FEC_ERROR_RATE =
    0.001;                                     ///< 0.1% Bit Error Rate (BER) proxy for FEC retries.
constexpr uint64_t DEFAULT_FEC_PENALTY_NS = 5; ///< Baseline logic delay for FEC decoding (5ns).
constexpr uint32_t DEFAULT_BASE_TEMP_MC =
    25000; ///< 25.0°C - Standard laboratory ambient temperature.
constexpr uint64_t DEFAULT_BASE_DELAY_NS =
    100; ///< Propagation time for light in 20m of optical fiber.
constexpr uint64_t DEFAULT_JITTER_INIT_NS = 25;  ///< Baseline clock recovery jitter.
constexpr double DEFAULT_CPNS             = 2.4; ///< Default calibration for a 2.4GHz CPU.

/** * @brief Physical thermal threshold (40.0°C).
 * Beyond this limit, the Indium Phosphide (InP) lattice drift significantly
 * impacts signal-to-noise ratio (SNR), introducing non-linear jitter.
 */
constexpr uint32_t THERMAL_THRESHOLD_MC = 40000;
constexpr uint32_t MC_TO_C_DIVISOR      = 1000; ///< Conversion factor for milli-Celsius to Celsius.

/** * @brief FEC Spike Penalty (500ns).
 * Simulates a full packet retransmission or deep-interleaving buffer flush
 * triggered by an uncorrectable error in the GaN power stage.
 */
constexpr uint64_t FEC_RETRY_SPIKE_NS = 500;

/**
 * @class PorthSimPHY
 * @brief Emulates PCIe physical layer effects for compound semiconductor interconnects.
 *
 * This class implements the Sovereign thermal/power feedback loop. It provides
 * a high-fidelity simulation of propagation delays and jitter caused by
 * physical fluctuations in the InP/GaN hardware lattice.
 */
class PorthSimPHY {
private:
    uint64_t m_base_delay_ns; ///< Base floor for propagation (nanoseconds).
    uint64_t m_jitter_ns;     ///< Peak-to-peak range of random clock noise.
    double m_cycles_per_ns;   ///< Calibrated CPU frequency factor.

    /** @brief Constants for thermal inertia increments. */
    static constexpr uint32_t HEATING_STEP_MC = 25;
    static constexpr uint32_t COOLING_STEP_MC = 15;

    /** @brief Probability of a Forward Error Correction (FEC) retry. */
    double m_fec_error_rate = DEFAULT_FEC_ERROR_RATE;

    /** @brief Constant logic delay for silicon-level FEC processing. */
    uint64_t m_fec_penalty_ns = DEFAULT_FEC_PENALTY_NS;

    /** @brief Real-time laser temperature.
     * Shared across simulation threads to model thermal-induced signal degradation.
     */
    std::atomic<uint32_t> m_current_temp_mc{DEFAULT_BASE_TEMP_MC};

    std::mt19937 m_gen;
    std::uniform_int_distribution<int64_t> m_jitter_dist;
    std::uniform_real_distribution<double> m_error_dist;

    /** * @brief Physics Model: Calculates jitter based on thermal lattice drift.
     * * Justification: As temperature rises above 40°C, the InP semiconductor
     * bandgap shifts, causing timing uncertainties at the photodetector stage.
     */
    [[nodiscard]] auto calculate_thermal_jitter() const noexcept -> uint64_t {
        uint64_t thermal_jitter = 0;
        const uint32_t temp     = m_current_temp_mc.load(std::memory_order_relaxed);

        if (temp > THERMAL_THRESHOLD_MC) {
            // Linear approximation of jitter increase per degree Celsius above threshold.
            thermal_jitter = (temp - THERMAL_THRESHOLD_MC) / MC_TO_C_DIVISOR;
        }
        return thermal_jitter;
    }

    /** * @brief Error Model: Simulates uncorrectable FEC events.
     * @return uint64_t Penalty in ns (0 if no error).
     */
    [[nodiscard]] auto get_fec_penalty() noexcept -> uint64_t {
        if (m_error_dist(m_gen) < m_fec_error_rate) {
            return FEC_RETRY_SPIKE_NS;
        }
        return 0;
    }

    /** * @brief Performance Guard: CPU Architecture Hint.
     * * Justification: Using architecture-specific relax instructions prevents
     * "Pipeline Sizzling" during busy-wait loops, reducing CPU power consumption
     * and preventing speculative execution from polluting the cache.
     */
    static void cpu_relax() noexcept {
#if defined(__i386__) || defined(__x86_64__)
        // PAUSE: Notifies the CPU that we are in a spin-loop, improving power
        // efficiency and reducing the exit-latency of the loop.
        asm volatile("pause" ::: "memory");
#elif defined(__aarch64__)
        // ISB: Flushes the pipeline on ARM64 to ensure the loop condition is
        // re-evaluated without speculative interference.
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
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    explicit PorthSimPHY(uint64_t base_ns     = DEFAULT_BASE_DELAY_NS,
                         uint64_t jitter_init = DEFAULT_JITTER_INIT_NS,
                         double cpns          = DEFAULT_CPNS)
        : m_base_delay_ns(base_ns), m_jitter_ns(jitter_init), m_cycles_per_ns(cpns),
          m_gen(std::random_device{}()),
          m_jitter_dist(-static_cast<int64_t>(jitter_init), static_cast<int64_t>(jitter_init)),
          m_error_dist(0.0, 1.0) {}

    /**
     * @brief update_thermal_load: Pushes real-time temperature telemetry into the PHY model.
     * @param temp_mc Current laser temperature in milli-Celsius (mC).
     */
    auto update_thermal_load(uint32_t temp_mc) noexcept -> void {
        // Thermal Inertia: Instead of an instant "jump", we model the heat capacity
        // of the InP lattice. The temperature "sloughs" towards the target.
        const uint32_t current = m_current_temp_mc.load(std::memory_order_relaxed);
        if (temp_mc > current) {
            m_current_temp_mc.store(current + HEATING_STEP_MC,
                                    std::memory_order_relaxed); // Gradual heating
        } else if (temp_mc < current) {
            m_current_temp_mc.store(current - COOLING_STEP_MC,
                                    std::memory_order_relaxed); // Gradual cooling
        }
    }

    /** @brief Returns the internal inertial temperature for register syncing. */
    [[nodiscard]] auto get_current_temp() const noexcept -> uint32_t {
        return m_current_temp_mc.load(std::memory_order_relaxed);
    }

    /**
     * @brief apply_protocol_delay: Busy-waits to simulate physical propagation time.
     * * This is the high-level orchestrator for the PHY model. It enforces
     * deterministic latency by summing fiber delay, jitter, thermal drift,
     * and FEC retry penalties.
     * @note Utilizes a sovereign spin-loop to achieve nanosecond resolution
     * that is impossible with standard OS sleep primitives.
     */
    auto apply_protocol_delay() noexcept -> void {
        // 1. Accumulate total propagation delay from the physics model.
        const int64_t random_jitter = (m_jitter_ns > 0) ? m_jitter_dist(m_gen) : 0;

        uint64_t total_delay_ns =
            m_base_delay_ns + m_fec_penalty_ns + static_cast<uint64_t>(random_jitter);

        // 2. Add non-deterministic penalties (Physics-based).
        total_delay_ns += calculate_thermal_jitter();
        total_delay_ns += get_fec_penalty();

        // 3. Convert time to CPU cycles for high-precision wait.
        const auto target_cycles =
            static_cast<uint64_t>(static_cast<double>(total_delay_ns) * m_cycles_per_ns);

        // Sovereign Spin-Loop: Guarantees timing precision to within ~3-5 cycles.
        const uint64_t start_cycles = PorthClock::now_precise();
        while (PorthClock::now_precise() - start_cycles < target_cycles) {
            cpu_relax();
        }
    }

    /**
     * @brief Hot-swappable configuration for different hardware scenarios.
     * @param base New base delay in ns (Fiber length simulation).
     * @param jitter New maximum jitter in ns (Clock noise simulation).
     */
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    auto set_config(uint64_t base, uint64_t jitter) -> void {
        m_base_delay_ns = base;
        m_jitter_ns     = jitter;
        m_jitter_dist   = std::uniform_int_distribution<int64_t>(-static_cast<int64_t>(jitter),
                                                               static_cast<int64_t>(jitter));
    }
};

} // namespace porth