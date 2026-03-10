cmake_minimum_required(VERSION 3.28)
project(PorthCore VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure the cmake folder is in the search path for include(ebpf)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# --- Compiler Specifics & Linter Support ---
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(PORTH_WARNING_FLAGS -Wall -Wextra -Wshadow -Wpedantic -Wconversion -Wsign-conversion -Wno-unknown-warning-option)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PORTH_WARNING_FLAGS -Wall -Wextra -Wshadow -Wpedantic)
    if(NOT CMAKE_CXX_CLANG_TIDY)
        list(APPEND PORTH_WARNING_FLAGS -Wno-interference-size)
    endif()
endif()

# --- External Dependencies ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBPF REQUIRED libbpf)
pkg_check_modules(LIBXDP REQUIRED libxdp)
find_package(Threads REQUIRED)

include(FetchContent)

# 1. Catch2 for Unit Testing
FetchContent_Declare(Catch2 
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git 
    GIT_TAG v3.5.2
)

# 2. Google Benchmark for performance analysis
FetchContent_Declare(google_benchmark 
    GIT_REPOSITORY https://github.com/google/benchmark.git 
    GIT_TAG v1.8.3
)

set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Catch2 google_benchmark)

# --- eBPF Module Integration ---
include(ebpf)
add_ebpf_program(porth_xdp ${CMAKE_CURRENT_SOURCE_DIR}/include/porth/porth_xdp_kern.c)

# --- Core Target (Interface Library) ---
add_library(porth-core INTERFACE)
add_library(Porth::porth-core ALIAS porth-core)

target_compile_options(porth-core INTERFACE ${PORTH_WARNING_FLAGS})

target_include_directories(porth-core INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${LIBBPF_INCLUDE_DIRS}
    ${LIBXDP_INCLUDE_DIRS}
    ${porth_xdp_INCLUDE_DIR}
)

target_link_libraries(porth-core INTERFACE 
    ${LIBBPF_LIBRARIES} 
    ${LIBXDP_LIBRARIES} 
    Threads::Threads
    $<$<PLATFORM_ID:Linux>:rt>
)

# --- Orchestration ---
enable_testing()

# We delegate the actual executable creation to the subdirectories
# to keep the root file clean and easy to read.
add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(tools)
add_subdirectory(examples)

# --- Status Summary ---
message(STATUS "----------------------------------------------------")
message(STATUS "Porth-IO: Sovereign Logic Layer Build Configured")
message(STATUS "Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "----------------------------------------------------")