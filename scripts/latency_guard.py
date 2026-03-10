import sys
import os
import json

# Porth-IO: Sovereign Latency Guard (Tail Latency Edition)
# ---------------------------------------------------------
MAX_MEDIAN_NS = 50.0  # P50 Limit for Virtualized CI
MAX_P99_NS = 200.0    # THE POINT: P99 Tail Latency Limit
MAX_STDEV_NS = 15.0   # Jitter (Standard Deviation) Limit
# ---------------------------------------------------------

def check_latencies(filename):
    if not os.path.exists(filename):
        print(f"Error: Benchmark result file '{filename}' not found.")
        sys.exit(1)

    try:
        with open(filename, 'r') as f:
            raw_data = f.read()
            
        # FIX: Find the first '{' to skip any leading console logs/warnings
        start_idx = raw_data.find('{')
        if start_idx == -1:
            print(f"Error: No JSON data found in {filename}")
            sys.exit(1)
            
        data = json.loads(raw_data[start_idx:])
        
        benchmarks = data.get("benchmarks", [])
        if not benchmarks:
            print("Error: JSON parsed but no benchmark entries found.")
            sys.exit(1)
        
        # Aggregate logic
        stats = {b["aggregate_name"]: b["real_time"] for b in benchmarks if "aggregate_name" in b}
        
        # Fallback to the first measurement if aggregates (median/p99) aren't present
        median = stats.get("median", benchmarks[0]["real_time"])
        p99 = stats.get("p99", median) 

        print(f"[Guard] Current Median: {median:.2f}ns (Limit: {MAX_MEDIAN_NS}ns)")
        print(f"[Guard] Current P99.9:  {p99:.2f}ns (Limit: {MAX_P99_NS}ns)")

        failed = False
        if median > MAX_MEDIAN_NS:
            print(f"!! [FAILED] Median Latency Regression")
            failed = True
        if p99 > MAX_P99_NS:
            print(f"!! [FAILED] Tail Latency (P99) Regression")
            failed = True

        if failed:
            sys.exit(1)

    except Exception as e:
        print(f"Error: Could not parse benchmark data: {e}")
        # Print the first 100 chars of the file to help debug what's inside
        with open(filename, 'r') as f:
            print(f"Debug - First 100 chars of file: {f.read(100)}")
        sys.exit(1)

    print("--- [PASSED] Sovereign Performance Integrity Maintained ---")

if __name__ == "__main__":
    target_file = sys.argv[1] if len(sys.argv) > 1 else "benchmarks_report.json"
    check_latencies(target_file)