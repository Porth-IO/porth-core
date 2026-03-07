/**
 * @file unit_ring_buffer.cpp
 * @brief Isolated Unit Tests for the SPSC Lock-Free Ring Buffer.
 *
 * Porth-IO: The Sovereign Logic Layer
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthRingBuffer.hpp"
#include <cassert>
#include <cstddef>   // Required for size_t
#include <cstdint>   // Required for uint64_t, uint32_t
#include <exception> // Required for std::exception
#include <iostream>

namespace {
using namespace porth;

/** @brief Domain constants to eliminate magic numbers and satisfy clang-tidy. */
constexpr uint64_t ADDR_A     = 0x1000;
constexpr uint64_t ADDR_B     = 0x2000;
constexpr uint64_t ADDR_WRAP  = 0xAAAA;
constexpr uint64_t ADDR_BOUND = 0xBBBB;
constexpr uint32_t LEN_64     = 64;
constexpr uint32_t LEN_128    = 128;
constexpr uint32_t LEN_TINY   = 1;
constexpr size_t RING_SIZE_2  = 2;
constexpr size_t RING_SIZE_4  = 4;

void test_basic_push_pop() {
    std::cout << "[Unit] Testing Basic Push/Pop...\n";
    PorthRingBuffer<RING_SIZE_4> ring;

    // Designated initializers for 10/10 readability and safety
    PorthDescriptor desc1 = {.addr = ADDR_A, .len = LEN_64};
    PorthDescriptor desc2 = {.addr = ADDR_B, .len = LEN_128};
    PorthDescriptor out   = {.addr = 0, .len = 0};

    const bool p1 = ring.push(desc1);
    const bool p2 = ring.push(desc2);
    assert(p1 && p2);

    const bool r1 = ring.pop(out);
    assert(r1 && out.addr == ADDR_A);

    const bool r2 = ring.pop(out);
    assert(r2 && out.addr == ADDR_B);

    // Silence unused-variable warnings in Release mode (-DNDEBUG)
    (void)p1;
    (void)p2;
    (void)r1;
    (void)r2;

    std::cout << "  -> Passed.\n";
}

void test_wrap_around_logic() {
    std::cout << "[Unit] Testing Wrap-Around Logic...\n";
    PorthRingBuffer<RING_SIZE_2> ring;
    PorthDescriptor d   = {.addr = ADDR_WRAP, .len = LEN_TINY};
    PorthDescriptor out = {.addr = 0, .len = 0};

    const bool p1 = ring.push(d);
    const bool r1 = ring.pop(out);

    const bool p2 = ring.push(d);
    const bool r2 = ring.pop(out);
    assert(p1 && r1 && p2 && r2);
    assert(out.addr == ADDR_WRAP);

    (void)p1;
    (void)r1;
    (void)p2;
    (void)r2;

    std::cout << "  -> Passed.\n";
}

void test_full_empty_transitions() {
    std::cout << "[Unit] Testing Boundary Conditions (Full/Empty)...\n";
    PorthRingBuffer<RING_SIZE_2> ring;
    PorthDescriptor d   = {.addr = ADDR_BOUND, .len = LEN_TINY};
    PorthDescriptor out = {.addr = 0, .len = 0};

    const bool r1 = ring.pop(out);
    assert(!r1);

    const bool p1 = ring.push(d);
    const bool p2 = ring.push(d); // Should fail (full)
    assert(p1 && !p2);

    (void)r1;
    (void)p1;
    (void)p2;

    std::cout << "  -> Passed.\n";
}
} // namespace

/** @brief Main entry point for Ring Buffer Unit Tests. */
auto main() -> int {
    try {
        test_basic_push_pop();
        test_wrap_around_logic();
        test_full_empty_transitions();
        std::cout << "\n--- [SUCCESS] All Ring Buffer Unit Tests Passed ---\n";
    } catch (const std::exception& e) {
        std::cerr << "Unit Test Exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}