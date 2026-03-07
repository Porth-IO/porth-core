#include "porth/PorthClock.hpp"
#include "porth/PorthMetric.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

auto main() -> int {
    using namespace porth;

    // 1. Setup Metrics (Matches your OrbStack 24MHz calibration)
    const double cycles_per_ns = 0.0240002;
    constexpr int iterations   = 100000;
    PorthMetric std_metric(static_cast<size_t>(iterations));

    // 2. Create a standard UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Set up a loopback address (127.0.0.1)
    // Value-initialize the struct to satisfy cppcoreguidelines-pro-type-member-init
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(8888);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    std::cout << "[Baseline] Measuring " << iterations << " POSIX 'sendto' calls..." << '\n';

    for (int i = 0; i < iterations; ++i) {
        uint64_t t1 = PorthClock::now_precise();

        // This is the "Slow Path":
        // 1. Context Switch to Kernel
        // 2. Copy data from User space to Kernel space
        // 3. Traverse the IP stack
        // Fix: Use reinterpret_cast instead of C-style cast
        sendto(sock, "ping", 4, 0, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));

        uint64_t t2 = PorthClock::now_precise();
        std_metric.record(t2 - t1);
    }

    std_metric.print_stats(cycles_per_ns);
    close(sock);

    return 0;
}