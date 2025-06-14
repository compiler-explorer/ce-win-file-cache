#!/bin/bash

# Enhanced script to build and run different types of tests
# Usage: 
#   ./run_unit_tests.sh              # Run all Catch2 unit tests
#   ./run_unit_tests.sh --quick      # Run Catch2 tests with minimal output
#   ./run_unit_tests.sh --ctest      # Run via CTest only
#   ./run_unit_tests.sh --list       # List available tests
#   ./run_unit_tests.sh --help       # Show help

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default options
QUICK_MODE=false
CTEST_ONLY=false
LIST_TESTS=false
SHOW_HELP=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            QUICK_MODE=true
            shift
            ;;
        --ctest)
            CTEST_ONLY=true
            shift
            ;;
        --list)
            LIST_TESTS=true
            shift
            ;;
        --help|-h)
            SHOW_HELP=true
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            SHOW_HELP=true
            shift
            ;;
    esac
done

# Show help
if [ "$SHOW_HELP" = true ]; then
    echo -e "${CYAN}Catch2 Unit Test Runner${NC}"
    echo
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Options:"
    echo "  --quick      Run tests with minimal output"
    echo "  --ctest      Run via CTest only"
    echo "  --list       List available tests"
    echo "  --help, -h   Show this help message"
    echo
    echo "Examples:"
    echo "  $0                # Run all Catch2 tests with detailed output"
    echo "  $0 --quick       # Run tests quickly"
    echo "  $0 --ctest       # Use CTest runner"
    echo "  $0 --list        # See what tests are available"
    exit 0
fi

echo -e "${BLUE}=== Catch2 Unit Test Runner ===${NC}"

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

# Configure CMake if needed
if [ ! -f "CMakeCache.txt" ]; then
    echo -e "${YELLOW}Configuring CMake...${NC}"
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_CACHE_TEST=ON \
        -DENABLE_METRICS=ON
fi

# List tests and exit if requested
if [ "$LIST_TESTS" = true ]; then
    echo -e "${YELLOW}Available Catch2 unit tests:${NC}"
    echo
    if [ -f "bin/glob_matcher_unit_test" ]; then
        ./bin/glob_matcher_unit_test --list-tests
    else
        echo "Building tests first..."
        make glob_matcher_unit_test -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4) > /dev/null
        ./bin/glob_matcher_unit_test --list-tests
    fi
    echo
    echo -e "${YELLOW}Available CTest tests:${NC}"
    ctest -N
    exit 0
fi

# Build only if not CTest-only mode
if [ "$CTEST_ONLY" = false ]; then
    echo -e "${YELLOW}Building Catch2 unit tests...${NC}"
    
    # Get CPU count for parallel builds
    NCPU=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    # Build Catch2 dependency and unit tests
    make Catch2 glob_matcher_unit_test -j$NCPU
    
    # Check if build was successful
    if [ ! -f "bin/glob_matcher_unit_test" ]; then
        echo -e "${RED}Error: Failed to build Catch2 unit tests${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Build successful!${NC}"
fi

# Run tests based on mode
if [ "$CTEST_ONLY" = true ]; then
    echo -e "${BLUE}Running via CTest...${NC}"
    ctest -R "GlobMatcher" --output-on-failure
elif [ "$QUICK_MODE" = true ]; then
    echo -e "${BLUE}Running Catch2 tests (quick mode)...${NC}"
    ./bin/glob_matcher_unit_test --reporter=compact --success
else
    echo -e "${BLUE}Running Catch2 tests (detailed mode)...${NC}"
    echo
    
    echo -e "${CYAN}Test Summary:${NC}"
    ./bin/glob_matcher_unit_test --reporter=console
    
    echo
    echo -e "${CYAN}Detailed Results:${NC}"
    ./bin/glob_matcher_unit_test --reporter=console --success
    
    echo
    echo -e "${CYAN}Available Test Cases:${NC}"
    ./bin/glob_matcher_unit_test --list-tests
fi

echo
echo -e "${GREEN}=== Unit Tests Complete ===${NC}"