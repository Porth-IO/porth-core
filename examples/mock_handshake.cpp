#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

#include "../include/porth/PorthMockDevice.hpp"

// Hardware Constants (The "Dictionary" for our Chip)
namespace CardiffChip {
    constexpr uint32_t CTRL_IDLE  = 0x0;
    constexpr uint32_t CTRL_START = 0x1;
    constexpr uint32_t CTRL_RESET = 0x9;

    constexpr uint32_t STATUS_IDLE  = 0x0;
    constexpr uint32_t STATUS_BUSY  = 0x1;
    constexpr uint32_t STATUS_READY = 0x2;
}

/**
 * The "Hardware" Thread: Simulates the Cardiff Indium Phosphide chip.
 * It sits in a tight loop watching the 'control' register.
 */
void run_mock_hardware(const std::string& dev_name) {
    std::cout << "[Hardware] Powering on Cardiff Mock Chip..." << std::endl;
    
    // Connect to the device as a non-owner (the driver creates it)
    // We wait a moment to ensure the driver has initialized the memory first.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    try {
        PorthMockDevice hw(dev_name, false); 
        auto* chip = hw.view();

        std::cout << "[Hardware] Chip is online and Polling..." << std::endl;

        bool running = true;
        while (running) {
            // 1. Wait for START command from the driver
            uint32_t ctrl = chip->control.load();

            if (ctrl == CardiffChip::CTRL_START) {
                std::cout << "[Hardware] START received! Processing photonics data..." << std::endl;
                
                // Set status to BUSY
                chip->status.write(CardiffChip::STATUS_BUSY);

                // Simulate "Work" (e.g., laser stabilization)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                // Update results: Increment counter and set status to READY
                uint64_t current_count = chip->counter.load();
                chip->counter.write(current_count + 1);
                
                std::cout << "[Hardware] Task complete. Result committed to counter." << std::endl;
                
                // Signal completion
                chip->status.write(CardiffChip::STATUS_READY);

                // Return to IDLE state (acknowledge the command)
                chip->control.write(CardiffChip::CTRL_IDLE);

            } else if (ctrl == CardiffChip::CTRL_RESET) {
                std::cout << "[Hardware] RESET signal received. Powering down." << std::endl;
                running = false;
            }

            // High-performance "Pause" to avoid CPU pipeline stalls
            #if defined(__i386__) || defined(__x86_64__)
                asm volatile("pause" ::: "memory");
            #endif
        }
    } catch (const std::exception& e) {
        std::cerr << "[Hardware] Fatal Error: " << e.what() << std::endl;
    }
}

/**
 * The "Driver" Logic (The Main Thread):
 * Acts as the Porth-IO user-space driver.
 */
int main() {
    const std::string device_name = "porth_vdev_0";

    std::cout << "[Driver] Initializing Porth-IO HAL..." << std::endl;

    try {
        // 1. Create the Virtual Hardware Device
        PorthMockDevice driver(device_name, true);
        auto* dev = driver.view();

        // 2. Start the Hardware Thread
        std::thread hw_thread(run_mock_hardware, device_name);

        // 3. Trigger a Hardware Operation
        std::cout << "[Driver] Requesting Cardiff Chip to process packet..." << std::endl;
        dev->control.write(CardiffChip::CTRL_START);

        // 4. Spin-Wait for the result (Zero-Latency Polling)
        std::cout << "[Driver] Polling status register..." << std::endl;
        while (dev->status.load() != CardiffChip::STATUS_READY) {
            // Hot-spinning... 
            #if defined(__i386__) || defined(__x86_64__)
                asm volatile("pause" ::: "memory");
            #endif
        }

        // 5. Read the telemetry data
        uint64_t result = dev->counter.load();
        std::cout << "[Driver] SUCCESS! Chip processed data. New Counter: " << result << std::endl;

        // 6. Cleanup: Send RESET to shutdown hardware thread
        std::cout << "[Driver] Sending SHUTDOWN signal..." << std::endl;
        dev->control.write(CardiffChip::CTRL_RESET);

        hw_thread.join();
        std::cout << "[Driver] All systems offline." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Driver] Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}