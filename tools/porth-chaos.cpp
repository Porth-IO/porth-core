#include "porth/PorthDeviceLayout.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

int main() {
    using namespace porth;
    
    // 1. Map the SAME hardware simulation memory the driver is using
    int fd = shm_open("/porth_newport_0", O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "Hardware Simulation not found. Run the demo first.\n";
        return 1;
    }

    void* ptr = mmap(nullptr, sizeof(PorthDeviceLayout), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    auto* regs = static_cast<PorthDeviceLayout*>(ptr);

    std::cout << "--- Porth-IO Chaos Injector ---\n";
    std::cout << "1. Trigger Thermal Spike (Simulate Laser Overheating)\n";
    std::cout << "2. Manually Halt Power Rail (Simulate Hardware Fault)\n";
    std::cout << "Selection: ";
    
    int choice;
    std::cin >> choice;

    if (choice == 1) {
        std::cout << "Injecting 50,000mC thermal spike...\n";
        regs->laser_temp.write(50000); // Trigger the 45C watchdog limit
    } else {
        std::cout << "Dropping Hardware Power Rail...\n";
        regs->control.write(0); // Simulate a logic hang
    }

    munmap(ptr, sizeof(PorthDeviceLayout));
    close(fd);
    return 0;
}