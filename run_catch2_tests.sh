#!/bin/bash

# Script to build and run Catch2 unit tests
# This focuses only on the Catch2-based tests (not the other integration tests)

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Building and Running Catch2 Unit Tests ===${NC}"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Create build directory if it doesn't exist
BUILD_DIR="build-macos"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Creating build directory: $BUILD_DIR${NC}"
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure CMake if needed (check if CMakeCache.txt exists)
if [ ! -f "CMakeCache.txt" ]; then
    echo -e "${YELLOW}Configuring CMake...${NC}"
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_CACHE_TEST=ON \
        -DENABLE_METRICS=ON
fi

# Build only the Catch2 unit tests
echo -e "${YELLOW}Building Catch2 unit tests...${NC}"

# Build Catch2 dependency first
echo -e "Building Catch2 dependency..."
make Catch2 -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Build the unit test executable
echo -e "Building glob_matcher_unit_test..."
make glob_matcher_unit_test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Check if the build was successful
if [ ! -f "bin/glob_matcher_unit_test" ]; then
    echo -e "${RED}Error: Failed to build Catch2 unit tests${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful!${NC}"

# Run the Catch2 unit tests
echo -e "${BLUE}Running Catch2 unit tests...${NC}"
echo

# Run with different output options
echo -e "${YELLOW}Running all tests with summary:${NC}"
./bin/glob_matcher_unit_test --reporter=console

echo
echo -e "${YELLOW}Running tests with verbose output:${NC}"
./bin/glob_matcher_unit_test --reporter=console --verbosity=high

echo
echo -e "${YELLOW}Running specific test sections (if any):${NC}"
./bin/glob_matcher_unit_test --list-tests

echo
echo -e "${GREEN}=== Catch2 Unit Tests Complete ===${NC}"

# Optional: Run using CTest for integration
echo
echo -e "${YELLOW}Running via CTest for additional validation:${NC}"
ctest -R "GlobMatcher" --verbose

echo -e "${GREEN}All Catch2 tests completed successfully!${NC}"