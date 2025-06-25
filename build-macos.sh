#!/bin/bash

# Build script for macOS using clang++
# This builds without WinFsp support for testing cache logic
# Usage: ./build-macos.sh [--clean]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse command line arguments
CLEAN_BUILD=false
for arg in "$@"; do
    case $arg in
        --clean)
            CLEAN_BUILD=true
            ;;
        --help|-h)
            echo "Usage: $0 [--clean]"
            echo "  --clean    Clean build directory before building"
            echo "  --help     Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}Building CeWinFileCacheFS for macOS (NO_WINFSP)${NC}"

# Create build directory
BUILD_DIR="build-macos"
if [ -d "$BUILD_DIR" ]; then
    if [ "$CLEAN_BUILD" = true ]; then
        echo -e "${YELLOW}Build directory exists. Cleaning...${NC}"
        rm -rf "$BUILD_DIR"
        mkdir -p "$BUILD_DIR"
    else
        echo -e "${YELLOW}Build directory exists. Using existing directory...${NC}"
    fi
else
    mkdir -p "$BUILD_DIR"
fi

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

# Run clang-tidy if available
echo -e "${GREEN}Running clang-tidy analysis...${NC}"
if command -v clang-tidy >/dev/null 2>&1; then
    # Run clang-tidy on recently modified files
    echo -e "${GREEN}Running clang-tidy checks on recently modified files...${NC}"
    RECENT_FILES=(
        "../src/directory_tree.cpp"
        "../include/types/directory_tree.hpp" 
        "../src/file_access_tracker.cpp"
        "../include/ce-win-file-cache/file_access_tracker.hpp"
    )
    
    for file in "${RECENT_FILES[@]}"; do
        if [ -f "$file" ]; then
            echo "Checking $file..."
            clang-tidy "$file" --checks='-*,readability-*,performance-*,modernize-*,bugprone-*' \
                --format-style=file \
                -- -std=c++20 -DNO_WINFSP -I../include -I../include/ce-win-file-cache -I../include/types \
                2>/dev/null || echo -e "${YELLOW}Warning: clang-tidy check completed with issues for $file${NC}"
        fi
    done
    echo -e "${GREEN}clang-tidy analysis complete${NC}"
else
    echo -e "${YELLOW}clang-tidy not found. Install with: brew install llvm${NC}"
    echo -e "${YELLOW}Skipping static analysis...${NC}"
fi

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