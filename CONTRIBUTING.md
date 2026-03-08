# Contributing to Porth-IO: The Engineering Standard

Welcome. As a mission-critical, low-latency systems project, we maintain an uncompromising bar for performance, safety, and readability. To contribute, you must be prepared to defend every clock cycle and every byte of memory alignment.

---
# ⚖️ Contributor License Agreement (CLA)

By contributing to Porth-IO, you agree that:

1. Your contributions are licensed under the **Apache License, Version 2.0**.

2. You grant the Porth-IO project and its maintainers a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable copyright license to reproduce, prepare derivative works of, and distribute your contributions.

3. You have the legal right to submit the contribution (it is your original work or you have permission).

This ensures the project remains **"Sovereign"** and can be safely integrated into industrial and commercial environments.

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
 * Copyright (c) 2026 Porth-IO Contributors
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

## 🛠️ The Immaculate Toolkit

To contribute to Porth-IO, you must use the same toolchain as our CI/CD pipeline. This ensures your code is "Immaculate" before you even open a Pull Request.

---

### 1. Static Analysis & Formatting

We enforce the absolute latest standards. Do not attempt to format code manually.

* **Clang-Format 20:** Enforces the Porth-IO structural layout.
* **Clang-Tidy 20:** Enforces HFT-grade safety and naming (`m_` prefix).
```bash
# 1. Format all files before committing (Enforces structural layout)
find include examples tests -name "*.hpp" -o -name "*.cpp" | xargs clang-format -i

# 2. Run Static Analysis (Enforces m_ prefix and Cognitive Complexity < 12)
# Note: This requires a 'build' directory and generated compile_commands.json
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
run-clang-tidy -p build include/porth/*.hpp
```

---

### 2. Local CI Testing (`act`)

We use `act` to run our GitHub Actions locally. This allows you to verify the Sovereign Logic Layer CI on your own machine.
```bash
# Run the entire audit and physics verification locally
act -j verify-and-audit
```

---

### 3. Automatic "Immaculate" Gates (Pre-Commit)
We use `pre-commit` to ensure all code meets our standards before it even leaves your machine.

**Installation:**
1. Install the framework: `pip install pre-commit`
2. Install the Porth-IO hooks: `pre-commit install`

**Workflow:**
The hooks require a compilation database to run `clang-tidy`. Ensure your build directory is configured:
```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

**Build for Performance. Design for Sovereignty.**