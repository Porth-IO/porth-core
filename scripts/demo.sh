#!/bin/bash
# Porth-IO: Sovereign Demo Orchestrator
set -e

# 1. Environment Setup
echo "[Porth-Demo] 1. Allocating Sovereign HugePages..."
sudo ./scripts/setup_hugepages.sh

# 2. Use the Professional Build Orchestrator
echo "[Porth-Demo] 2. Running Sovereign Build Orchestrator..."
chmod +x scripts/build.sh
./scripts/build.sh

# 3. Launching the Driver
echo "[Porth-Demo] 3. Launching Sovereign Driver (Root Required for AF_XDP)..."
# Send driver logs to a file so they don't mess up the dashboard
sudo ./build/examples/porth_full_demo > driver_activity.log 2>&1 & 
DRIVER_PID=$!

# Ensure the driver is cleaned up on exit
trap "sudo kill $DRIVER_PID 2>/dev/null" EXIT

# 4. Connecting the Dashboard
echo "[Porth-Demo] 4. Connecting Sovereign Dashboard..."
sleep 2 
# Add sudo here so the dashboard can read the root-owned telemetry SHM
sudo ./build/tools/porth-sim-ctl monitor porth_newport_0

echo "--- Running Sovereign Resilience Audit ---"
python3 ./scripts/generate_resilience_report.py