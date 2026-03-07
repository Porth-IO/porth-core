cmake_minimum_required(VERSION 3.20)
project(porth-core 
        VERSION 1.0.0 
        DESCRIPTION "The Sovereign Logic Layer for Compound Semiconductors"
        LANGUAGES CXX)

# --- 1. C++ Standard & Build Settings ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Hardened compile options for "Sovereign" quality
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

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-interference-size)
else()
    # Clang and static analysis tools need to ignore unknown flags
    add_compile_options(-Wno-unknown-warning-option)
endif()

# --- 2. Dependencies (Professional Search) ---
find_package(Threads REQUIRED)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # REQUIRED ensures the build fails early if the OS isn't ready
    find_library(RT_LIBRARY NAMES rt REQUIRED)
endif()

# Static Analysis Tooling
find_program(IWYU_PATH include-what-you-use)
if(IWYU_PATH)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH} -Wno-unknown-warning-option)
endif()

find_program(CLANG_TIDY_PATH clang-tidy)
if(CLANG_TIDY_PATH)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_PATH} -extra-arg=-Wno-unknown-warning-option)
endif()

# --- 3. Core Library Definition ---
add_library(porth-core INTERFACE)
add_library(Porth::porth-core ALIAS porth-core)

target_include_directories(porth-core INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 10/10 Fix: Link by name 'rt' instead of absolute path ${RT_LIBRARY} 
# to ensure the exported PorthTargets.cmake is portable across systems.
target_link_libraries(porth-core INTERFACE 
    Threads::Threads
    $<$<PLATFORM_ID:Linux>:rt>
)

# 10/10 Fix: Explicitly propagate C++23 requirement to any project 
# that links against porth-core.
target_compile_features(porth-core INTERFACE cxx_std_23)

# --- 4. Installation & Export (The 10/10 Transition) ---
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS porth-core
    EXPORT PorthTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the actual header files
install(DIRECTORY include/porth 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate and install the CMake export set
install(EXPORT PorthTargets
    FILE PorthTargets.cmake
    NAMESPACE Porth::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Porth
)

# Create PorthConfig.cmake so users can find_package(Porth)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PorthConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/PorthConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Porth
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PorthConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Porth
)

# Generate and install the version file to prevent version mismatch
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PorthConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/PorthConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Porth
)

# --- 5. Internal Development (Tests & Examples) ---
option(BUILD_TESTING "Enable project tests" ON)

function(add_porth_target name source is_test)
    add_executable(${name} ${source})
    # Consumers link to the ALIAS to mirror real-world usage
    target_link_libraries(${name} PRIVATE Porth::porth-core)
    
    if(is_test AND BUILD_TESTING)
        add_test(NAME ${name} COMMAND ${name})
    endif()
endfunction()

if(BUILD_TESTING)
    enable_testing()
    
    # Registered Tests
    add_porth_target(test_metrics tests/test_metrics.cpp TRUE)
    add_porth_target(test_sim_phy tests/test_sim_phy.cpp TRUE)
    add_porth_target(test_sim_protocol tests/test_sim_protocol.cpp TRUE)
    add_porth_target(test_sim_domains tests/test_sim_domains.cpp TRUE)
    add_porth_target(test_sim_chaos tests/test_sim_chaos.cpp TRUE)
    add_porth_target(test_physics_barrier tests/test_physics_barrier.cpp TRUE)
    add_porth_target(unit_ring_buffer tests/unit_ring_buffer.cpp TRUE)
endif()

# Build High-Fidelity Examples
add_porth_target(mock_handshake examples/mock_handshake.cpp FALSE)
add_porth_target(zero_copy_handshake examples/zero_copy_handshake.cpp FALSE)
add_porth_target(calibrate_clock examples/calibrate_clock.cpp FALSE)
add_porth_target(std_baseline examples/std_baseline.cpp FALSE)
add_porth_target(porth_vs_std examples/porth_vs_std.cpp FALSE)
add_porth_target(porth-sim-ctl examples/porth-sim-ctl.cpp FALSE)
add_porth_target(porth_bench examples/porth_bench.cpp FALSE)
add_porth_target(porth_full_demo examples/porth_full_demo.cpp FALSE)

# --- Status Summary ---
message(STATUS "----------------------------------------------------")
message(STATUS "Porth-IO: Sovereign Logic Layer Build Configured")
message(STATUS "Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "----------------------------------------------------")