#!/bin/bash

# Build script for macOS using clang++
# This builds without WinFsp support for testing cache logic

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building CeWinFileCacheFS for macOS (NO_WINFSP)${NC}"

# Create build directory
BUILD_DIR="build-macos"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Build directory exists. Cleaning...${NC}"
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

# Configure with CMake
echo -e "${GREEN}Configuring with CMake...${NC}"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-DNO_WINFSP -std=c++20" \
    -DUSE_WINE_WINDOWS_API=OFF \
    -DBUILD_CACHE_TEST=ON \
    -DENABLE_METRICS=ON \
    -G "Unix Makefiles"

# Build
echo -e "${GREEN}Building...${NC}"
make -j$(sysctl -n hw.ncpu)

# Create output directory structure
echo -e "${GREEN}Setting up output directory...${NC}"
mkdir -p bin
cp CeWinFileCacheFS bin/ 2>/dev/null || true

# Copy config file if it exists
if [ -f ../compilers.yaml ]; then
    cp ../compilers.yaml bin/
fi

echo -e "${GREEN}Build complete!${NC}"
echo -e "${YELLOW}Binaries location: $BUILD_DIR/bin/${NC}"
echo ""
echo "Available binaries:"
echo "  - cache_test: Memory cache manager test program"
echo "  - glob_test: Glob pattern matching test program"
echo "  - glob_matcher_unit_test: Catch2 unit tests for glob matcher"
echo "  - Other test programs: async_test, directory_test, metrics_test, etc."
echo ""
echo "Run tests with:"
echo "  ./$BUILD_DIR/bin/cache_test --test-cache"
echo "  ./$BUILD_DIR/bin/glob_matcher_unit_test"
echo "  cd $BUILD_DIR && ctest -V"
echo ""
echo "Or run all tests with:"
echo "  ./run_all_tests.sh"