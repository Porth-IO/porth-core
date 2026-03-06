/**
 * @file PorthHugePage.hpp
 * @brief Hardware-abstracted access to high-precision CPU cycle counters.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace porth {

/**
 * @class PorthHugePage
 * @brief Strictly Linux RAII wrapper for HugePage memory.
 *
 * Optimized for InP/GaN DMA transfers, this class provides pinned memory
 * that is resistant to swapping and TLB overhead. It implements a
 * robust fallback mechanism for environments without pre-allocated
 * HugePage pools.
 */
class PorthHugePage {
private:
    void* ptr = nullptr; ///< Base address of the mapped memory region.
    size_t total_size;   ///< Total size after alignment to HugePage boundaries.

    /** @brief Standard 2MB HugePage size for x86_64 architecture. */
    static constexpr size_t HP_SIZE = 2 * 1024 * 1024;

public:
    /**
     * @brief Constructor: Allocates pinned memory with HugePage support.
     * * Attempts to allocate memory from the HugePage pool first. If the kernel
     * is not configured for HugePages, it falls back to standard pages while
     * retaining the MAP_LOCKED attribute to prevent jitter-inducing swaps.
     * * @param size The requested size in bytes.
     * @throws std::runtime_error If both allocation attempts fail.
     */
    explicit PorthHugePage(size_t size) {
        // Round up to nearest 2MB boundary to satisfy HugePage alignment requirements
        total_size = ((size + HP_SIZE - 1) / HP_SIZE) * HP_SIZE;

        /**
         * Attempt 1: The "Fast Path"
         * MAP_PRIVATE: Copy-on-write mapping.
         * MAP_ANONYMOUS: Not backed by any file (RAM only).
         * MAP_HUGETLB: Utilize the 2MB HugePage pool.
         * MAP_LOCKED: Pin pages to RAM to prevent kernel swapping.
         */
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_LOCKED;
        ptr       = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, flags, -1, 0);

        if (ptr == MAP_FAILED) {
            std::cout << "[Porth-IO] Note: HugePages not supported on this kernel. Falling back to "
                         "standard pages.\n";

            // Attempt 2: The "Standard Way" (Remove MAP_HUGETLB but keep the lock)
            flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED;
            ptr   = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, flags, -1, 0);

            if (ptr == MAP_FAILED) {
                throw std::runtime_error("[Porth-IO] Fatal: Total memory allocation failure.");
            }
        }

        // Advise the kernel on access patterns to optimize TLB behavior
        madvise(ptr, total_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
    }

    /** @brief Destructor: Ensures atomic-level hardware memory is unmapped correctly. */
    ~PorthHugePage() {
        if (ptr && ptr != MAP_FAILED) {
            munmap(ptr, total_size);
        }
    }

    // No copying: Memory ownership must be unique to prevent double-freeing mapped regions
    PorthHugePage(const PorthHugePage&)            = delete;
    PorthHugePage& operator=(const PorthHugePage&) = delete;

    /** @brief Returns the raw pointer to the mapped memory. */
    [[nodiscard]] void* data() const noexcept { return ptr; }

    /** @brief Returns the total allocated size (including alignment padding). */
    [[nodiscard]] size_t size() const noexcept { return total_size; }

    /**
     * @brief Maps the memory region to a specific structure type.
     * @tparam T The target structure (e.g., PorthRingBuffer).
     * @return T* A typed view of the HugePage memory.
     * @throws std::runtime_error If the requested type is larger than the allocation.
     */
    template <typename T>
    [[nodiscard]] T* get_as() {
        if (sizeof(T) > total_size) {
            throw std::runtime_error("Porth-IO: Requested structure exceeds HugePage size.");
        }
        return static_cast<T*>(ptr);
    }

    /**
     * @brief Returns the device-visible address for DMA handshakes.
     */
    [[nodiscard]] uint64_t get_device_addr() const noexcept {
        return reinterpret_cast<uint64_t>(ptr);
    }
};

} // namespace porth