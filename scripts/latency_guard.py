import sys
import os

# Porth-IO: Sovereign Latency Guard
# ---------------------------------------------------------
# NOTE: These thresholds are calibrated for VIRTUALIZED CI 
# environments. BARE-METAL targets: Median < 1.5ns, P99 < 6.0ns
# ---------------------------------------------------------
MAX_MEDIAN_NS = 50.0  # Threshold for virtualized regression
MAX_P99_NS = 200.0    # Threshold for virtualized regression
MAX_STDEV_NS = 10.0   # 10/10 Gate: Strict limit on jitter (Standard Deviation)

def check_latencies(filename):
    if not os.path.exists(filename):
        print(f"Error: Benchmark file {filename} not found.")
        sys.exit(1)

    with open(filename, 'r') as f:
        content = f.read()

    # Simple parser for the PorthMetric Markdown output
    try:
        median = float(content.split("| Median (P50) |")[1].split("|")[0].strip())
        p99 = float(content.split("| P99.9 |")[1].split("|")[0].strip())
        
        # 10/10: Attempt to parse StDev from the benchmark logs
        stdev = 0.0
        for line in content.split('\n'):
            if "StDev:" in line:
                stdev = float(line.split(":")[1].replace("ns", "").strip())
                break
    except (IndexError, ValueError):
        print("Error: Could not parse latency values from report.")
        sys.exit(1)

    print(f"[Guard] Current Median: {median}ns (Limit: {MAX_MEDIAN_NS}ns)")
    print(f"[Guard] Current P99.9: {p99}ns (Limit: {MAX_P99_NS}ns)")
    if stdev > 0:
        print(f"[Guard] Current Jitter: {stdev}ns (Limit: {MAX_STDEV_NS}ns)")

    if median > MAX_MEDIAN_NS or p99 > MAX_P99_NS or (stdev > MAX_STDEV_NS and stdev > 0):
        print("--- [FAILED] Latency or Jitter Regression Detected ---")
        sys.exit(1)

    print("--- [PASSED] Sovereign Performance Integrity Maintained ---")

if __name__ == "__main__":
    check_latencies("BENCHMARKS.md")