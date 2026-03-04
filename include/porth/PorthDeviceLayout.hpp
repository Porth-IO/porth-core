#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "PorthRegister.hpp"

/**
 * PorthDeviceLayout: The physical memory map for Porth-IO hardware.
 * * This struct represents a 256-byte window (BAR) into the Cardiff chip.
 * Every register is exactly 64-bytes (one cache line) apart to ensure
 * maximum throughput and zero false-sharing between CPU cores.
 */
struct alignas(64) PorthDeviceLayout {
    // --- CORE REGISTERS ---
    // Offset 0x00: Master control (Start/Stop/Reset)
    PorthRegister<uint32_t> control;

    // Offset 0x40: Device status (Ready/Busy/Error)
    PorthRegister<uint32_t> status;

    // Offset 0x80: Data Plane pointer (DMA address for InP/GaN processing)
    PorthRegister<uint64_t> data_ptr;

    // Offset 0xC0: Telemetry counter (Packet/Work-unit count)
    PorthRegister<uint64_t> counter;

    // --- DOMAIN REGISTERS ---
    // Offset 0x100: Photonics Laser Temperature (milli-Celsius)
    PorthRegister<uint32_t> laser_temp;

    // Offset 0x140: GaN Power Stage Voltage (milli-Volts)
    PorthRegister<uint32_t> gan_voltage;

    // Offset 0x180: RF Signal-to-Noise Ratio (dB * 100)
    PorthRegister<int32_t> rf_snr;
};

// --- PHYSICAL MEMORY AUDIT ---
// These assertions ensure that the compiler does not add padding or reorder 
// members, which would make the software incompatible with the hardware.

static_assert(std::is_standard_layout<PorthDeviceLayout>::value, 
    "PorthDeviceLayout must have a standard layout for MMIO!");

static_assert(sizeof(PorthDeviceLayout) == 448, "Layout size mismatch!");
static_assert(offsetof(PorthDeviceLayout, control) == 0x00, "Control offset must be 0x00");
static_assert(offsetof(PorthDeviceLayout, status)  == 0x40, "Status offset must be 0x40");
static_assert(offsetof(PorthDeviceLayout, data_ptr) == 0x80, "Data pointer offset must be 0x80");
static_assert(offsetof(PorthDeviceLayout, counter)  == 0xC0, "Counter offset must be 0xC0");
static_assert(offsetof(PorthDeviceLayout, laser_temp) == 0x100, "Temp offset mismatch");
static_assert(offsetof(PorthDeviceLayout, gan_voltage) == 0x140, "Voltage offset mismatch");
