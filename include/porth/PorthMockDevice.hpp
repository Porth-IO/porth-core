#pragma once

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <stdexcept>

#include "PorthDeviceLayout.hpp"

/**
 * PorthMockDevice: A RAII wrapper for POSIX Shared Memory.
 * This simulates the Physical PCIe BAR mapping for the Cardiff hardware.
 */
class PorthMockDevice {
private:
    std::string name;
    PorthDeviceLayout* device_ptr = nullptr;
    bool is_owner;

public:
    /**
     * Constructor: Maps or creates the shared memory segment.
     * @param mem_name The name in /dev/shm/ (e.g., "porth_dev0")
     * @param create If true, creates and truncates the memory.
     */
    PorthMockDevice(const std::string& mem_name, bool create = true) 
        : name("/" + mem_name), is_owner(create) {
        
        // 1. Open/Create the shared memory object
        int flags = O_RDWR;
        if (create) flags |= O_CREAT;

        int fd = shm_open(name.c_str(), flags, 0666);
        if (fd == -1) {
            throw std::runtime_error("Porth-IO Error: shm_open failed for " + name);
        }

        // 2. Set size (only if we are the creator/owner)
        if (create) {
            if (ftruncate(fd, sizeof(PorthDeviceLayout)) == -1) {
                close(fd);
                throw std::runtime_error("Porth-IO Error: ftruncate failed");
            }
        }

        // 3. Map into process address space
        void* raw_ptr = mmap(nullptr, sizeof(PorthDeviceLayout), 
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        
        // We can close the FD immediately after mapping
        close(fd);

        if (raw_ptr == MAP_FAILED) {
            throw std::runtime_error("Porth-IO Error: mmap failed");
        }

        device_ptr = static_cast<PorthDeviceLayout*>(raw_ptr);
    }

    // Destructor: Clean up the mapping
    ~PorthMockDevice() {
        if (device_ptr) {
            munmap(device_ptr, sizeof(PorthDeviceLayout));
        }
        // Only the owner (the "Hardware" process) should delete the SHM file
        if (is_owner) {
            shm_unlink(name.c_str());
        }
    }

    // Prevent copying (we don't want multiple objects unlinking the same memory)
    PorthMockDevice(const PorthMockDevice&) = delete;
    PorthMockDevice& operator=(const PorthMockDevice&) = delete;

    // Accessors
    PorthDeviceLayout* view() { return device_ptr; }
    const PorthDeviceLayout* view() const { return device_ptr; }
    
    // Easy access to the registers
    PorthDeviceLayout* operator->() { return device_ptr; }
};
