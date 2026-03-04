cmake_minimum_required(VERSION 3.10)
project(porth-core VERSION 1.0.0 LANGUAGES CXX)

# 1. C++ Standard Enforcement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2. Compiler Optimization Flags
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-O3 -Wall -Wextra -Wpedantic)

# 3. Define the Header-Only Library
# Using 'INTERFACE' tells CMake this library consists only of headers.
add_library(porth-core INTERFACE)

# Tell the library where its headers are located
target_include_directories(porth-core INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# 4. Find System Dependencies
find_package(Threads REQUIRED)

# The 'rt' library is required on Linux for shm_open/shm_unlink
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(RT_LIBRARY rt)
endif()

# 5. Build Examples
# This helper function simplifies adding new examples in the future
function(add_porth_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE 
        porth-core 
        Threads::Threads
        ${RT_LIBRARY}
    )
endfunction()

# Add your existing examples
add_porth_example(mock_handshake examples/mock_handshake.cpp)
add_porth_example(zero_copy_handshake examples/zero_copy_handshake.cpp)
# Add to the "Build Examples" section
add_porth_example(calibrate_clock examples/calibrate_clock.cpp)
add_porth_example(test_metrics examples/test_metrics.cpp)
add_porth_example(std_baseline examples/std_baseline.cpp)
add_porth_example(porth_vs_std examples/porth_vs_std.cpp)

message(STATUS "----------------------------------------------------")
message(STATUS "Porth-IO Build Configuration Complete")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Target System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "----------------------------------------------------")