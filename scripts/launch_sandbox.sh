#!/bin/bash
# Porth-IO: Sovereign Sandbox Launcher
# Launches the Digital Twin and the Performance Demo in parallel.

echo "--- Porth-IO: Launching Sovereign Sandbox ---"

# 1. Setup Environment
./scripts/setup_hugepages.sh
./build/tools/porth-check-env

# 2. Launch the Simulation Controller (Background)
# This mimics the Newport Hardware being powered on
./build/tools/porth-sim-ctl --config ./configs/newport_default.json &
SIM_PID=$!

sleep 2

# 3. Launch the Developer Demo
# This is the application the developer is actually building
./build/examples/network_portal_demo

# 4. Cleanup
echo "--- Shutting down Sandbox ---"
kill $SIM_PID