cmake_minimum_required(VERSION 3.20)
project(porth-core 
        VERSION 1.0.0 
        DESCRIPTION "The Sovereign Logic Layer for Compound Semiconductors"
        LANGUAGES CXX)

# --- 1. C++ Standard & Build Settings ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    add_compile_options(-Wno-unknown-warning-option)
endif()

# --- 2. Dependencies (Professional Search) ---
find_package(Threads REQUIRED)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(RT_LIBRARY NAMES rt REQUIRED)
endif()

# Static Analysis Tooling
find_program(IWYU_PATH include-what-you-use)
if(IWYU_PATH)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH} -Wno-unknown-warning-option)
endif()

find_program(CLANG_TIDY_PATH NAMES clang-tidy clang-tidy-20)
if(CLANG_TIDY_PATH)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_PATH} -extra-arg=-Wno-unknown-warning-option)
endif()

# --- 3. Core Library Definition ---
# FIX: Defined as INTERFACE to satisfy the header-only requirement.
add_library(porth-core INTERFACE)
add_library(Porth::porth-core ALIAS porth-core)

# FIX: Changed PUBLIC to INTERFACE for header-only propagation.
target_include_directories(porth-core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# FIX: Changed PUBLIC to INTERFACE.
target_link_libraries(porth-core INTERFACE 
    Threads::Threads
    $<$<PLATFORM_ID:Linux>:rt>
)

# FIX: Changed PUBLIC to INTERFACE.
target_compile_features(porth-core INTERFACE cxx_std_23)

# --- 4. Installation & Export ---
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS porth-core
    EXPORT PorthTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/porth 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT PorthTargets
    FILE PorthTargets.cmake
    NAMESPACE Porth::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Porth
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PorthConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/PorthConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Porth
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PorthConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Porth
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PorthConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/PorthConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Porth
)

# --- 5. Internal Development ---
option(BUILD_TESTING "Enable project tests" ON)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE AND NOT MSVC)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

if(BUILD_TESTING)
    enable_testing()
    find_package(Catch2 CONFIG REQUIRED)
    add_subdirectory(tests)
endif()

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