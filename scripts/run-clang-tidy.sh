#!/bin/bash

# Script to run clang-tidy on the codebase
# Usage: ./scripts/run-clang-tidy.sh [options]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_DIR="build"
FIX_MODE=false
PARALLEL_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
FILES_PATTERN=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --fix)
            FIX_MODE=true
            shift
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --jobs|-j)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --file)
            FILES_PATTERN="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --fix           Apply fixes automatically"
            echo "  --build-dir DIR Use specified build directory (default: build)"
            echo "  --jobs N        Run N jobs in parallel (default: number of CPUs)"
            echo "  --file PATTERN  Check only files matching pattern"
            echo "  --help          Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Check if clang-tidy is installed
CLANG_TIDY=""
if command -v clang-tidy &> /dev/null; then
    CLANG_TIDY="clang-tidy"
elif [ -f "/opt/homebrew/Cellar/llvm/20.1.7/bin/clang-tidy" ]; then
    CLANG_TIDY="/opt/homebrew/Cellar/llvm/20.1.7/bin/clang-tidy"
elif [ -f "/usr/local/Cellar/llvm/*/bin/clang-tidy" ]; then
    CLANG_TIDY=$(find /usr/local/Cellar/llvm -name clang-tidy -type f 2>/dev/null | head -1)
elif [ -f "/opt/homebrew/Cellar/llvm/*/bin/clang-tidy" ]; then
    CLANG_TIDY=$(find /opt/homebrew/Cellar/llvm -name clang-tidy -type f 2>/dev/null | head -1)
fi

if [ -z "$CLANG_TIDY" ]; then
    echo -e "${RED}Error: clang-tidy not found!${NC}"
    echo "Please install clang-tidy:"
    echo "  macOS: brew install llvm"
    echo "  Ubuntu/Debian: sudo apt-get install clang-tidy"
    echo "  Fedora: sudo dnf install clang-tools-extra"
    exit 1
fi

# Check if compile_commands.json exists
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo -e "${YELLOW}Warning: compile_commands.json not found in $BUILD_DIR${NC}"
    echo "Generating compile_commands.json..."
    
    # Create build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"
    
    # Generate compile_commands.json
    cd "$BUILD_DIR"
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
    cd ..
fi

echo "Using clang-tidy version: $($CLANG_TIDY --version | head -n1)"
echo "Build directory: $BUILD_DIR"
echo "Parallel jobs: $PARALLEL_JOBS"

# Find all C++ source files
if [ -n "$FILES_PATTERN" ]; then
    # Use provided pattern
    FILES=$(find src include -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) | grep "$FILES_PATTERN" | grep -v build | grep -v external | sort)
else
    # Find all source files
    FILES=$(find src include -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) | grep -v build | grep -v external | grep -v wine_stubs | sort)
fi

if [ -z "$FILES" ]; then
    echo -e "${YELLOW}No files found to check${NC}"
    exit 0
fi

echo "Found $(echo "$FILES" | wc -l) files to check"

# Run clang-tidy
if [ "$FIX_MODE" = true ]; then
    echo -e "${YELLOW}Running clang-tidy in fix mode...${NC}"
    echo "$FILES" | xargs -P "$PARALLEL_JOBS" -I {} "$CLANG_TIDY" -p "$BUILD_DIR" --fix --fix-errors {} 2>&1 | tee clang-tidy.log
else
    echo "Running clang-tidy in check mode..."
    echo "$FILES" | xargs -P "$PARALLEL_JOBS" -I {} "$CLANG_TIDY" -p "$BUILD_DIR" {} 2>&1 | tee clang-tidy.log
fi

# Check for errors
if grep -E "(warning:|error:)" clang-tidy.log > /dev/null; then
    WARNINGS=$(grep -c "warning:" clang-tidy.log || true)
    ERRORS=$(grep -c "error:" clang-tidy.log || true)
    
    echo -e "\n${YELLOW}Summary:${NC}"
    echo "  Warnings: $WARNINGS"
    echo "  Errors: $ERRORS"
    
    if [ "$ERRORS" -gt 0 ]; then
        echo -e "${RED}clang-tidy found errors!${NC}"
        exit 1
    else
        echo -e "${YELLOW}clang-tidy found warnings${NC}"
        exit 0
    fi
else
    echo -e "\n${GREEN}No issues found!${NC}"
    rm -f clang-tidy.log
    exit 0
fi