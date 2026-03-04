#pragma once

#include <memory>
#include "PorthDeviceLayout.hpp"
#include "PorthShuttle.hpp"
#include "PorthUtil.hpp"

namespace porth {

/**
 * PorthDriver: The high-level Master Driver for the Newport Cluster.
 * Encapsulates the HAL (Registers) and the Shuttle (Data Plane).
 */
template <size_t RingSize = 1024>
class Driver {
private:
    PorthDeviceLayout* regs;
    std::unique_ptr<PorthShuttle<RingSize>> shuttle;

public:
    /**
     * Constructor: Automates the Hardware Handshake.
     * Maps the registers and writes the Shuttle address to the chip.
     */
    explicit Driver(PorthDeviceLayout* hardware_regs) 
        : regs(hardware_regs) {
        
        if (!regs) {
            throw std::runtime_error("Porth-Driver: Hardware registers pointer is null");
        }

        // Initialize the Shuttle (HugePage memory + RingBuffer)
        shuttle = std::make_unique<PorthShuttle<RingSize>>();

        // Task 2.2: Automated Handshake
        // Write the DMA-ready address of the Shuttle to the hardware's data_ptr
        uint64_t dma_addr = shuttle->get_device_addr();
        regs->data_ptr.write(dma_addr);

        std::cout << "[Porth-Driver] Handshake Complete. Shuttle address 0x" 
                  << std::hex << dma_addr << " committed to hardware." << std::dec << std::endl;
    }

    /**
     * transmit: Sends a descriptor via the Zero-Copy Shuttle.
     */
    PorthStatus transmit(const PorthDescriptor& desc) {
        if (shuttle->ring()->push(desc)) {
            return PorthStatus::SUCCESS;
        }
        return PorthStatus::FULL;
    }

    /**
     * receive: Retrieves a descriptor processed by the hardware.
     */
    PorthStatus receive(PorthDescriptor& out_desc) {
        if (shuttle->ring()->pop(out_desc)) {
            return PorthStatus::SUCCESS;
        }
        return PorthStatus::EMPTY;
    }

    PorthDeviceLayout* get_regs() { return regs; }
    PorthShuttle<RingSize>* get_shuttle() { return shuttle.get(); }
};

} // namespace porth