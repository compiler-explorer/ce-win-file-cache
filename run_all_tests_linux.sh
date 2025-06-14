#!/bin/bash

# Script to build and run all test programs on Linux
# 
# This script will:
# 1. Configure CMake with test and metrics support
# 2. Build all test executables 
# 3. Create any missing config files
# 4. Run all tests from the correct working directory
# 5. Provide a colored summary of results
#
# Usage: ./run_all_tests_linux.sh [options]
#
# Options:
#   -h, --help    Show this help message
#   --clean       Clean build directory before building
#   --quick       Skip CMake configuration if build directory exists
#
# Examples:
#   ./run_all_tests_linux.sh              # Full build and test run
#   ./run_all_tests_linux.sh --clean      # Clean build and test run
#   ./run_all_tests_linux.sh --quick      # Skip configuration, just build and test

set -e  # Exit on any error

# Parse command line arguments
CLEAN_BUILD=false
QUICK_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "Linux Test Runner for CE Win File Cache"
            echo
            echo "Usage: $0 [options]"
            echo
            echo "Options:"
            echo "  -h, --help    Show this help message"
            echo "  --clean       Clean build directory before building"
            echo "  --quick       Skip CMake configuration if build directory exists"
            echo
            echo "This script builds and runs all test programs with proper configuration."
            exit 0
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --quick)
            QUICK_BUILD=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build-linux"

echo -e "${BLUE}=== Linux Test Runner ===${NC}"
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo


# Step 1: Handle clean build option
if [[ "$CLEAN_BUILD" == true ]]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Step 1: Configure CMake
if [[ "$QUICK_BUILD" == true && -d "$BUILD_DIR" ]]; then
    echo -e "${YELLOW}Step 1: Skipping CMake configuration (--quick mode)...${NC}"
else
    echo -e "${YELLOW}Step 1: Configuring CMake...${NC}"
    cd "$PROJECT_ROOT"
    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_CACHE_TEST=ON \
        -DENABLE_METRICS=ON \
        -DNO_WINFSP=ON \
        -G Ninja
fi
echo

# Step 2: Build all targets
echo -e "${YELLOW}Step 2: Building all test programs...${NC}"
if ! cmake --build "$BUILD_DIR" --parallel; then
    echo -e "${RED}Build failed! Check the error messages above.${NC}"
    exit 1
fi
echo

# Step 3: Check that executables exist
echo -e "${YELLOW}Step 3: Checking built executables...${NC}"
TESTS_FOUND=0

# List what's actually in the bin directory
echo "Contents of $BUILD_DIR/bin/:"
ls -la "$BUILD_DIR/bin/" || echo "  (directory does not exist)"
echo

for exe in "$BUILD_DIR/bin"/*; do
    if [[ -x "$exe" ]]; then
        echo "  ✓ $(basename "$exe")"
        TESTS_FOUND=$((TESTS_FOUND + 1))
    fi
done
echo "Found $TESTS_FOUND test executables"

# Expected test programs
EXPECTED_TESTS=("cache_test" "cache_demo" "directory_test" "async_test" "filesystem_async_test" 
                "config_threads_test" "config_async_test" "single_thread_test" "edge_cases_test" 
                "metrics_test" "json_config_test" "glob_test" "glob_matcher_unit_test")

echo "Expected ${#EXPECTED_TESTS[@]} test programs, but found $TESTS_FOUND"

if [[ $TESTS_FOUND -lt ${#EXPECTED_TESTS[@]} ]]; then
    echo -e "${YELLOW}Missing executables - checking build log for errors...${NC}"
    # Continue anyway, but note the discrepancy
fi
echo

# Step 4: Run tests
echo -e "${YELLOW}Step 4: Running all tests...${NC}"
cd "$PROJECT_ROOT"  # Set working directory to project root

# Test programs and their special requirements
PASSED=0
FAILED=0
FAILED_TESTS=""

run_test() {
    local test_name="$1"
    local test_args="$2"
    local test_exe="$BUILD_DIR/bin/$test_name"
    
    if [[ -x "$test_exe" ]]; then
        echo -e "${BLUE}--- Running $test_name $test_args ---${NC}"
        
        # Run test and capture exit code
        set +e  # Temporarily disable exit on error for this test
        "$test_exe" $test_args
        local exit_code=$?
        set -e  # Re-enable exit on error
        
        if [[ $exit_code -eq 0 ]]; then
            echo -e "${GREEN}✓ $test_name PASSED (exit code: $exit_code)${NC}"
            PASSED=$((PASSED + 1))
        else
            echo -e "${RED}✗ $test_name FAILED (exit code: $exit_code)${NC}"
            FAILED=$((FAILED + 1))
            FAILED_TESTS="$FAILED_TESTS\n  - $test_name (exit code: $exit_code)"
        fi
        echo
    else
        echo -e "${RED}✗ $test_name executable not found${NC}"
        FAILED=$((FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (not built)"
        echo
    fi
}

# Ensure config files exist
echo -e "${YELLOW}Creating required config files...${NC}"
if [[ ! -f "test_single_thread.json" ]]; then
    cat > test_single_thread.json << 'EOF'
{
  "global": {
    "total_cache_size_mb": 1024,
    "eviction_policy": "lru",
    "cache_directory": "./cache",
    "download_threads": 1
  },
  "compilers": {
    "msvc-14.40": {
      "network_path": "\\\\test-server\\msvc\\14.40",
      "cache_size_mb": 512,
      "cache_always": ["*.exe", "*.dll"],
      "prefetch_patterns": ["*.h"]
    }
  }
}
EOF
    echo "  ✓ Created test_single_thread.json"
fi
echo

# Run all tests with their specific arguments
run_test "cache_test" "--test-cache"
run_test "cache_demo" ""
run_test "directory_test" ""
run_test "async_test" ""
run_test "filesystem_async_test" ""
run_test "config_threads_test" ""
run_test "config_async_test" ""
run_test "single_thread_test" ""
run_test "edge_cases_test" ""
run_test "metrics_test" ""
run_test "glob_test" ""
run_test "glob_matcher_unit_test" ""

# Run CTest unit tests if available
echo -e "${BLUE}--- Running CTest unit tests ---${NC}"
cd "$BUILD_DIR"
if command -v ctest &> /dev/null; then
    set +e  # Temporarily disable exit on error
    ctest --test-dir . -V
    local ctest_exit_code=$?
    set -e  # Re-enable exit on error
    
    if [[ $ctest_exit_code -eq 0 ]]; then
        echo -e "${GREEN}✓ CTest unit tests PASSED (exit code: $ctest_exit_code)${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}✗ CTest unit tests FAILED (exit code: $ctest_exit_code)${NC}"
        FAILED=$((FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - CTest unit tests (exit code: $ctest_exit_code)"
    fi
else
    echo -e "${YELLOW}⚠ CTest not available, skipping unit tests${NC}"
fi
cd "$PROJECT_ROOT"
echo

# Step 5: Summary
echo -e "${YELLOW}=== Test Summary ===${NC}"
echo -e "Total tests: $((PASSED + FAILED))"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"

if [[ $FAILED -gt 0 ]]; then
    echo -e "\n${RED}Failed tests:${NC}"
    echo -e "$FAILED_TESTS"
    echo
    exit 1
else
    echo -e "\n${GREEN}🎉 All tests passed!${NC}"
    exit 0
fi