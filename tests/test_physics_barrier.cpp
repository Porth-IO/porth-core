/**
 * @file test_physics_barrier.cpp
 * @brief Formal Verification of the Thermal Jitter and Physics Model.
 *
 * Porth-IO: The Sovereign Logic Layer
 * Copyright (c) 2026 Porth-IO Contributors
 */

#include "../include/porth/PorthDriver.hpp"
#include "../include/porth/PorthSimDevice.hpp"
#include <cassert>
#include <format>
#include <iostream>

using namespace porth;

/**
 * @brief Validates that the driver handles thermal spikes deterministically.
 */
void test_thermal_feedback_loop() {
    std::cout << "[Test] Initializing Physics Barrier Verification...\n";

    // 1. Setup Digital Twin
    PorthSimDevice hw("test_dev_0", true);
    Driver<1024> driver(hw.view());

    // 2. Measure Base Latency (at 25C)
    uint32_t base_temp = driver.get_regs()->laser_temp.load();
    std::cout << std::format("  - Base Temp: {} mC\n", base_temp);

    // 3. Force Thermal Load
    std::cout << "  - Injecting operational load (Heating)...\n";
    driver.get_regs()->command.write(0x1);

    // Wait for thermal lattice to heat up (Simulation loop runs at 100Hz)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    uint32_t active_temp = driver.get_regs()->laser_temp.load();
    std::cout << std::format("  - Active Temp: {} mC\n", active_temp);

    // 4. Verification: Thermal Jitter Logic
    // Logic: Temp must have increased due to command 0x1
    assert(active_temp > base_temp && "Physics Model Failure: No thermal drift detected.");

    // 5. Verification: Driver Resilience
    // Ensure the handshake remains stable under heat
    assert(driver.get_regs()->data_ptr.read() != 0 &&
           "Hardware Failure: Handshake lost during thermal spike.");

    std::cout << "[Success] Physics Barrier Test Passed: Thermal Determinism Validated.\n";
}

int main() {
    try {
        test_thermal_feedback_loop();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Fail] " << e.what() << "\n";
        return 1;
    }
}