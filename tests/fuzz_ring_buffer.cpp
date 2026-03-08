/**
 * @file fuzz_ring_buffer.cpp
 * @brief Differential Fuzzing for SPSC Ring Buffer verification.
 *
 * Porth-IO: The Sovereign Logic Layer
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthRingBuffer.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <thread>

/** @brief Domain constants to eliminate magic numbers and satisfy clang-tidy. */
constexpr std::uint32_t FUZZ_SEED = 42;
constexpr std::uint64_t ADDR_MASK = 0xFFFFFFFFULL;
constexpr std::size_t RING_SIZE   = 1024;
constexpr int OPERATIONS          = 1000000;

/** @brief The "Gold Standard" reference implementation using mutex-guarded queue. */
struct ReferenceQueue {
    std::queue<porth::PorthDescriptor> q;
    std::mutex mtx;

    void push(const porth::PorthDescriptor& d) {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(d);
    }

    auto pop(porth::PorthDescriptor& out) -> bool {
        std::lock_guard<std::mutex> lock(mtx);
        const bool has_data = !q.empty();
        if (has_data) {
            out = q.front();
            q.pop();
        }
        return has_data;
    }
};

/**
 * @brief Functional verification of a retrieved descriptor against the reference model.
 */
static void verify_transfer(porth::PorthDescriptor& out_dut, ReferenceQueue& ref) {
    porth::PorthDescriptor out_ref{};
    const bool ref_success = ref.pop(out_ref);

    /**
     * @note Assertions for formal functional verification.
     * Explicit comparison with nullptr avoids implicit bool conversion warnings.
     */
    assert(ref_success && "DUT produced data that Reference didn't have." != nullptr);
    assert(out_dut.addr == out_ref.addr && "Data Mismatch detected!" != nullptr);
    assert(out_dut.len == out_ref.len && "Metadata Mismatch detected!" != nullptr);

    /**
     * @note (void) casts satisfy unused-parameter/variable audits in Release builds.
     */
    (void)out_dut;
    (void)ref_success;
}

/**
 * @brief Producer task: Generates random descriptors and pushes them to the DUT and Ref.
 * Uses NOLINT to suppress swappable parameter warnings for atomic references.
 */
static void producer_task(porth::PorthRingBuffer<RING_SIZE>& dut,
                          ReferenceQueue& ref,
                          std::atomic<bool>& done, // NOLINT(bugprone-easily-swappable-parameters)
                          std::atomic<std::size_t>& count) {
    std::mt19937 gen(FUZZ_SEED);
    std::uniform_int_distribution<std::uint64_t> addr_dist(0, ADDR_MASK);

    for (int i = 0; i < OPERATIONS; ++i) {
        porth::PorthDescriptor desc = {.addr = addr_dist(gen),
                                       .len  = static_cast<std::uint32_t>(i)};
        while (!dut.push(desc)) {
            std::this_thread::yield();
        }
        ref.push(desc);
        count++;
    }
    done = true;
}

/**
 * @brief Consumer task: Drains descriptors and triggers differential validation.
 */
static void
consumer_task(porth::PorthRingBuffer<RING_SIZE>& dut,
              ReferenceQueue& ref,
              const std::atomic<bool>& done, // NOLINT(bugprone-easily-swappable-parameters)
              std::atomic<std::size_t>& count) {
    while (!done || count < OPERATIONS) {
        porth::PorthDescriptor out_dut{};
        if (dut.pop(out_dut)) {
            verify_transfer(out_dut, ref);
            count++;
        }
    }
}

/**
 * @brief Differential Fuzzing Entry Point.
 * Spawns a producer and consumer to stress the SPSC logic with random data.
 */
auto main() -> int {
    porth::PorthRingBuffer<RING_SIZE> dut;
    ReferenceQueue ref;

    std::atomic<bool> producer_done{false};
    std::atomic<std::size_t> pushed_count{0};
    std::atomic<std::size_t> popped_count{0};

    std::cout << "[Fuzz] Starting Differential Fuzzing (1M Operations)...\n";

    // Launch tasks using explicit references to satisfy thread requirements.
    std::thread producer(producer_task,
                         std::ref(dut),
                         std::ref(ref),
                         std::ref(producer_done),
                         std::ref(pushed_count));

    std::thread consumer(consumer_task,
                         std::ref(dut),
                         std::ref(ref),
                         std::cref(producer_done),
                         std::ref(popped_count));

    producer.join();
    consumer.join();

    std::cout << "[Success] Fuzzer passed. Logic is bit-identical to reference model.\n";
    return 0;
}