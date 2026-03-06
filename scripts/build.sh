#!/bin/bash
# @file build.sh
# @brief Professional build orchestrator for Porth-IO.
#
# Porth-IO: The Sovereign Logic Layer
# Copyright (c) 2026 Porth-IO Contributors

# Exit on any error
set -e

# ANSI Color Codes for "Immaculate" Output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

echo -e "${BLUE}${BOLD}--- Porth-IO: Sovereign Build Orchestrator ---${NC}"

# 1. Navigate to Project Root
# Ensures the script works from any directory (even if called from root)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# 2. Prepare Build Environment
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${CYAN}[1/3] Creating build infrastructure...${NC}"
    mkdir -p "$BUILD_DIR"
fi
cd "$BUILD_DIR"

# 3. Configure CMake (Defaulting to Release for HFT performance)
echo -e "${CYAN}[2/3] Configuring Sovereign Logic Layer (Release/C++23)...${NC}"
cmake -DCMAKE_BUILD_TYPE=Release ..

# 4. Compile with all available cores
echo -e "${CYAN}[3/3] Compiling Newport Cluster drivers...${NC}"
CPU_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu || echo 1)
make -j"$CPU_CORES"

# 5. Success Summary
echo -e "\n${GREEN}${BOLD}--- Build Immaculate ---${NC}"
echo -e "${BOLD}Artifacts ready in:${NC} ${BLUE}$PROJECT_ROOT/$BUILD_DIR/${NC}"
echo -e "${BOLD}Sovereign Ready for 1.6T Deployment.${NC}"

# List only the compiled executables
echo -e "\n${CYAN}Compiled Targets:${NC}"
find . -maxdepth 1 -executable -type f | grep -v "\.sh" | grep -v "\.so" | sed 's|./||' | sort