cmake_minimum_required(VERSION 3.20)
project(porth-core 
        VERSION 1.0.0 
        DESCRIPTION "The Sovereign Logic Layer for Compound Semiconductors"
        LANGUAGES CXX)

# --- 1. C++ Standard Enforcement (Sovereign C++23 Standard) ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 2. Build Configuration & Hardened Flags ---
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Hardened flags to ensure "Immaculate" code quality
# -Werror ensures that no "code smells" or warnings ever make it into the binary.
if(MSVC)
    add_compile_options(/W4 /permissive- /WX)
else()
    add_compile_options(
        -O3 
        -Wall 
        -Wextra 
        -Wpedantic 
        -Werror 
        -Wshadow 
        -Wnon-virtual-dtor 
        -Wunused 
        -fstack-protector-strong
    )
endif()

# --- 3. Define the Header-Only Library ---
# Using 'INTERFACE' tells CMake this library consists only of headers.
add_library(porth-core INTERFACE)

# Export the include directory for the library and its consumers
target_include_directories(porth-core INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# --- 4. Find System Dependencies ---
find_package(Threads REQUIRED)

# The 'rt' library is strictly required on Linux for shm_open/shm_unlink (PorthHugePage)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(RT_LIBRARY rt)
endif()

# --- 5. Example Orchestration Helper ---
# This helper ensures all examples are linked against the core library, 
# threading primitives, and the Linux RT library for deterministic memory access.
function(add_porth_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE 
        porth-core 
        Threads::Threads
        ${RT_LIBRARY}
    )
    # Ensure examples also follow the C++23 standard
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

# --- 6. Build High-Fidelity Examples ---
add_porth_example(mock_handshake examples/mock_handshake.cpp)
add_porth_example(zero_copy_handshake examples/zero_copy_handshake.cpp)
add_porth_example(calibrate_clock examples/calibrate_clock.cpp)
add_porth_example(test_metrics tests/test_metrics.cpp)
add_porth_example(std_baseline examples/std_baseline.cpp)
add_porth_example(porth_vs_std examples/porth_vs_std.cpp)
add_porth_example(test_sim_phy tests/test_sim_phy.cpp)
add_porth_example(test_sim_protocol tests/test_sim_protocol.cpp)
add_porth_example(test_sim_domains tests/test_sim_domains.cpp)
add_porth_example(test_sim_chaos tests/test_sim_chaos.cpp)
add_porth_example(porth-sim-ctl examples/porth-sim-ctl.cpp)
add_porth_example(porth_bench examples/porth_bench.cpp)
add_porth_example(porth_full_demo examples/porth_full_demo.cpp)

# --- 7. Status Summary ---
message(STATUS "----------------------------------------------------")
message(STATUS "Porth-IO: Sovereign Logic Layer Build Configured")
message(STATUS "Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "Target System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "Architecture:  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "----------------------------------------------------")