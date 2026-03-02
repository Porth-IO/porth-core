# Contributing to Porth-IO

Welcome! As a low-latency systems project, we maintain high standards for performance and safety.

### ⚖️ Engineering Principles
- **No Exceptions in Hot Paths:** We prefer `std::expected` or error codes to ensure deterministic timing.
- **Cache-Awareness:** Minimize cache misses; keep data structures compact.
- **Modern Standards:** We target C++20 and utilize C++23 where it offers clear performance wins.

### 📝 Pull Request Process
1. Create an Issue describing the hardware support or performance optimization.
2. Ensure code is formatted with the project's `.clang-format`.
3. All data-path changes must include a benchmark from our `porth-bench` suite (coming soon).
