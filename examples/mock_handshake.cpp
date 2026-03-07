#include "porth/PorthDeviceLayout.hpp"
#include "porth/PorthMockDevice.hpp"
#include "porth/PorthRegister.hpp"
#include <bits/chrono.h>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

// Hardware Constants (The "Dictionary" for our Chip)
namespace CardiffChip {
constexpr uint32_t CTRL_IDLE  = 0x0;
constexpr uint32_t CTRL_START = 0x1;
constexpr uint32_t CTRL_RESET = 0x9;

constexpr uint32_t STATUS_BUSY  = 0x1;
constexpr uint32_t STATUS_READY = 0x2;

// Timing Constants to resolve magic number warnings
constexpr auto INIT_DELAY_MS     = std::chrono::milliseconds(100);
constexpr auto SIM_WORK_DELAY_MS = std::chrono::milliseconds(500);
} // namespace CardiffChip

using namespace porth;

/**
 * The "Hardware" Thread: Simulates the Cardiff Indium Phosphide chip.
 * It sits in a tight loop watching the 'control' register.
 */
void run_mock_hardware(const std::string& dev_name) {
    std::cout << "[Hardware] Powering on Cardiff Mock Chip..." << '\n';

    // Connect to the device as a non-owner (the driver creates it)
    // We wait a moment to ensure the driver has initialized the memory first.
    std::this_thread::sleep_for(CardiffChip::INIT_DELAY_MS);

    try {
        PorthMockDevice hw(dev_name, false);
        auto* chip = hw.view();

        std::cout << "[Hardware] Chip is online and Polling..." << '\n';

        bool running = true;
        while (running) {
            // 1. Wait for START command from the driver
            uint32_t ctrl = chip->control.load();

            if (ctrl == CardiffChip::CTRL_START) {
                std::cout << "[Hardware] START received! Processing photonics data..." << '\n';

                // Set status to BUSY
                chip->status.write(CardiffChip::STATUS_BUSY);

                // Simulate "Work" (e.g., laser stabilization)
                std::this_thread::sleep_for(CardiffChip::SIM_WORK_DELAY_MS);

                // Update results: Increment counter and set status to READY
                uint64_t current_count = chip->counter.load();
                chip->counter.write(current_count + 1);

                std::cout << "[Hardware] Task complete. Result committed to counter." << '\n';

                // Signal completion
                chip->status.write(CardiffChip::STATUS_READY);

                // Return to IDLE state (acknowledge the command)
                chip->control.write(CardiffChip::CTRL_IDLE);

            } else if (ctrl == CardiffChip::CTRL_RESET) {
                std::cout << "[Hardware] RESET signal received. Powering down." << '\n';
                running = false;
            }

// High-performance "Pause" to avoid CPU pipeline stalls
#if defined(__i386__) || defined(__x86_64__)
            asm volatile("pause" ::: "memory");
#endif
        }
    } catch (const std::exception& e) {
        std::cerr << "[Hardware] Fatal Error: " << e.what() << '\n';
    }
}

/**
 * The "Driver" Logic (The Main Thread):
 * Acts as the Porth-IO user-space driver.
 */
auto main() -> int {
    const std::string device_name = "porth_vdev_0";

    std::cout << "[Driver] Initializing Porth-IO HAL..." << '\n';

    try {
        // 1. Create the Virtual Hardware Device
        PorthMockDevice driver(device_name, true);
        auto* dev = driver.view();

        // 2. Start the Hardware Thread
        std::thread hw_thread(run_mock_hardware, device_name);

        // 3. Trigger a Hardware Operation
        std::cout << "[Driver] Requesting Cardiff Chip to process packet..." << '\n';
        dev->control.write(CardiffChip::CTRL_START);

        // 4. Spin-Wait for the result (Zero-Latency Polling)
        std::cout << "[Driver] Polling status register..." << '\n';
        while (dev->status.load() != CardiffChip::STATUS_READY) {
// Hot-spinning...
#if defined(__i386__) || defined(__x86_64__)
            asm volatile("pause" ::: "memory");
#endif
        }

        // 5. Read the telemetry data
        uint64_t result = dev->counter.load();
        std::cout << "[Driver] SUCCESS! Chip processed data. New Counter: " << result << '\n';

        // 6. Cleanup: Send RESET to shutdown hardware thread
        std::cout << "[Driver] Sending SHUTDOWN signal..." << '\n';
        dev->control.write(CardiffChip::CTRL_RESET);

        hw_thread.join();
        std::cout << "[Driver] All systems offline." << '\n';

    } catch (const std::exception& e) {
        std::cerr << "[Driver] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}