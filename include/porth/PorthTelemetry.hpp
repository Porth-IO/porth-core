#pragma once
#include <atomic>
#include <cstdint>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace porth {

struct PorthStats {
    std::atomic<uint64_t> total_packets{0};
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<uint64_t> dropped_packets{0};
    std::atomic<uint32_t> current_temp_mc{0};
    std::atomic<uint64_t> last_latency_ns{0};
    std::atomic<uint32_t> max_temp_mc{0};
};

class PorthTelemetryHub {
private:
    std::string m_name;
    PorthStats* m_stats{nullptr};
    bool m_owner;

public:
    explicit PorthTelemetryHub(const std::string& name, bool create = true)
        : m_name("/" + name), m_owner(create) {

        int flags = O_RDWR | (create ? O_CREAT : 0);
        int fd    = shm_open(m_name.c_str(), flags, 0666);
        if (fd == -1)
            throw std::runtime_error("Telemetry SHM failed");

        if (create) {
            // 1. Set the size of the shared memory block
            if (ftruncate(fd, sizeof(PorthStats)) == -1) {
                close(fd);
                throw std::runtime_error("Failed to size telemetry SHM");
            }

            // 2. Change permissions so non-root users can read/write.
            // This allows porth_stat (standard user) to talk to the Driver (root).
            if (fchmod(fd, 0666) == -1) {
                close(fd);
                throw std::runtime_error("Failed to set SHM permissions");
            }
        }

        void* ptr = mmap(nullptr, sizeof(PorthStats), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        m_stats = static_cast<PorthStats*>(ptr);
        if (create)
            new (m_stats) PorthStats(); // Placement new to init atomics
    }

    ~PorthTelemetryHub() {
        munmap(m_stats, sizeof(PorthStats));
        if (m_owner)
            shm_unlink(m_name.c_str());
    }

    auto view() noexcept -> PorthStats* { return m_stats; }
};

} // namespace porth