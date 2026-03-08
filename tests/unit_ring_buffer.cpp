/**
 * @file unit_ring_buffer.cpp
 * @brief Isolated Unit Tests for the SPSC Lock-Free Ring Buffer using Catch2.
 *
 * Porth-IO: The Sovereign Logic Layer
 * Copyright (c) 2026 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "porth/PorthRingBuffer.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>

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

TEST_CASE("Ring Buffer Basic Operations", "[unit][ring_buffer]") {
    PorthRingBuffer<RING_SIZE_4> ring;

    // Designated initializers for readability and safety
    PorthDescriptor desc1 = {.addr = ADDR_A, .len = LEN_64};
    PorthDescriptor desc2 = {.addr = ADDR_B, .len = LEN_128};
    PorthDescriptor out   = {.addr = 0, .len = 0};

    SECTION("Successful push and pop sequence") {
        REQUIRE(ring.push(desc1) == true);
        REQUIRE(ring.push(desc2) == true);

        REQUIRE(ring.pop(out) == true);
        CHECK(out.addr == ADDR_A);
        CHECK(out.len == LEN_64);

        REQUIRE(ring.pop(out) == true);
        CHECK(out.addr == ADDR_B);
        CHECK(out.len == LEN_128);
    }

    SECTION("Empty pop returns false") {
        REQUIRE(ring.pop(out) == false);
    }
}

TEST_CASE("Ring Buffer Wrap-Around Logic", "[unit][ring_buffer]") {
    PorthRingBuffer<RING_SIZE_2> ring;
    PorthDescriptor d   = {.addr = ADDR_WRAP, .len = LEN_TINY};
    PorthDescriptor out = {.addr = 0, .len = 0};

    SECTION("Continuous push/pop cycles wrap correctly") {
        // First cycle
        REQUIRE(ring.push(d) == true);
        REQUIRE(ring.pop(out) == true);
        CHECK(out.addr == ADDR_WRAP);

        // Second cycle (triggers wrap-around logic)
        REQUIRE(ring.push(d) == true);
        REQUIRE(ring.pop(out) == true);
        CHECK(out.addr == ADDR_WRAP);
    }
}

TEST_CASE("Ring Buffer Boundary Conditions", "[unit][ring_buffer]") {
    PorthRingBuffer<RING_SIZE_2> ring;
    PorthDescriptor d   = {.addr = ADDR_BOUND, .len = LEN_TINY};
    PorthDescriptor out = {.addr = 0, .len = 0};

    SECTION("SPSC Full state prevents overflow") {
        // Size is 2. In this SPSC implementation, full is N-1 to distinguish from empty.
        REQUIRE(ring.push(d) == true);

        // This should fail because head+1 would hit tail
        REQUIRE(ring.push(d) == false);
    }

    SECTION("Drain to empty state") {
        REQUIRE(ring.push(d) == true);
        REQUIRE(ring.pop(out) == true);
        REQUIRE(ring.pop(out) == false);
    }
}
} // namespace