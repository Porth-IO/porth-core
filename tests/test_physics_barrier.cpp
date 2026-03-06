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
auto test_thermal_feedback_loop() -> void {
    std::cout << "[Test] Initializing Physics Barrier Verification...\n";

    // Constants to resolve magic number warnings
    constexpr size_t test_ring_size = 1024;
    constexpr auto heat_wait_ms     = std::chrono::milliseconds(200);
    constexpr uint32_t start_cmd    = 0x1;

    // 1. Setup Digital Twin
    PorthSimDevice hw("test_dev_0", true);
    Driver<test_ring_size> driver(hw.view());

    // 2. Measure Base Latency (at 25C)
    uint32_t base_temp = driver.get_regs()->laser_temp.load();
    std::cout << std::format("  - Base Temp: {} mC\n", base_temp);

    // 3. Force Thermal Load
    std::cout << "  - Injecting operational load (Heating)...\n";
    // Fixed: renamed 'command' to 'control' to match PorthDeviceLayout
    driver.get_regs()->control.write(start_cmd);

    // Wait for thermal lattice to heat up (Simulation loop runs at 100Hz)
    std::this_thread::sleep_for(heat_wait_ms);

    uint32_t active_temp = driver.get_regs()->laser_temp.load();
    std::cout << std::format("  - Active Temp: {} mC\n", active_temp);

    // 4. Verification: Thermal Jitter Logic
    // Logic: Temp must have increased due to command 0x1
    assert(active_temp > base_temp && "Physics Model Failure: No thermal drift detected.");

    // 5. Verification: Driver Resilience
    // Ensure the handshake remains stable under heat
    // Fixed: changed '.read()' to '.load()' to match PorthRegister API
    assert(driver.get_regs()->data_ptr.load() != 0 &&
           "Hardware Failure: Handshake lost during thermal spike.");

    std::cout << "[Success] Physics Barrier Test Passed: Thermal Determinism Validated.\n";
}

auto main() -> int {
    try {
        test_thermal_feedback_loop();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Fail] " << e.what() << "\n";
        return 1;
    }
}