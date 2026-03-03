#pragma once

#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdint>

/**
 * PorthHugePage: Strictly Linux RAII wrapper for HugePage memory.
 * Optimized for InP/GaN DMA transfers.
 */
class PorthHugePage {
private:
    void* ptr = nullptr;
    size_t total_size;

    // 2MB is the standard HugePage size on x86_64 Linux
    static constexpr size_t HP_SIZE = 2 * 1024 * 1024;

public:
    /**
     * Constructor: Allocates pinned memory using MAP_HUGETLB.
     * @param size The requested size (will be rounded up to 2MB boundary).
     */
    explicit PorthHugePage(size_t size) {
        // Round up to nearest 2MB boundary
        total_size = ((size + HP_SIZE - 1) / HP_SIZE) * HP_SIZE;

        /**
         * MAP_PRIVATE: Not shared with other processes (unless forked).
         * MAP_ANONYMOUS: Not backed by any file.
         * MAP_HUGETLB: Use the 2MB HugePage pool.
         * MAP_LOCKED: Pin pages in RAM; prevent the kernel from swapping them.
         */
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_LOCKED;

        ptr = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, flags, -1, 0);

        if (ptr == MAP_FAILED) {
            std::string err_msg = "[Porth-IO] Fatal: HugePage allocation failed.\n";
            err_msg += "Check: 'sysctl vm.nr_hugepages' must be > 0.\n";
            err_msg += "Check: Current user must have 'memlock' limits set in /etc/security/limits.conf.";
            throw std::runtime_error(err_msg);
        }

        // Advise the kernel that we intend to use this memory sequentially (optional but good practice)
        madvise(ptr, total_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
    }

    ~PorthHugePage() {
        if (ptr && ptr != MAP_FAILED) {
            munmap(ptr, total_size);
        }
    }

    // No copying: Memory ownership must be unique
    PorthHugePage(const PorthHugePage&) = delete;
    PorthHugePage& operator=(const PorthHugePage&) = delete;

    void* data() { return ptr; }
    size_t size() const { return total_size; }

    /**
     * get_as<T>(): Casts the HugePage to your RingBuffer or Descriptor pool.
     */
    template <typename T>
    T* get_as() {
        if (sizeof(T) > total_size) {
            throw std::runtime_error("Porth-IO: Requested structure exceeds HugePage size.");
        }
        return static_cast<T*>(ptr);
    }
};