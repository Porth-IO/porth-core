/**
 * @file posix_baseline.cpp
 * @brief Baseline performance measurement of the standard POSIX networking stack.
 *
 * Porth-IO: The Sovereign Logic Layer
 * This utility quantifies the "Legacy Tax" of kernel-space networking to provide
 * a reference point for the Newport Cluster's zero-copy performance.
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthClock.hpp"
#include "porth/PorthMetric.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief Main execution for the POSIX 'sendto' benchmark.
 * Measures the round-trip latency of the standard kernel stack.
 */
auto main() -> int {
    using namespace porth;

    // 1. Setup Metrics (Matches your OrbStack 24MHz calibration)
    const double cycles_per_ns = 0.0240002;
    constexpr int iterations   = 100000;
    PorthMetric std_metric(static_cast<std::size_t>(iterations));

    // 2. Create a standard UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Set up a loopback address (127.0.0.1)
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(8888);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    std::cout << "[Baseline] Measuring " << iterations << " POSIX 'sendto' calls..." << '\n';

    for (int i = 0; i < iterations; ++i) {
        const std::uint64_t t1 = PorthClock::now_precise();

        /**
         * THE SLOW PATH:
         * Safety: reinterpret_cast is required for the legacy POSIX sockaddr API.
         * We move the cast to a variable to prevent clang-format from breaking the suppression.
         */

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto* target_addr = reinterpret_cast<const struct sockaddr*>(&addr);

        (void)sendto(sock, "ping", 4, 0, target_addr, sizeof(addr));

        const std::uint64_t t2 = PorthClock::now_precise();
        std_metric.record(t2 - t1);
    }

    std_metric.print_stats(cycles_per_ns);
    (void)close(sock);

    return 0;
}