#include "../include/porth/PorthClock.hpp"
#include "../include/porth/PorthMetric.hpp"
#include <thread>

int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset); // Lock to Core 1
    
    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "[Warning] Could not pin thread to core. Run with sudo?" << std::endl;
    } else {
        std::cout << "[Shield] Thread successfully locked to Core 1." << std::endl;
    }
    
    using namespace porth;
    
    // 1. Setup
    const double cycles_per_ns = 0.024; // Use the value from your calibration!
    PorthMetric metric(1000000);

    std::cout << "[Driver] Running 1,000,000 sample stress test..." << std::endl;

    // 2. The Measurement Loop
    for(int i = 0; i < 1000000; ++i) {
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