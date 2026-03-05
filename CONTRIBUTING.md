# Contributing to Porth-IO: The Engineering Standard

Welcome. As a mission-critical, low-latency systems project, we maintain an uncompromising bar for performance, safety, and readability. To contribute, you must be prepared to defend every clock cycle and every byte of memory alignment.

---

## 1. Engineering Principles

### I. The Hot-Path Rule

* **No Exceptions:** We use `std::expected` or error codes. Exception unwinding is non-deterministic and introduces unacceptable latency overhead.
* **No Heap Allocation:** All necessary memory must be pre-allocated during the system initialization phase.
* **Cache-Awareness:** Data structures must be compact and aligned to 64-byte cache lines (`alignas(64)`) to prevent false sharing and optimize memory throughput.

### II. Modern Standards

* **Compiler Requirements:** Clang 16+ or GCC 13+.
* **Language Standard:** C++23. We utilize modern concurrency primitives and features like `std::mdspan` and `std::atomic_ref` where they offer clear performance advantages.

---

## 2. Pull Request Process

### I. The Quality Bar

Pull Requests will be rejected if they contain even a single compiler warning. Our CI environment runs with `-Wall -Wextra -Werror` active at all times.

### II. Formatting and Uniformity

Code must be strictly formatted according to the project's `.clang-format` configuration. Furthermore, every source file MUST include the standardized Porth-IO header.

#### Standardized File Header
```cpp
/**
 * @file [FILENAME]
 * @brief [BRIEF_DESCRIPTION]
 *
 * Porth-IO: The Sovereign Logic Layer
 *
 * Copyright (c) 2024 Porth-IO Contributors
 * SPDX-License-Identifier: Apache-2.0
 */
```

### III. Documentation

All public APIs must be documented using Doxygen-style headers. Descriptions should be concise and focus on the "why" and "how" of the implementation.

### IV. Performance Validation

Any change to the core data path (`include/porth/`) must include a `porth-bench` report. We do not merge code that regresses our Tail Latency (P99.9) targets.

---

## 3. Getting Started

Check the project's Issue tracker for "Good First Issue" tags. These typically involve enhancements to the hardware simulator or refinement of the utility layers.

---

**Build for Performance. Design for Sovereignty.**