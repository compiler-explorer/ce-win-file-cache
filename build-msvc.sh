#!/bin/bash

# Build script for compiling with MSVC from WSL

# Create build directory
BUILD_DIR="build-msvc"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake using the MSVC toolchain
echo "Configuring CMake with MSVC toolchain..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../msvc-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -G "Unix Makefiles"

# Build the project
echo "Building project..."
cmake --build . --config Release -j$(nproc)

echo "Build complete! Executable should be in: $BUILD_DIR/bin/"