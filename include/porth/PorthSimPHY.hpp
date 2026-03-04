#pragma once

#include <random>
#include <atomic>
#include "PorthClock.hpp"

namespace porth {

/**
 * PorthSimPHY: Emulates PCIe physical layer effects.
 * Task 4.3: Implements Thermal/Power Feedback Loop.
 */
class PorthSimPHY {
private:
    uint64_t base_delay_ns;
    uint64_t jitter_ns;
    double cycles_per_ns;
    
    // Task 2.2: FEC Simulation constants
    double fec_error_rate = 0.001; 
    uint64_t fec_penalty_ns = 5;    

    // Task 4.3: Thermal Feedback
    std::atomic<uint32_t> current_temp_mc{25000}; // milli-Celsius

    std::mt19937 gen;
    std::uniform_int_distribution<int64_t> jitter_dist;
    std::uniform_real_distribution<double> error_dist;

public:
    PorthSimPHY(uint64_t base_ns = 100, uint64_t jitter_ns = 25, double cpns = 2.4)
        : base_delay_ns(base_ns), jitter_ns(jitter_ns), cycles_per_ns(cpns),
          gen(std::random_device{}()), 
          jitter_dist(-static_cast<int64_t>(jitter_ns), static_cast<int64_t>(jitter_ns)),
          error_dist(0.0, 1.0) {}

    /**
     * update_thermal_load: Allows the SimDevice to push temperature updates.
     */
    void update_thermal_load(uint32_t temp_mc) {
        current_temp_mc.store(temp_mc);
    }

    void apply_protocol_delay() {
        // Task 4.3: Calculate thermal jitter multiplier
        // Jitter increases by 1ns for every 1 degree above 40C
        uint64_t thermal_jitter = 0;
        uint32_t temp = current_temp_mc.load();
        if (temp > 40000) {
            thermal_jitter = (temp - 40000) / 1000;
        }

        int64_t current_jitter = (jitter_ns > 0) ? jitter_dist(gen) : 0;
        uint64_t total_delay_ns = base_delay_ns + current_jitter + fec_penalty_ns + thermal_jitter;

        // Simulate FEC retry spike
        if (error_dist(gen) < fec_error_rate) {
            total_delay_ns += 500; 
        }
        
        uint64_t target_cycles = static_cast<uint64_t>(total_delay_ns * cycles_per_ns);
        uint64_t start = PorthClock::now();

        while (PorthClock::now() - start < target_cycles) {
            #if defined(__i386__) || defined(__x86_64__)
                asm volatile("pause" ::: "memory");
            #endif
        }
    }

    void set_config(uint64_t base, uint64_t jitter) {
        base_delay_ns = base;
        jitter_ns = jitter;
        jitter_dist = std::uniform_int_distribution<int64_t>(-static_cast<int64_t>(jitter), static_cast<int64_t>(jitter));
    }
};

} // namespace porth