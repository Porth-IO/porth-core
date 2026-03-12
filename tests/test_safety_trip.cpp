#include "porth/PorthDriver.hpp"
#include "porth/PorthSimDevice.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

TEST_CASE("Sovereign Watchdog: Thermal Emergency Halt", "[safety]") {
    using namespace porth;

    // 1. Setup the Hardware Simulation
    // Use a unique name for CI to avoid collisions with other tests
    PorthSimDevice sim("safety_test_shm_trip", true);
    auto* regs = sim.view();

    // 2. Ensure SimDevice is actually ready before proceeding
    // This prevents the watchdog from starting against uninitialized SHM
    REQUIRE(regs != nullptr);

    // 3. Create the Driver scope
    {
        Driver<1024> driver(regs);

        // 4. Power on the "Laser" and wait for READY bit
        // The Simulator needs a moment to transition its internal state
        regs->control.write(0x1);

        bool ready = false;
        for (int i = 0; i < 100; ++i) {
            if (regs->status.load() != 0) {
                ready = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE(ready == true);

        // 5. Inject "Illegal" Heat (50.0°C)
        // We use store() to ensure atomic visibility to the watchdog thread
        std::cout << "[Test] Injecting 50,000mC thermal spike...\n";
        regs->laser_temp.write(50000);

        // 6. SOVEREIGN POLL: Give the watchdog time to wake up and trip
        bool tripped = false;
        for (int i = 0; i < 500; ++i) { // 500ms allows for CI scheduling jitter
            if (regs->control.load() == 0x0) {
                tripped = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // 7. Verify the hardware was SHUT DOWN by the Sovereign Watchdog
        REQUIRE(tripped == true);
        REQUIRE(regs->control.load() == 0x0);

        std::cout << "[Success] Watchdog successfully killed the hardware power rail.\n";
    }
    // Driver goes out of scope, watchdog thread is joined here.
}