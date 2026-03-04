#pragma once

#include <fstream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <atomic>
#include <random>

#include "PorthMockDevice.hpp"
#include "PorthSimPHY.hpp"
#include "PorthRingBuffer.hpp"
#include "PorthUtil.hpp"

namespace porth {

/**
 * PorthSimDevice: The high-fidelity Digital Twin for Cardiff Photonics.
 * * This class simulates the physical, electrical, and protocol-level behaviors
 * of a compound semiconductor device on a PCIe Gen 6 bus.
 */
class PorthSimDevice {
private:
    PorthMockDevice mock_hw;
    PorthSimPHY phy;
    std::ofstream tlp_log;
    
    // Physics & Simulation Threads
    std::thread physics_thread;
    std::atomic<bool> run_sim{true};

    // --- Chaos & Error Injection State ---
    std::atomic<bool> inject_deadlock{false}; 
    std::atomic<bool> corrupt_status{false};  
    std::atomic<bool> bus_hang{false};

    /**
     * Internal Physics Engine: Simulates thermal and electrical behavior.
     */
    void run_physics_loop() {
        auto* dev = mock_hw.view();
        uint32_t temp = 25000; // Base 25.0 C
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<uint32_t> bit_dist(0, 31);

        while (run_sim) {
            if (!inject_deadlock) {
                // Task 3: Photonics/GaN Model
                if (dev->control.load() == 0x1) {
                    temp += 800; // Rapid heating during operation
                } else {
                    if (temp > 25000) temp -= 200; // Cooling
                }
                
                dev->laser_temp.write(temp);
                
                // Task 4.3: Thermal Jitter Feedback
                // Pushes current temperature back into the PHY delay engine
                phy.update_thermal_load(temp);

                // GaN Power Rail Noise (48V base)
                uint32_t noise = (std::rand() % 100); 
                dev->gan_voltage.write(48000 + noise);

                // Sub-task 4.1: Register Corruption
                if (corrupt_status) {
                    uint32_t current_status = dev->status.load();
                    dev->status.write(current_status ^ (1 << bit_dist(gen)));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

public:
    /**
     * Constructor: Initializes shared memory and the physics background thread.
     */
    PorthSimDevice(const std::string& name, bool create = true) 
        : mock_hw(name, create) {
        tlp_log.open("porth_tlp_traffic.log", std::ios::app);
        physics_thread = std::thread(&PorthSimDevice::run_physics_loop, this);
    }

    /**
     * Destructor: Safely shuts down simulation threads and closes logs.
     */
    ~PorthSimDevice() {
        run_sim = false;
        if (physics_thread.joinable()) physics_thread.join();
        if (tlp_log.is_open()) tlp_log.close();
    }

    // --- Task 5: Orchestrator & Scenario Control ---
    void apply_scenario(uint64_t base_ns, uint64_t jitter_ns, bool chaos = false) {
        phy.set_config(base_ns, jitter_ns);
        if (chaos) trigger_corruption(true);
    }

    // --- Task 4: Chaos Control ---
    void trigger_deadlock(bool active) { inject_deadlock = active; }
    void trigger_corruption(bool active) { corrupt_status = active; }
    void set_bus_hang(bool active) { bus_hang = active; }

    /**
     * Sub-task 4.3: Buffer Overflow Testing.
     * Artificially saturates the RingBuffer to test driver resilience.
     */
    void force_buffer_overflow(PorthRingBuffer<1024>& ring) {
        PorthDescriptor junk = {0xDEADBEEF, 64};
        for(int i = 0; i < 2048; ++i) {
            ring.push(junk); 
        }
    }

    // --- Tasks 1 & 2: Register Access (MMIO & FLIT) ---

    /**
     * read_reg: Standard MMIO read with PHY latency.
     */
    template <typename T>
    T read_reg(PorthRegister<T>& reg) {
        if (bus_hang) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        phy.apply_protocol_delay();
        return reg.load();
    }

    /**
     * write_reg: Standard MMIO write with PHY latency.
     */
    template <typename T>
    void write_reg(PorthRegister<T>& reg, T val) {
        if (bus_hang) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        phy.apply_protocol_delay();
        reg.write(val);
    }

    /**
     * read_flit: Protocol-aware read simulating a PCIe Gen 6 FLIT completion.
     */
    template <typename T>
    T read_flit(PorthRegister<T>& reg, uint64_t offset) {
        if (bus_hang) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        log_tlp("READ_REQ", offset);
        phy.apply_protocol_delay();
        T val = reg.load();
        log_tlp("COMPLETION", offset, static_cast<uint64_t>(val));
        return val;
    }

    /**
     * write_flit: Protocol-aware write simulating a PCIe Gen 6 Memory Write TLP.
     */
    template <typename T>
    void write_flit(PorthRegister<T>& reg, uint64_t offset, T val) {
        if (bus_hang) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        log_tlp("WRITE_MEM", offset, static_cast<uint64_t>(val));
        phy.apply_protocol_delay();
        reg.write(val);
    }

    // Accessors
    PorthDeviceLayout* view() { return mock_hw.view(); }
    PorthSimPHY& get_phy() { return phy; }

private:
    /**
     * log_tlp: Internal logger for Transaction Layer Packets.
     */
    void log_tlp(const std::string& type, uint64_t addr, uint64_t val = 0) {
        if (tlp_log.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto t_c = std::chrono::system_clock::to_time_t(now);
            tlp_log << "[" << std::put_time(std::localtime(&t_c), "%H:%M:%S") << "] "
                    << "TLP_" << type << " | Addr: 0x" << std::hex << addr 
                    << " | Data: 0x" << val << std::dec << std::endl;
        }
    }
};

} // namespace porth