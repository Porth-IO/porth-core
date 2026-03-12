#include "porth/PorthDriver.hpp"
#include "porth/PorthSimDevice.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

TEST_CASE("Sovereign Watchdog: Thermal Emergency Halt", "[safety]") {
    using namespace porth;

    // 1. Setup the Hardware Simulation FIRST
    PorthSimDevice sim("safety_test_device");
    auto* regs = sim.view();

    // 2. Create a nested scope for the Driver
    {
        Driver<1024> driver(regs);

        // 3. Power on the "Laser"
        regs->control.write(0x1);

        // 4. Inject "Illegal" Heat (50.0°C)
        std::cout << "[Test] Injecting 50,000mC thermal spike...\n";
        regs->laser_temp.write(50000);

        // 5. SOVEREIGN POLL: Give the watchdog time to wake up.
        bool tripped = false;
        for (int i = 0; i < 200; ++i) { // Increase to 200ms to be safe
            if (regs->control.load() == 0x0) {
                tripped = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // 6. Verify the hardware was SHUT DOWN
        // We check 'tripped' first to avoid calling regs if the test is failing
        REQUIRE(tripped == true);
        REQUIRE(regs->control.load() == 0x0);

        std::cout << "[Success] Watchdog successfully killed the hardware power rail.\n";
    }
    // Driver's destructor is called here. m_watchdog_thread.join() runs.
    // Thread is dead. Now 'sim' can safely go out of scope.
}