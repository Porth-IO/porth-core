# Porth-IO: Sovereign Validation Protocol (v1.0)
**Target:** Newport Cluster Compound Semiconductor Hardware  
**Goal:** Certification of sub-microsecond Deterministic Logic

## 1. Physical Test Rig Requirements
To validate the 1.42ns median latency claim, the following hardware must be utilized at the **CSA Catapult** or equivalent facility:
* **Oscilloscope:** Teledyne LeCroy LabMaster 10Zi-A (or equivalent 100GHz BW).
* **Logic Analyzer:** Keysight U4164A with PCIe Gen 6 probing.
* **FPGA Reference:** Xilinx Versal Premium (modeling the Newport InP/GaN controller).

## 2. Test Case: The "Trigger-to-Wire" Benchmark
1. **Setup:** Connect Porth-IO to the Newport FPGA via PCIe Gen 6.
2. **Action:** The Porth-IO `Sentinel` writes `0xDEADBEEF` to the `safety_trip` register (Offset 0x1C0).
3. **Measurement:**
    * **T0:** Software instruction retirement (measured via CPU performance counters).
    * **T1:** PCIe TLP arrival at the FPGA physical layer.
    * **Latency:** $L = T1 - T0$
4. **Pass Criteria:** Median latency must be < 2.0ns over 10 million iterations.

## 3. Environmental Validation
Run `./build/tools/porth-check-env` and attach the output to ensure the host OS was in "Sovereign Mode" (isolcpus, PREEMPT_RT, performance governor).