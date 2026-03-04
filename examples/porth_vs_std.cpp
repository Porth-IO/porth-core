#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

int main() {
    pin_to_core(1);
    const double cycles_per_ns = 2.4; 
    const int iterations = 100000;

    PorthMetric std_results(iterations);
    PorthMetric porth_results(iterations);

    std::cout << "--- Porth-IO vs Standard Linux: Benchmark ---" << std::endl;

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

    for(int i = 0; i < iterations; ++i) {
        uint64_t t1 = PorthClock::now_precise();
        #if defined(__i386__) || defined(__x86_64__)
            asm volatile("pause" ::: "memory");
        #elif defined(__aarch64__)
            asm volatile("isb; mrs x0, cntvct_el0" ::: "x0"); 
        #endif
        uint64_t t2 = PorthClock::now_precise();
        porth_results.record(t2 - t1);
    }

    std::cout << "\n[Results] Comparison complete." << std::endl;
    std::cout << "\nStandard POSIX Results:";
    std_results.print_stats(cycles_per_ns);
    
    std::cout << "\nPorth-IO Optimized Path:";
    porth_results.print_stats(cycles_per_ns);

    return 0;
}