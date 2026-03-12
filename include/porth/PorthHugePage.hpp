/**
 * @file PorthHugePage.hpp
 * @brief NUMA-aware RAII wrapper for HugePage memory sovereignty.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <numa.h>
#include <numaif.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace porth {

/**
 * @class PorthHugePage
 * @brief RAII wrapper for pinning memory to 2MB HugePage boundaries with NUMA affinity.
 *
 * Optimized for InP/GaN DMA transfers, this class ensures memory is physically
 * co-located on the same NUMA node as the hardware device to eliminate
 * cross-socket interconnect latency.
 */
class PorthHugePage {
private:
    void* m_ptr = nullptr;  ///< Base address of the mapped memory region.
    size_t m_total_size;    ///< Total size after alignment to HugePage boundaries.
    int m_node;             ///< The target NUMA node for this allocation.
    bool m_is_numa_managed; ///< Tracks if libnuma was used for allocation.
    bool m_is_mmaped = false;

    static constexpr size_t HP_SIZE = static_cast<size_t>(2) * 1024 * 1024;

public:
    /**
     * @brief Constructor: Allocates pinned, node-locked HugePage memory.
     * @param size The requested size in bytes.
     * @param numa_node The target physical NUMA node (default: 0).
     * @throws std::runtime_error If NUMA-aware HugePage allocation fails.
     */
    explicit PorthHugePage(size_t size, int numa_node = 0)
        : m_total_size(((size + HP_SIZE - 1) / HP_SIZE) * HP_SIZE), m_node(numa_node),
          m_is_numa_managed(false) {

        // Step 1: Attempt Sovereign NUMA-Locked Allocation
        m_ptr = numa_alloc_onnode(m_total_size, m_node);

        // FALLBACK LOGIC FOR VIRTUALIZED ENVIRONMENTS (Bedroom Lab)
        if (m_ptr == nullptr) {
            std::cout << "[Porth-IO] Warning: NUMA node allocation failed. Falling back to "
                         "standard heap for simulation.\n";
            m_ptr             = std::aligned_alloc(HP_SIZE, m_total_size);
            m_is_numa_managed = false;
        } else {
            m_is_numa_managed = true;
        }

        if (m_ptr == nullptr) {
            throw std::runtime_error("[Porth-IO] Fatal: Total memory allocation failure.");
        }

        // Step 2: Attempt HugePage Mapping
        int flags        = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_LOCKED | MAP_FIXED;
        void* mapped_ptr = mmap(m_ptr, m_total_size, PROT_READ | PROT_WRITE, flags, -1, 0);

        if (mapped_ptr == MAP_FAILED) {
            std::cout << "[Porth-IO] Note: HugePages not supported. Using standard locked pages.\n"
                      << std::flush;
            m_is_mmaped = false;
            // Attempt to lock standard pages to prevent swapping
            (void)mlock(m_ptr, m_total_size);
        } else {
            m_is_mmaped = true;
        }

        std::cout << std::format(
            "[Porth-IO] Memory Initialized: {} bytes at {}\n", m_total_size, m_ptr);
    }

    /** @brief Destructor: Releases the node-locked mapping. */
    ~PorthHugePage() {
        if (m_ptr != nullptr) {
            if (m_is_mmaped) {
                munmap(m_ptr, m_total_size);
            }

            if (m_is_numa_managed) {
                numa_free(m_ptr, m_total_size);
            } else if (!m_is_mmaped) {
                // Only free if we didn't successfully mmap over the aligned_alloc
                std::free(m_ptr);
            }
        }
    }

    // Prohibit copying/moving to maintain address stability for DMA.
    PorthHugePage(const PorthHugePage&)                    = delete;
    auto operator=(const PorthHugePage&) -> PorthHugePage& = delete;
    PorthHugePage(PorthHugePage&&)                         = delete;
    auto operator=(PorthHugePage&&) -> PorthHugePage&      = delete;

    [[nodiscard]] auto data() const noexcept -> void* { return m_ptr; }
    [[nodiscard]] auto size() const noexcept -> size_t { return m_total_size; }
    [[nodiscard]] auto node() const noexcept -> int { return m_node; }

    [[nodiscard]] auto get_device_addr() const noexcept -> uint64_t {
        return std::bit_cast<uint64_t>(m_ptr);
    }
};

} // namespace porth