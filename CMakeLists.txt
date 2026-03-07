cmake_minimum_required(VERSION 3.20)
project(porth-core 
        VERSION 1.0.0 
        DESCRIPTION "The Sovereign Logic Layer for Compound Semiconductors"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 1. Testing Infrastructure ---
option(BUILD_TESTING "Enable project tests" ON)
if(BUILD_TESTING)
    enable_testing()
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /WX)
else()
    add_compile_options(-O3 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wnon-virtual-dtor -Wunused -fstack-protector-strong)
endif()

add_library(porth-core INTERFACE)
target_include_directories(porth-core INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(RT_LIBRARY rt)
endif()

find_program(IWYU_PATH include-what-you-use)
if(IWYU_PATH)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH})
endif()

function(add_porth_target name source is_test)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE porth-core Threads::Threads ${RT_LIBRARY})
    
    if(is_test AND BUILD_TESTING)
        add_test(NAME ${name} COMMAND ${name})
    endif()
endfunction()

# --- 2. Build High-Fidelity Examples & Tests ---
add_porth_target(mock_handshake examples/mock_handshake.cpp FALSE)
add_porth_target(zero_copy_handshake examples/zero_copy_handshake.cpp FALSE)
add_porth_target(calibrate_clock examples/calibrate_clock.cpp FALSE)
add_porth_target(std_baseline examples/std_baseline.cpp FALSE)
add_porth_target(porth_vs_std examples/porth_vs_std.cpp FALSE)
add_porth_target(porth-sim-ctl examples/porth-sim-ctl.cpp FALSE)
add_porth_target(porth_bench examples/porth_bench.cpp FALSE)
add_porth_target(porth_full_demo examples/porth_full_demo.cpp FALSE)

# Registered Tests
add_porth_target(test_metrics tests/test_metrics.cpp TRUE)
add_porth_target(test_sim_phy tests/test_sim_phy.cpp TRUE)
add_porth_target(test_sim_protocol tests/test_sim_protocol.cpp TRUE)
add_porth_target(test_sim_domains tests/test_sim_domains.cpp TRUE)
add_porth_target(test_sim_chaos tests/test_sim_chaos.cpp TRUE)
add_porth_target(test_physics_barrier tests/test_physics_barrier.cpp TRUE)

# --- Status Summary ---
message(STATUS "----------------------------------------------------")
message(STATUS "Porth-IO: Sovereign Logic Layer Build Configured")
message(STATUS "Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "Target System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "Architecture:  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "----------------------------------------------------")