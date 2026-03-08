# Porth-IO: Performance Validation Suite
find_package(benchmark REQUIRED)

# Add the new Google Benchmark target
add_executable(porth_perf_suite porth_perf_suite.cpp)
target_link_libraries(porth_perf_suite PRIVATE 
    Porth::porth-core 
    benchmark::benchmark
)

# Legacy benchmarks for comparison
add_porth_target(porth_bench porth_bench.cpp FALSE)
add_porth_target(porth_vs_std porth_vs_std.cpp FALSE)