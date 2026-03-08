# Porth-IO: Performance Benchmarks
# ---------------------------------------------------------

# Find the vcpkg benchmark package
find_package(benchmark CONFIG REQUIRED)

# Helper function for benchmarks
function(add_porth_benchmark name source)
    add_executable(${name} ${source})
    # Link against porth-core and the Google Benchmark libraries
    target_link_libraries(${name} PRIVATE 
        Porth::porth-core 
        benchmark::benchmark 
        benchmark::benchmark_main
    )
endfunction()

add_porth_benchmark(porth_bench porth_bench.cpp)
add_porth_benchmark(porth_vs_std porth_vs_std.cpp)
add_porth_benchmark(porth_perf_suite porth_perf_suite.cpp)