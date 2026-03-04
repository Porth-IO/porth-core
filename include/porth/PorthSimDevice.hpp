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

namespace porth {

class PorthSimDevice {
private:
    PorthMockDevice mock_hw;
    PorthSimPHY phy;
    std::ofstream tlp_log;
    
    std::thread physics_thread;
    std::atomic<bool> run_sim{true};

    // --- Task 4: Chaos State ---
    std::atomic<bool> inject_deadlock{false}; // Sub-task 4.2
    std::atomic<bool> corrupt_status{false};  // Sub-task 4.1

    void run_physics_loop() {
        auto* dev = mock_hw.view();
        uint32_t temp = 25000;
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<uint32_t> bit_dist(0, 31);

        while (run_sim) {
            // Sub-task 4.2: Deadlock Simulation
            // If deadlock is active, the hardware stops updating registers
            if (!inject_deadlock) {
                // Standard Physics (Task 3)
                if (dev->control.load() == 0x1) temp += 500;
                else if (temp > 25000) temp -= 100;
                dev->laser_temp.write(temp);

                uint32_t noise = (std::rand() % 100); 
                dev->gan_voltage.write(48000 + noise);

                // Sub-task 4.1: Register Corruption
                if (corrupt_status) {
                    uint32_t current_status = dev->status.load();
                    // Flip a random bit to simulate electrical interference
                    dev->status.write(current_status ^ (1 << bit_dist(gen)));
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

public:
    PorthSimDevice(const std::string& name, bool create = true) 
        : mock_hw(name, create) {
        tlp_log.open("porth_tlp_traffic.log", std::ios::app);
        physics_thread = std::thread(&PorthSimDevice::run_physics_loop, this);
    }

    ~PorthSimDevice() {
        run_sim = false;
        if (physics_thread.joinable()) physics_thread.join();
        if (tlp_log.is_open()) tlp_log.close();
    }

    void apply_scenario(uint64_t base_ns, uint64_t jitter_ns, bool chaos = false) {
        phy.set_config(base_ns, jitter_ns);
        if (chaos) trigger_corruption(true);
    }
    
    // --- Task 4 Control Interface ---
    void trigger_deadlock(bool active) { inject_deadlock = active; }
    void trigger_corruption(bool active) { corrupt_status = active; }

    // Sub-task 4.3: Buffer Overflow Testing
    // Forces the hardware to ignore the "tail" and keep pushing data
    void force_buffer_overflow(PorthRingBuffer<1024>& ring) {
        PorthDescriptor junk = {0xDEADBEEF, 64};
        for(int i = 0; i < 2000; ++i) {
            ring.push(junk); // This will eventually return false, testing the 'Full' logic
        }
    }

    // --- RETAINED METHODS (Tasks 1-3) ---
    template <typename T>
    T read_reg(PorthRegister<T>& reg) {
        phy.apply_protocol_delay();
        return reg.load();
    }

    template <typename T>
    void write_reg(PorthRegister<T>& reg, T val) {
        phy.apply_protocol_delay();
        reg.write(val);
    }

    template <typename T>
    T read_flit(PorthRegister<T>& reg, uint64_t offset) {
        log_tlp("READ_REQ", offset);
        phy.apply_protocol_delay();
        T val = reg.load();
        log_tlp("COMPLETION", offset, static_cast<uint64_t>(val));
        return val;
    }

    template <typename T>
    void write_flit(PorthRegister<T>& reg, uint64_t offset, T val) {
        log_tlp("WRITE_MEM", offset, static_cast<uint64_t>(val));
        phy.apply_protocol_delay();
        reg.write(val);
    }

    PorthDeviceLayout* view() { return mock_hw.view(); }
    PorthSimPHY& get_phy() { return phy; }

private:
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