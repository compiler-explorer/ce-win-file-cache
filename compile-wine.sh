#!/bin/bash

set -e

# Wine compilation script with error handling
echo "Compiling CeWinFileCache with Wine..."

# Check Wine development tools
if ! command -v winegcc &> /dev/null; then
    echo "Error: Wine development tools not found."
    echo "Install with: sudo apt-get install wine-dev"
    exit 1
fi

# Clean and create build directory
rm -rf build-wine
mkdir -p build-wine
cd build-wine

# Configure with CMake
echo "Configuring with CMake..."
cmake .. \
    -DUSE_WINE_WINDOWS_API=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=winegcc \
    -DCMAKE_CXX_COMPILER=wineg++ \
    -DCMAKE_CXX_FLAGS="-DNOMINMAX -D_GNU_SOURCE" \
    -DCMAKE_VERBOSE_MAKEFILE=ON

# Build with reduced parallelism to see errors better
echo "Building..."
make -j2 2>&1 | tee build.log

if [ $? -eq 0 ]; then
    echo "Build complete!"
    echo "Executable: build-wine/bin/CompilerCacheFS"
else
    echo "Build failed. Check build.log for details."
    echo "Common issues:"
    echo "- Missing Wine headers: install wine-dev"
    echo "- WinFsp compatibility: WinFsp designed for native Windows"
    exit 1
fi