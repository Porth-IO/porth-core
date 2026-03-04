#include <iostream>
#include <vector>
#include <string>
#include "../include/porth/PorthClock.hpp"
#include "../include/porth/PorthMetric.hpp"
#include "../include/porth/PorthUtil.hpp"

/**
 * Task 3.1: Automated Benchmark Suite.
 * Runs multiple iterations to ensure statistical significance.
 */
int main() {
    using namespace porth;
    
    // Setup environment
    pin_thread_to_core(1);
    const double cycles_per_ns = 2.4; // Targeted calibration
    const int iterations = 10;
    const int samples_per_run = 100000;

    PorthMetric global_metric(iterations * samples_per_run);

    std::cout << "[Bench] Starting Newport Cluster Speed Test (" << iterations << " iterations)..." << std::endl;

    for (int run = 0; run < iterations; ++run) {
        for (int i = 0; i < samples_per_run; ++i) {
            uint64_t t1 = PorthClock::now_precise();
            
            // Simulating Porth Register Read
            #if defined(__aarch64__)
                asm volatile("isb; mrs x0, cntvct_el0" ::: "x0"); 
            #endif
            
            uint64_t t2 = PorthClock::now_precise();
            global_metric.record(t2 - t1);
        }
        std::cout << "  - Iteration " << (run + 1) << " complete." << std::endl;
    }

    // Task 3.2: Final Analysis
    global_metric.print_stats(cycles_per_ns);

    // Task 3.3: Generate Automated Report
    std::cout << "[Report] Appending results to BENCHMARKS.md..." << std::endl;
    global_metric.save_markdown_report("BENCHMARKS.md", "Porth-IO Newport Production Driver", cycles_per_ns);

    return 0;
}