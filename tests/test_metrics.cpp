#include "../include/porth/PorthClock.hpp"
#include "../include/porth/PorthMetric.hpp"
#include <thread>

auto main() -> int {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset); // Lock to Core 1

    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "[Warning] Could not pin thread to core. Run with sudo?" << '\n';
    } else {
        std::cout << "[Shield] Thread successfully locked to Core 1." << '\n';
    }

    using namespace porth;

    // 1. Setup
    const double cycles_per_ns = 0.024; // Use the value from your calibration!
    constexpr int sample_count = 1000000;
    PorthMetric metric(static_cast<size_t>(sample_count));

    std::cout << "[Driver] Running 1,000,000 sample stress test..." << '\n';

    // 2. The Measurement Loop
    for (int i = 0; i < sample_count; ++i) {
        uint64_t t1 = PorthClock::now_precise();

// Simulating a tiny bit of "work" (like a register read)
#if defined(__aarch64__)
        asm volatile("nop");
#endif

        uint64_t t2 = PorthClock::now_precise();

        // Record the delta
        metric.record(t2 - t1);
    }

    // 3. Results
    metric.print_stats(cycles_per_ns);
    metric.save_to_file("latency_test.dat");

    return 0;
}