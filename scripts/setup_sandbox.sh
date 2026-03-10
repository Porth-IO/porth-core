#!/bin/bash
# Sovereign Sandbox - Final Bidirectional Static IP Neighbor Link
# Uses modern 'ip neighbor' instead of legacy 'arp'.

# 1. Cleanup
sudo ip netns del injector 2>/dev/null
sudo ip link delete porth0 2>/dev/null

# 2. Create Veth Pair
sudo ip link add porth0 type veth peer name porth1

# 3. Setup Namespace
sudo ip netns add injector
sudo ip link set porth1 netns injector

# 4. Configure IPs
sudo ip addr add 192.168.100.1/24 dev porth0
sudo ip link set porth0 up
sudo ip netns exec injector ip addr add 192.168.100.2/24 dev porth1
sudo ip netns exec injector ip link set porth1 up
sudo ip netns exec injector ip link set lo up

# 5. BIDIRECTIONAL STATIC LINK (The "Deadlock Breaker")
# Get MAC addresses
PORTAL_MAC=$(cat /sys/class/net/porth0/address)
INJECTOR_MAC=$(sudo ip netns exec injector cat /sys/class/net/porth1/address)

# Tell Injector about Portal (using modern ip neighbor)
sudo ip netns exec injector ip neighbor add 192.168.100.1 lladdr $PORTAL_MAC dev porth1
# Tell Portal (Host) about Injector
sudo ip neighbor add 192.168.100.2 lladdr $INJECTOR_MAC dev porth0

echo "[Success] Static Link established via ip-neighbor."
echo "[Config] Portal($PORTAL_MAC) <-> Injector($INJECTOR_MAC)"