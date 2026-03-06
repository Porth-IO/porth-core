/**
 * @file PorthDeviceLayout.hpp
 * @brief Physical memory map definition for Porth-IO hardware.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "PorthRegister.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace porth {

// --- MMIO Layout Constants ---
constexpr size_t cache_line_alignment = 64;
constexpr size_t expected_layout_size = 448;

constexpr size_t offset_control     = 0x00;
constexpr size_t offset_status      = 0x40;
constexpr size_t offset_data_ptr    = 0x80;
constexpr size_t offset_counter     = 0xC0;
constexpr size_t offset_laser_temp  = 0x100;
constexpr size_t offset_gan_voltage = 0x140;
constexpr size_t offset_rf_snr      = 0x180;

/**
 * @struct PorthDeviceLayout
 * @brief The physical memory-mapped I/O (MMIO) layout for Porth-compatible hardware.
 *
 * This structure defines the precise register offsets for interaction with
 * Cardiff-based compound semiconductor chips. It covers three primary domains:
 * 1. Core Control: Basic device lifecycle management.
 * 2. Photonics: Laser telemetry and mode coupling.
 * 3. Power: GaN-based power stage monitoring.
 *
 * The layout is cache-line aligned (64 bytes) to ensure deterministic access
 * and compatibility with high-speed PCIe Gen 6 interconnects.
 */
struct alignas(cache_line_alignment) PorthDeviceLayout {
    /** @brief Offset 0x00: Master control register (Start/Stop/Reset) */
    PorthRegister<uint32_t> control;

    /** @brief Offset 0x40: Device status register (Ready/Busy/Error) */
    PorthRegister<uint32_t> status;

    /** @brief Offset 0x80: Data Plane pointer (DMA base address for the Shuttle) */
    PorthRegister<uint64_t> data_ptr;

    /** @brief Offset 0xC0: Telemetry counter (Packet/Work-unit throughput) */
    PorthRegister<uint64_t> counter;

    /** @brief Offset 0x100: Photonics Laser Temperature in milli-Celsius */
    PorthRegister<uint32_t> laser_temp;

    /** @brief Offset 0x140: GaN Power Stage Voltage in milli-Volts */
    PorthRegister<uint32_t> gan_voltage;

    /** @brief Offset 0x180: RF Signal-to-Noise Ratio (scaled dB: value * 100) */
    PorthRegister<int32_t> rf_snr;
};

// --- PHYSICAL MEMORY AUDIT ---
// These assertions are the primary defense against compiler-introduced padding
// or reordering, which would render the logic layer incompatible with the physical PDK.

static_assert(std::is_standard_layout_v<PorthDeviceLayout>,
              "PorthDeviceLayout must maintain a standard layout for binary MMIO compatibility.");

static_assert(sizeof(PorthDeviceLayout) == expected_layout_size,
              "PorthDeviceLayout size mismatch detected. Layout must be exactly 448 bytes.");

static_assert(offsetof(PorthDeviceLayout, control) == offset_control,
              "Invalid offset: control must be 0x00");
static_assert(offsetof(PorthDeviceLayout, status) == offset_status,
              "Invalid offset: status must be 0x40");
static_assert(offsetof(PorthDeviceLayout, data_ptr) == offset_data_ptr,
              "Invalid offset: data_ptr must be 0x80");
static_assert(offsetof(PorthDeviceLayout, counter) == offset_counter,
              "Invalid offset: counter must be 0xC0");
static_assert(offsetof(PorthDeviceLayout, laser_temp) == offset_laser_temp,
              "Invalid offset: laser_temp must be 0x100");
static_assert(offsetof(PorthDeviceLayout, gan_voltage) == offset_gan_voltage,
              "Invalid offset: gan_voltage must be 0x140");
static_assert(offsetof(PorthDeviceLayout, rf_snr) == offset_rf_snr,
              "Invalid offset: rf_snr must be 0x180");

} // namespace porth