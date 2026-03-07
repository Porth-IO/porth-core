/**
 * @file PorthMockDevice.hpp
 * @brief Hardware-abstracted access to high-precision CPU cycle counters.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fcntl.h>
#include <format>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "PorthDeviceLayout.hpp"

namespace porth {

/**
 * @class PorthMockDevice
 * @brief A RAII wrapper for POSIX Shared Memory simulating hardware BAR mapping.
 *
 * This class simulates the Physical PCIe BAR mapping for the Cardiff hardware.
 * It manages the lifecycle of the shared memory segment used for the register
 * map, ensuring that only the "owner" (the simulated hardware) is responsible
 * for unlinking the segment from the system.
 */
class PorthMockDevice {
private:
    std::string m_name;                       ///< POSIX shared memory name (prepended with /).
    PorthDeviceLayout* m_device_ptr{nullptr}; ///< Pointer to the memory-mapped register space.
    bool m_is_owner; ///< Flag indicating if this instance owns the SHM lifecycle.

public:
    /**
     * @brief Constructor: Maps or creates the shared memory segment.
     * @param mem_name The identifier in /dev/shm/ (e.g., "porth_dev0").
     * @param create If true, creates and truncates the memory (Hardware mode).
     * @throws std::runtime_error If shm_open, ftruncate, or mmap fails.
     */
    PorthMockDevice(const std::string& mem_name, bool create = true)
        : m_name("/" + mem_name), m_is_owner(create) {

        // 1. Open/Create the shared memory object
        int flags = O_RDWR;
        if (create) {
            flags |= O_CREAT;
        }

        const int fd = shm_open(m_name.c_str(), flags, 0666);
        if (fd == -1) {
            throw std::runtime_error(std::format("Porth-IO Error: shm_open failed for {}", m_name));
        }

        // 2. Set size (only if we are the creator/owner of the "hardware" segment)
        if (create) {
            if (ftruncate(fd, sizeof(PorthDeviceLayout)) == -1) {
                (void)close(fd);
                throw std::runtime_error("Porth-IO Error: ftruncate failed");
            }
        }

        // 3. Map into process address space
        void* raw_ptr =
            mmap(nullptr, sizeof(PorthDeviceLayout), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        // Resource Safety: We can close the FD immediately after the mapping is established
        (void)close(fd);

        if (raw_ptr == MAP_FAILED) {
            throw std::runtime_error("Porth-IO Error: mmap failed");
        }

        m_device_ptr = static_cast<PorthDeviceLayout*>(raw_ptr);
    }

    /**
     * @brief Destructor: Clean up the mapping and unlink if owner.
     */
    ~PorthMockDevice() {
        if (m_device_ptr != nullptr) {
            (void)munmap(m_device_ptr, sizeof(PorthDeviceLayout));
        }

        // Only the owner (the "Hardware" process) should delete the SHM file
        if (m_is_owner) {
            (void)shm_unlink(m_name.c_str());
        }
    }

    // Prevent copying to maintain strict RAII identity for the hardware mapping
    PorthMockDevice(const PorthMockDevice&)                    = delete;
    auto operator=(const PorthMockDevice&) -> PorthMockDevice& = delete;

    // Rule of Five: Explicitly delete move operations
    PorthMockDevice(PorthMockDevice&&)                    = delete;
    auto operator=(PorthMockDevice&&) -> PorthMockDevice& = delete;

    /** @brief Returns a typed pointer to the register map. */
    [[nodiscard]] auto view() noexcept -> PorthDeviceLayout* { return m_device_ptr; }

    /** @brief Returns a const typed pointer to the register map. */
    [[nodiscard]] auto view() const noexcept -> const PorthDeviceLayout* { return m_device_ptr; }

    /** @brief Operator overload for intuitive, pointer-like access to registers. */
    [[nodiscard]] auto operator->() noexcept -> PorthDeviceLayout* { return m_device_ptr; }
};

} // namespace porth