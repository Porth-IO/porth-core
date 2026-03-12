/**
 * @file PorthMockDevice.hpp
 * @brief Simulation layer for POSIX Shared Memory mapping of hardware registers.
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
 * This class simulates the physical PCIe BAR (Base Address Register) mapping for
 * the Newport Cluster hardware. It allows the Logic Layer to run in a "Mock"
 * environment where a separate process simulates the InP/GaN chip behavior.
 * * @note This is essential for Software-in-the-Loop (SIL) testing, ensuring
 * register-level compatibility before deploying to physical Cardiff hardware.
 */
class PorthMockDevice {
private:
    std::string m_name;                       ///< POSIX shared memory name (prepended with /).
    PorthDeviceLayout* m_device_ptr{nullptr}; ///< Pointer to the memory-mapped register space.
    bool m_is_owner; ///< Flag indicating if this instance owns the SHM lifecycle (Hardware
                     ///< Simulator).

public:
    /**
     * @brief Constructor: Maps or creates the shared memory segment.
     * * Simulates the act of the OS BIOS mapping the PCIe device into the
     * CPU's physical address space.
     * * @param mem_name The identifier in /dev/shm/ (e.g., "porth_dev0").
     * @param create If true, creates and truncates the memory (Simulates Hardware Power-On).
     * @throws std::runtime_error If shm_open, ftruncate, or mmap fails.
     */
    PorthMockDevice(const std::string& mem_name, bool create = true)
        : m_name("/" + mem_name), m_is_owner(create) {

        if (create) {
            (void)shm_unlink(m_name.c_str());
        }

        // 1. Open/Create the shared memory object
        // O_CREAT: Only used by the simulator to allocate the "physical" register file.
        int flags = O_RDWR;
        if (create) {
            flags |= O_CREAT;
        }

        const int fd = shm_open(m_name.c_str(), flags, 0666);
        if (fd == -1) {
            throw std::runtime_error(std::format("Porth-IO Error: shm_open failed for {}", m_name));
        }

        // 2. Set size (only if we are the creator/owner)
        // This simulates the fixed-size register file defined in the PorthDeviceLayout RTL.
        if (create) {
            if (ftruncate(fd, sizeof(PorthDeviceLayout)) == -1) {
                (void)close(fd);
                throw std::runtime_error(
                    "Porth-IO Error: ftruncate failed during mock-BAR allocation.");
            }
        }

        // 3. Map into process address space
        // MAP_SHARED: Required to allow the Driver and Simulator processes to see
        // the same register state, mimicking the hardware/host interaction.
        void* raw_ptr =
            mmap(nullptr, sizeof(PorthDeviceLayout), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        // Resource Safety: The file descriptor is no longer needed once the
        // mapping is established in the page tables. Closing it prevents FD leaks.
        (void)close(fd);

        if (raw_ptr == MAP_FAILED) {
            throw std::runtime_error("Porth-IO Error: mmap failed for mock-BAR segment.");
        }

        m_device_ptr = static_cast<PorthDeviceLayout*>(raw_ptr);
    }

    /**
     * @brief Destructor: Clean up the mapping and unlink if owner.
     * * If this is the "Hardware" process (owner), it unlinks the SHM segment,
     * simulating a hardware power-down/removal.
     */
    ~PorthMockDevice() {
        if (m_device_ptr != nullptr) {
            // munmap removes the mapping from the process VMAs.
            (void)munmap(m_device_ptr, sizeof(PorthDeviceLayout));
        }

        // Only the owner (the "Hardware" simulator) should delete the SHM file.
        // This ensures that drivers currently attached to the mock device can
        // gracefully handle the "removal" of the device.
        if (m_is_owner) {
            (void)shm_unlink(m_name.c_str());
        }
    }

    // Copying is deleted to prevent multiple instances from trying to manage
    // the same physical mapping identity via RAII.
    PorthMockDevice(const PorthMockDevice&)                    = delete;
    auto operator=(const PorthMockDevice&) -> PorthMockDevice& = delete;

    // Moving is deleted to maintain a stable, immutable mapping throughout the
    // object's lifecycle, reflecting the static nature of PCIe BARs.
    PorthMockDevice(PorthMockDevice&&)                    = delete;
    auto operator=(PorthMockDevice&&) -> PorthMockDevice& = delete;

    /** @brief Returns a typed pointer to the register map. */
    [[nodiscard]] auto view() noexcept -> PorthDeviceLayout* { return m_device_ptr; }

    /** @brief Returns a const typed pointer to the register map for telemetry. */
    [[nodiscard]] auto view() const noexcept -> const PorthDeviceLayout* { return m_device_ptr; }

    /** * @brief Operator overload for intuitive, pointer-like access to registers.
     * Allows code like 'device->control.read()' to feel like direct hardware access.
     */
    [[nodiscard]] auto operator->() noexcept -> PorthDeviceLayout* { return m_device_ptr; }
};

} // namespace porth