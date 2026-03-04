#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "../include/porth/PorthClock.hpp"
#include "../include/porth/PorthMetric.hpp"

int main() {
    using namespace porth;

    // 1. Setup Metrics (Matches your OrbStack 24MHz calibration)
    const double cycles_per_ns = 0.0240002; 
    PorthMetric std_metric(100000); 

    // 2. Create a standard UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Set up a loopback address (127.0.0.1)
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    std::cout << "[Baseline] Measuring 100,000 POSIX 'sendto' calls..." << std::endl;

    for(int i = 0; i < 100000; ++i) {
        uint64_t t1 = PorthClock::now_precise();
        
        // This is the "Slow Path":
        // 1. Context Switch to Kernel
        // 2. Copy data from User space to Kernel space
        // 3. Traverse the IP stack
        sendto(sock, "ping", 4, 0, (struct sockaddr*)&addr, sizeof(addr));
        
        uint64_t t2 = PorthClock::now_precise();
        std_metric.record(t2 - t1);
    }

    std_metric.print_stats(cycles_per_ns);
    close(sock);

    return 0;
}