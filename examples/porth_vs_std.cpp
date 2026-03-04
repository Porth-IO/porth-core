#define _GNU_SOURCE
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>

#include "../include/porth/PorthClock.hpp"
#include "../include/porth/PorthMetric.hpp"

using namespace porth;

// Helper to pin the current thread to a specific CPU core
void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

int main() {
    // 1. HARDENING: Pin to Core 1 and use your calibration
    pin_to_core(1);
    const double cycles_per_ns = 0.0240002; 
    const int iterations = 100000;

    PorthMetric std_results(iterations);
    PorthMetric porth_results(iterations);

    std::cout << "--- Porth-IO vs Standard Linux: The Final Exam ---" << std::endl;
    std::cout << "[Shield] Thread locked to Core 1. Starting benchmarks..." << std::endl;

    // 2. PHASE A: Standard POSIX Sockets
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    for(int i = 0; i < iterations; ++i) {
        uint64_t t1 = PorthClock::now_precise();
        sendto(sock, "ping", 4, 0, (struct sockaddr*)&addr, sizeof(addr));
        uint64_t t2 = PorthClock::now_precise();
        std_results.record(t2 - t1);
    }
    close(sock);

    // 3. PHASE B: Porth-IO Hardware Registers (Simulation)
    for(int i = 0; i < iterations; ++i) {
        uint64_t t1 = PorthClock::now_precise();
        // Mimicking a direct hardware register write
        #if defined(__aarch64__)
            asm volatile("isb; mrs x0, cntvct_el0" ::: "x0"); 
        #endif
        uint64_t t2 = PorthClock::now_precise();
        porth_results.record(t2 - t1);
    }

    // 4. THE SPEEDUP TABLE
    std::cout << "\n" << std::left << std::setw(15) << "Metric" 
              << std::setw(20) << "Standard (ns)" 
              << std::setw(20) << "Porth-IO (ns)" 
              << "Speedup" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    auto print_row = [&](const std::string& name, double std_val, double porth_val) {
        double speedup = (porth_val > 0) ? std_val / porth_val : std_val;
        std::cout << std::left << std::setw(15) << name 
                  << std::fixed << std::setprecision(2)
                  << std::setw(20) << std_val 
                  << std::setw(20) << porth_val 
                  << std::fixed << std::setprecision(1) << speedup << "x" << std::endl;
    };

    // Note: We use the logic from PorthMetric to get percentiles
    // For this demo, let's just print the results we've already seen
    std::cout << "[Result] Data analyzed. Review the tail latency below:" << std::endl;
    porth_results.print_stats(cycles_per_ns);
    std_results.print_stats(cycles_per_ns);

    return 0;
}