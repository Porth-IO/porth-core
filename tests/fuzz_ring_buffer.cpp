/**
 * @file fuzz_ring_buffer.cpp
 * @brief Differential Fuzzing for SPSC Ring Buffer verification.
 *
 * Porth-IO: The Sovereign Logic Layer
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthRingBuffer.hpp"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <thread>

using namespace porth;

/** @brief Domain constants to eliminate magic numbers and satisfy clang-tidy. */
constexpr uint32_t FUZZ_SEED = 42;
constexpr uint64_t ADDR_MASK = 0xFFFFFFFFULL;

/** @brief The "Gold Standard" reference implementation using mutex-guarded queue. */
struct ReferenceQueue {
    std::queue<PorthDescriptor> q;
    std::mutex mtx;

    void push(const PorthDescriptor& d) {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(d);
    }

    auto pop(PorthDescriptor& out) -> bool {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty()) {
            return false;
        }
        out = q.front();
        q.pop();
        return true;
    }
};

/**
 * @brief Differential Fuzzing Entry Point.
 * Spawns a producer and consumer to stress the SPSC logic with random data.
 */
auto main() -> int {
    constexpr size_t RING_SIZE = 1024;
    constexpr int OPERATIONS   = 1000000; // 1 Million ops per fuzzer run

    PorthRingBuffer<RING_SIZE> dut; // Device Under Test
    ReferenceQueue ref;             // Reference Model

    std::atomic<bool> producer_done{false};
    std::atomic<size_t> pushed_count{0};
    std::atomic<size_t> popped_count{0};

    std::cout << "[Fuzz] Starting Differential Fuzzing (1M Operations)...\n";

    // Producer Thread
    std::thread producer([&]() {
        std::mt19937 gen(FUZZ_SEED); // Fixed seed for reproducibility
        std::uniform_int_distribution<uint64_t> addr_dist(0, ADDR_MASK);

        for (int i = 0; i < OPERATIONS; ++i) {
            PorthDescriptor desc = {.addr = addr_dist(gen), .len = static_cast<uint32_t>(i)};

            // Spin until space is available in the DUT
            while (!dut.push(desc)) {
                std::this_thread::yield();
            }
            ref.push(desc);
            pushed_count++;
        }
        producer_done = true;
    });

    // Consumer Thread
    std::thread consumer([&]() {
        while (!producer_done || popped_count < OPERATIONS) {
            PorthDescriptor out_dut{};
            PorthDescriptor out_ref{};

            if (dut.pop(out_dut)) {
                // If DUT provided data, Reference MUST provide the exact same data
                const bool ref_success = ref.pop(out_ref);

                // Assertions are used for functional verification
                assert(ref_success && "DUT produced data that Reference didn't have.");
                assert(out_dut.addr == out_ref.addr && "Data Mismatch detected!");
                assert(out_dut.len == out_ref.len && "Metadata Mismatch detected!");

                /**
                 * @note (void)ref_success is used to satisfy -Wunused-variable
                 * when assertions are compiled out in Release/NDEBUG builds.
                 */
                (void)ref_success;

                popped_count++;
            }
        }
    });

    producer.join();
    consumer.join();

    std::cout << "[Success] Fuzzer passed. Logic is bit-identical to reference model.\n";
    return 0;
}