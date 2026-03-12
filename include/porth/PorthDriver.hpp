/**
 * @file PorthDriver.hpp
 * @brief Orchestration layer for Newport Cluster hardware control and data planes.
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "PorthDeviceLayout.hpp"
#include "PorthShuttle.hpp"
#include "PorthTelemetry.hpp"
#include "PorthUtil.hpp"
#include <atomic>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace porth {

/** * @brief Default capacity for the DMA ring buffer.
 * Set to 1024 to balance memory pressure with throughput burst capabilities.
 */
constexpr size_t DEFAULT_RING_SIZE = 1024;

/**
 * @class Driver
 * @brief The high-level Master Driver for the Newport Cluster.
 *
 * This class encapsulates the HAL (Registers) and the Shuttle (Data Plane).
 * It manages the lifecycle of the memory fabric and automates the hardware
 * handshake during initialization.
 *
 * @tparam RingSize Depth of the DMA ring buffer. Must be a power of two to allow
 * bitwise masking for index wrapping, eliminating costly division cycles.
 */
template <size_t RingSize = DEFAULT_RING_SIZE>
class Driver {
private:
    /** @brief Pointer to memory-mapped hardware registers.
     * Expects a pointer within a UIO/VFIO mapped region.
     */
    PorthDeviceLayout* m_regs;

    /** @brief The zero-copy memory shuttle.
     * Managed by unique_ptr to ensure the HugePage memory is unmapped
     * only when the driver is destroyed (RAII).
     */
    std::unique_ptr<PorthShuttle<RingSize>> m_shuttle;

    std::thread m_watchdog_thread;
    std::atomic<bool> m_run_watchdog{true};
    std::atomic<bool> m_watchdog_ready{false};

    PorthStats* m_stats{nullptr};

    /**
     * @brief Internal watchdog loop: Polling the physical safety boundary.
     * Pins to a "Housekeeping" core (Core 0) to ensure telemetry doesn't
     * interfere with the hot-path (Core 1).
     */
    void watchdog_loop() {
        std::cout << "[Trace] Watchdog: Thread Entering Loop...\n" << std::flush;

        // Safety: Local copy of pointer to avoid re-fetching from 'this' if unstable
        auto* local_regs = m_regs;
        if (!local_regs) {
            std::cerr << "[Trace] Watchdog: FATAL - local_regs is null!\n" << std::flush;
            return;
        }

        try {
            (void)pin_thread_to_core(0);
            std::cout << "[Trace] Watchdog: Core Pinning Successful.\n" << std::flush;
        } catch (...) {
            std::cerr << "[Trace] Watchdog: Core Pinning Failed.\n" << std::flush;
        }

        m_watchdog_ready.store(true, std::memory_order_release);

        while (m_run_watchdog.load(std::memory_order_acquire)) {
            // Check for trip
            const uint32_t temp = local_regs->laser_temp.load();

            if (temp > 45000) {
                std::cerr << std::format("\n!! [Sovereign-Watchdog] THERMAL BREACH: {}mC. Halt.\n",
                                         temp)
                          << std::flush;
                local_regs->control.write(0x0);
                m_run_watchdog.store(false, std::memory_order_release);
                break;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
        std::cout << "[Trace] Watchdog: Thread Exiting Gracefully.\n" << std::flush;
    }

public:
    /**
     * @brief Constructor: Orchestrates the Logic-to-Physical Handshake.
     * * Initializes the zero-copy Shuttle and immediately commits the physical
     * memory address to the hardware's DMA engine.
     * * @param hardware_regs Pointer to the device layout (typically from mmap).
     * @throws std::runtime_error If hardware_regs is null, preventing a segfault
     * during the mandatory hardware handshake.
     */
    explicit Driver(PorthDeviceLayout* hardware_regs) : m_regs(hardware_regs) {
        if (m_regs == nullptr) {
            throw std::runtime_error("Porth-Driver: Hardware registers pointer is null.");
        }

        // Initialize the Shuttle: Allocates HugePage memory and maps the PorthRingBuffer.
        m_shuttle = std::make_unique<PorthShuttle<RingSize>>();

        // Automated Handshake: Commit the DMA address to the hardware.
        const uint64_t dma_addr = m_shuttle->get_device_addr();
        m_regs->data_ptr.write(dma_addr);

        // Spawn the background safety monitor on a housekeeping core.
        m_watchdog_thread = std::thread(&Driver::watchdog_loop, this);

        // Wait for watchdog to finish initialization (pinning/startup)
        while (!m_watchdog_ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        std::cout << std::format("[Porth-Driver] Handshake Complete. Shuttle 0x{:x} active.\n",
                                 dma_addr)
                  << std::flush;
    }

    /**
     * @brief Destructor: Executes the Sovereign Teardown Sequence.
     * * Ensures hardware is safely detached before memory is unmapped.
     * Critically, clears the hardware DMA pointer to prevent Use-After-Free
     * host panics, and safely synchronizes the background watchdog thread.
     */
    ~Driver() {
        // 1. CRITICAL SAFETRY TRIP: Stop hardware from polling system memory.
        // By writing 0, we force the DMA engine to halt BEFORE the Shuttle
        // (and its underlying pinned memory) is destroyed and returned to the OS.
        if (m_regs != nullptr) {
            m_regs->data_ptr.write(0);
        }

        // 2. Safely spin down and join the background watchdog thread.
        m_run_watchdog.store(false, std::memory_order_release);
        if (m_watchdog_thread.joinable()) {
            m_watchdog_thread.join();
        }
    }

    void set_stats_link(PorthStats* stats) noexcept { m_stats = stats; }

    /**
     * @brief transmit: Submits a descriptor to the hardware via the Zero-Copy Shuttle.
     * * @param desc The descriptor to push to hardware.
     * @return PorthStatus::SUCCESS on success, PorthStatus::FULL if the ring is
     * saturated and cannot accept new work without dropping packets.
     * * @note This operation is non-blocking. If the ring is full, the caller
     * must implement a backoff strategy or drop the payload to maintain latency.
     */
    [[nodiscard]] auto transmit(const PorthDescriptor& desc) noexcept -> PorthStatus {
        if (m_shuttle->ring()->push(desc)) {
            if (m_stats) {
                m_stats->total_packets.fetch_add(1, std::memory_order_relaxed);
                m_stats->total_bytes.fetch_add(desc.len, std::memory_order_relaxed);
            }
            return PorthStatus::SUCCESS;
        }
        if (m_stats)
            m_stats->dropped_packets.fetch_add(1, std::memory_order_relaxed);
        return PorthStatus::FULL;
    }

    /**
     * @brief receive: Retrieves a descriptor processed by the hardware.
     * * @param out_desc Reference to store the popped descriptor.
     * @return PorthStatus::SUCCESS on success, PorthStatus::EMPTY if the
     * hardware has not yet returned any completed work.
     * * @note Polling this method is the preferred way to handle high-frequency
     * event loops in the Sovereign Logic Layer.
     */
    [[nodiscard]] auto receive(PorthDescriptor& out_desc) noexcept -> PorthStatus {
        if (m_shuttle->ring()->pop(out_desc)) {
            return PorthStatus::SUCCESS;
        }
        return PorthStatus::EMPTY;
    }

    /** @brief Returns the underlying register layout for direct telemetry access. */
    [[nodiscard]] auto get_regs() const noexcept -> PorthDeviceLayout* { return m_regs; }

    /** @brief Returns the shuttle management object for manual memory auditing. */
    [[nodiscard]] auto get_shuttle() const noexcept -> PorthShuttle<RingSize>* {
        return m_shuttle.get();
    }
};

} // namespace porth