/**
 * @file PorthHugePage.hpp
 * @brief Strictly Linux RAII wrapper for HugePage memory.
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
 * @brief RAII wrapper for pinning memory to 2MB HugePage boundaries.
 *
 * Optimized for InP/GaN DMA transfers, this class provides pinned memory
 * that is resistant to swapping and TLB overhead. By utilizing 2MB pages,
 * we reduce the number of TLB entries required by the MMU, ensuring
 * deterministic memory access during high-throughput RF bursts.
 */
class PorthHugePage {
private:
    void* m_ptr = nullptr; ///< Base address of the mapped memory region.
    size_t m_total_size;   ///< Total size after alignment to HugePage boundaries.

    /** * @brief Standard 2MB HugePage size for x86_64 architecture.
     * This alignment is the physical requirement for the Linux hugetlbfs
     * to perform atomic-level mapping of the data plane.
     */
    static constexpr size_t HP_SIZE = static_cast<size_t>(2) * 1024 * 1024;

public:
    /**
     * @brief Constructor: Allocates pinned memory with HugePage support.
     * * Attempts to allocate memory from the HugePage pool first. If the kernel
     * is not configured for HugePages, it falls back to standard pages while
     * retaining the MAP_LOCKED attribute to prevent jitter-inducing swaps.
     * * @param size The requested size in bytes.
     * @throws std::runtime_error If both allocation attempts fail.
     * @note MAP_LOCKED ensures the memory remains in the physical RAM (RAM-Sovereignty),
     * preventing the Linux Swap Daemon from introducing millisecond-scale latencies.
     */
    explicit PorthHugePage(size_t size) : m_total_size(((size + HP_SIZE - 1) / HP_SIZE) * HP_SIZE) {

        /**
         * Attempt 1: The "Fast Path" (Sovereign DMA Memory)
         * MAP_PRIVATE: Copy-on-write mapping to isolate memory from other processes.
         * MAP_ANONYMOUS: Not backed by any file (RAM only).
         * MAP_HUGETLB: Utilize the 2MB HugePage pool to minimize TLB pressure.
         * MAP_LOCKED: Pin pages to RAM; prevents the kernel from "stealing" our pages.
         */
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_LOCKED;
        m_ptr     = mmap(nullptr, m_total_size, PROT_READ | PROT_WRITE, flags, -1, 0);

        if (m_ptr == MAP_FAILED) {
            std::cout << "[Porth-IO] Note: HugePages not supported on this kernel. Falling back to "
                         "standard pages. TLB pressure may increase.\n";

            // Attempt 2: The "Standard Way" (Remove MAP_HUGETLB but keep the lock)
            // We still require MAP_LOCKED to protect the logic layer from swap-out jitter.
            flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED;
            m_ptr = mmap(nullptr, m_total_size, PROT_READ | PROT_WRITE, flags, -1, 0);

            if (m_ptr == MAP_FAILED) {
                throw std::runtime_error("[Porth-IO] Fatal: Total memory allocation failure.");
            }
        }

        // Advise the kernel on access patterns:
        // MADV_HUGEPAGE: Strong hint for the kernel to coalesce these pages.
        // MADV_SEQUENTIAL: Optimizes the prefetcher for the RingBuffer's linear traversal.
        (void)madvise(m_ptr, m_total_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
    }

    /** @brief Destructor: Releases the HugePage mapping back to the kernel. */
    ~PorthHugePage() {
        if (m_ptr != nullptr && m_ptr != MAP_FAILED) {
            // munmap returns memory to the pool. Failure here is non-recoverable
            // but harmless as the process is usually exiting.
            (void)munmap(m_ptr, m_total_size);
        }
    }

    // No copying: Memory ownership must be unique to prevent double-freeing mapped regions.
    // In a low-latency logic layer, shared memory ownership must be explicit (Shuttle).
    PorthHugePage(const PorthHugePage&)                    = delete;
    auto operator=(const PorthHugePage&) -> PorthHugePage& = delete;

    // No moving: Maintaining strict unique ownership as per original design.
    PorthHugePage(PorthHugePage&&)                    = delete;
    auto operator=(PorthHugePage&&) -> PorthHugePage& = delete;

    /** @brief Returns the raw pointer to the mapped memory. */
    [[nodiscard]] auto data() const noexcept -> void* { return m_ptr; }

    /** @brief Returns the total allocated size (including alignment padding). */
    [[nodiscard]] auto size() const noexcept -> size_t { return m_total_size; }

    /**
     * @brief Maps the memory region to a specific structure type.
     * @tparam T The target structure (e.g., PorthRingBuffer).
     * @return T* A typed view of the HugePage memory.
     * @throws std::runtime_error If the requested type is larger than the allocation.
     * @note This utilizes a static_cast rather than reinterpret_cast where possible
     * to maintain type-safe pointer arithmetic.
     */
    template <typename T>
    [[nodiscard]] auto get_as() -> T* {
        if (sizeof(T) > m_total_size) {
            throw std::runtime_error("Porth-IO: Requested structure exceeds HugePage size.");
        }
        return static_cast<T*>(m_ptr);
    }

    /**
     * @brief Returns the device-visible address for DMA handshakes.
     * @return uint64_t The physical/virtual address to be written to hardware registers.
     * @note On systems with IOMMU disabled, this address is used directly by
     * the PCIe DMA engine of the Newport Cluster.
     */
    [[nodiscard]] auto get_device_addr() const noexcept -> uint64_t {
        // Safe because the memory is pinned (MAP_LOCKED) and will not move.
        return reinterpret_cast<uint64_t>(m_ptr);
    }
};

} // namespace porth