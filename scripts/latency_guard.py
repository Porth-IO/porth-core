import sys
import os

# Porth-IO: Sovereign Latency Guard
# ---------------------------------------------------------
# NOTE: These thresholds are calibrated for VIRTUALIZED CI 
# environments (GitHub Shared Runners). 
# Shared VMs experience significant jitter due to "noisy neighbors" 
# and hypervisor overhead. 
#
# BARE-METAL TARGETS (Newport Cluster): Median < 1.5ns, P99 < 6.0ns
# ---------------------------------------------------------
MAX_MEDIAN_NS = 50.0  # Threshold for virtualized regression
MAX_P99_NS = 200.0    # Threshold for virtualized regression

def check_latencies(filename):
    if not os.path.exists(filename):
        print(f"Error: Benchmark file {filename} not found.")
        sys.exit(1)

    with open(filename, 'r') as f:
        content = f.read()

    # Simple parser for the PorthMetric Markdown output
    # Extracts the latest run's Median and P99.9 values
    try:
        median = float(content.split("| Median (P50) |")[1].split("|")[0].strip())
        p99 = float(content.split("| P99.9 |")[1].split("|")[0].strip())
    except (IndexError, ValueError):
        print("Error: Could not parse latency values from report.")
        sys.exit(1)

    print(f"[Guard] Current Median: {median}ns (Limit: {MAX_MEDIAN_NS}ns)")
    print(f"[Guard] Current P99.9: {p99}ns (Limit: {MAX_P99_NS}ns)")

    if median > MAX_MEDIAN_NS or p99 > MAX_P99_NS:
        print("--- [FAILED] Latency Regression Detected ---")
        sys.exit(1)

    print("--- [PASSED] Sovereign Performance Integrity Maintained ---")

if __name__ == "__main__":
    check_latencies("BENCHMARKS.md")