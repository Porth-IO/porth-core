# Porth-IO: High-Fidelity Examples
# ---------------------------------------------------------
# These examples demonstrate the zero-copy handshake 
# and real-time synchronization protocols.

# 1. Basic Hardware Handshake
add_porth_target(mock_handshake mock_handshake.cpp FALSE)

# 2. Zero-Copy Address Mapping (Task 2.4 Validation)
add_porth_target(zero_copy_handshake zero_copy_handshake.cpp FALSE)

# 3. System Calibration
add_porth_target(calibrate_clock calibrate_clock.cpp FALSE)

# 4. Standard Linux Comparison Baseline
add_porth_target(std_baseline std_baseline.cpp FALSE)

# 5. The Full Sovereign MVP Demo
add_porth_target(porth_full_demo porth_full_demo.cpp FALSE)