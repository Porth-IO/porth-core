# Porth-IO: The High-Performance C++ Gateway for Compound Semiconductors

![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)
![Standard](https://img.shields.io/badge/C%2B%2B-20%2F23-blue.svg)
![Status](https://img.shields.io/badge/status-Pre--Alpha-orange.svg)

**Bypassing the Silicon Wall for the Next Generation of 1.6T Networking.**

## 🌐 Vision
Based in the Cardiff Compound Semiconductor Cluster, **Porth-IO** is an open-source, user-space data-plane designed to unlock the raw speed of non-silicon hardware. 

Standard Linux kernels introduce microsecond-scale "jitter" that bottlenecks modern Gallium Nitride (GaN) and Indium Phosphide (InP) chips. Porth-IO provides a **Kernel-Bypass** architecture written in Modern C++ that allows applications to talk directly to photonic hardware at sub-microsecond latencies.

## 🚀 Key Features
* **Zero-Copy Architecture:** Leveraging C++ `std::span` and custom allocators to move data from Photonic Integrated Circuits (PICs) directly to memory.
* **Kernel Bypass:** Optimized for `io_uring` and `DPDK` for deterministic, jitter-free I/O.
* **Modern C++ Core:** Utilizes C++20/23 features (Concepts, Coroutines, Modules) for maximum performance and safety.

## 🛠 Tech Stack
* **Language:** Modern C++ (20/23)
* **Build System:** CMake / Ninja
* **OS:** Linux (Targeting Kernel 6.x+)

---
*Porth (n): Welsh for Gateway/Port. Bridging the gap between exotic physics and enterprise software.*
