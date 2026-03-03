#!/bin/bash

# A simple helper script to run the CMake build process.

# 1. Create a build directory (standard practice to keep source clean)
mkdir -p build
cd build

# 2. Run CMake to generate the build files
cmake ..

# 3. Run the actual compilation
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo ""
echo "--- Build Complete ---"
echo "Executables are located in the 'build/' folder:"
ls -F | grep '*'