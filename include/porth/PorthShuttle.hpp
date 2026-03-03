#pragma once

#include "PorthHugePage.hpp"
#include "PorthRingBuffer.hpp"
#include <new> 

namespace porth {
    /**
    * PorthShuttle: The Zero-Copy Orchestrator.
    * Handles the "Placement New" logic to ensure the RingBuffer 
    * lives inside pinned HugePage memory.
    */
    template <size_t Capacity = 1024>
    class PorthShuttle {
    private:
        PorthHugePage memory;
        PorthRingBuffer<Capacity>* ring_ptr;

    public:
        /**
        * Constructor: 
        * Uses Placement New to initialize the RingBuffer inside the HugePage.
        */
        PorthShuttle() : memory(2 * 1024 * 1024) { // Allocate 2MB HugePage
            
            // Task 2.4: Get the raw address from the HugePage
            void* base_addr = memory.data();
            
            // We initialize the RingBuffer at the very start of the HugePage.
            // Because HugePages are 2MB aligned, this is guaranteed 64-byte aligned.
            ring_ptr = new (base_addr) PorthRingBuffer<Capacity>();
            
            std::cout << "[Porth-Shuttle] Zero-Copy Placement New successful at: " << base_addr << std::endl;
        }

        ~PorthShuttle() {
            if (ring_ptr) {
                // Since we used placement new, we must manually call the destructor
                ring_ptr->~PorthRingBuffer();
            }
        }

        /**
        * get_device_addr: 
        * Converts the HugePage pointer to a uint64_t for the Hardware Handshake.
        */
        uint64_t get_device_addr() const {
            return reinterpret_cast<uint64_t>(memory.data());
        }

        PorthRingBuffer<Capacity>* ring() { return ring_ptr; }

        PorthShuttle(const PorthShuttle&) = delete;
        PorthShuttle& operator=(const PorthShuttle&) = delete;
    };
}