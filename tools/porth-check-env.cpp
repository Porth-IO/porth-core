/**
 * @file porth-check-env.cpp
 * @brief Diagnostic tool to verify "Sovereign-Ready" system tuning.
 *
 * Porth-IO: The Sovereign Logic Layer
 */

#include "porth/PorthUtil.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

using namespace porth;

void check_cpu_governor() {
    std::cout << "[Check] CPU Frequency Governor... ";
    bool all_performance = true;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/devices/system/cpu/")) {
        std::string path = entry.path().string();
        if (path.find("cpu") != std::string::npos && std::isdigit(path.back())) {
            std::string gov_path = path + "/cpufreq/scaling_governor";
            if (std::filesystem::exists(gov_path)) {
                std::ifstream f(gov_path);
                std::string gov;
                f >> gov;
                if (gov != "performance") all_performance = false;
            }
        }
    }
    if (all_performance) std::cout << "PASS (Performance Mode)\n";
    else std::cout << "FAIL (Jitter Hazard: Set to 'performance')\n";
}

void check_isolated_cpus() {
    std::cout << "[Check] CPU Isolation (isolcpus)... ";
    std::ifstream cmdline("/proc/cmdline");
    std::string line;
    std::getline(cmdline, line);
    if (line.find("isolcpus=") != std::string::npos) {
        std::cout << "PASS\n";
    } else {
        std::cout << "FAIL (Jitter Hazard: No cores isolated from kernel scheduler)\n";
    }
}

void check_rt_kernel() {
    std::cout << "[Check] Real-Time Kernel (PREEMPT_RT)... ";
    if (std::filesystem::exists("/sys/kernel/realtime")) {
        std::ifstream f("/sys/kernel/realtime");
        int val;
        f >> val;
        if (val == 1) std::cout << "PASS\n";
        else std::cout << "FAIL (Not enabled)\n";
    } else {
        std::cout << "FAIL (Standard Kernel detected; expect micro-jitter)\n";
    }
}

int main() {
    std::cout << "--- Porth-IO: Sovereign Environment Audit ---\n";
    
    check_cpu_governor();
    check_isolated_cpus();
    check_rt_kernel();

    std::cout << "[Check] Real-Time Permissions... ";
    auto rt_status = set_realtime_priority();
    if (rt_status) std::cout << "PASS\n";
    else std::cout << "FAIL (Run with sudo or CAP_SYS_NICE)\n";

    std::cout << "--------------------------------------------\n";
    return 0;
}