#!/bin/bash

# Script to format all C++ source files using clang-format
# 
# This script will:
# 1. Find all .cpp, .hpp, and .h files in src/ and include/ directories
# 2. Apply clang-format to each file in-place
# 3. Report what files were formatted
#
# Usage: ./format_code.sh [options]
#
# Options:
#   -h, --help     Show this help message
#   --check        Check formatting without making changes (dry-run)
#   --verbose      Show detailed output for each file
#
# Examples:
#   ./format_code.sh              # Format all files
#   ./format_code.sh --check      # Check formatting without changes
#   ./format_code.sh --verbose    # Format with detailed output

set -e  # Exit on any error

# Parse command line arguments
CHECK_ONLY=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "C++ Code Formatter for CE Win File Cache"
            echo
            echo "Usage: $0 [options]"
            echo
            echo "Options:"
            echo "  -h, --help     Show this help message"
            echo "  --check        Check formatting without making changes (dry-run)"
            echo "  --verbose      Show detailed output for each file"
            echo
            echo "This script formats all C++ source files using clang-format."
            exit 0
            ;;
        --check)
            CHECK_ONLY=true
            shift
            ;;
        --verbose)
            VERBOSE=true
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

echo -e "${BLUE}=== C++ Code Formatter ===${NC}"
echo "Project root: $PROJECT_ROOT"
echo

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}❌ clang-format not found!${NC}"
    echo "Please install clang-format:"
    echo "  - Ubuntu/Debian: sudo apt-get install clang-format"
    echo "  - macOS: brew install clang-format"
    echo "  - Windows: Install LLVM tools"
    exit 1
fi

# Get clang-format version
CLANG_FORMAT_VERSION=$(clang-format --version)
echo -e "${BLUE}Using: $CLANG_FORMAT_VERSION${NC}"
echo

# Find all C++ source files
echo -e "${YELLOW}Finding C++ source files...${NC}"
SOURCE_FILES=$(find src/ include/ -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) 2>/dev/null || true)

if [[ -z "$SOURCE_FILES" ]]; then
    echo -e "${YELLOW}⚠ No C++ source files found in src/ and include/ directories${NC}"
    exit 0
fi

# Count files
FILE_COUNT=$(echo "$SOURCE_FILES" | wc -l)
echo "Found $FILE_COUNT C++ source files"
echo

# Format or check files
FORMATTED_COUNT=0
NEEDS_FORMATTING=0

if [[ "$CHECK_ONLY" == true ]]; then
    echo -e "${YELLOW}Checking code formatting (dry-run mode)...${NC}"
    echo
    
    for file in $SOURCE_FILES; do
        if [[ "$VERBOSE" == true ]]; then
            echo "Checking $file..."
        fi
        
        # Check if file needs formatting
        if ! clang-format --dry-run --Werror "$file" &>/dev/null; then
            echo -e "${YELLOW}  ⚠ $file needs formatting${NC}"
            ((NEEDS_FORMATTING++))
        elif [[ "$VERBOSE" == true ]]; then
            echo -e "${GREEN}  ✓ $file is properly formatted${NC}"
        fi
    done
    
    echo
    if [[ $NEEDS_FORMATTING -eq 0 ]]; then
        echo -e "${GREEN}✅ All files are properly formatted!${NC}"
        exit 0
    else
        echo -e "${YELLOW}⚠ $NEEDS_FORMATTING out of $FILE_COUNT files need formatting${NC}"
        echo "Run without --check to format them."
        exit 1
    fi
else
    echo -e "${YELLOW}Formatting C++ source files...${NC}"
    echo
    
    for file in $SOURCE_FILES; do
        if [[ "$VERBOSE" == true ]]; then
            echo "Formatting $file..."
        fi
        
        # Create backup and format
        cp "$file" "$file.bak"
        
        if clang-format -i "$file"; then
            # Check if file was actually changed
            if ! cmp -s "$file" "$file.bak"; then
                echo -e "${GREEN}  ✓ $file formatted${NC}"
                ((FORMATTED_COUNT++))
            elif [[ "$VERBOSE" == true ]]; then
                echo -e "${BLUE}  - $file already formatted${NC}"
            fi
        else
            echo -e "${RED}  ❌ Failed to format $file${NC}"
            # Restore backup on failure
            mv "$file.bak" "$file"
        fi
        
        # Remove backup
        rm -f "$file.bak"
    done
    
    echo
    if [[ $FORMATTED_COUNT -eq 0 ]]; then
        echo -e "${GREEN}✅ All files were already properly formatted!${NC}"
    else
        echo -e "${GREEN}✅ Successfully formatted $FORMATTED_COUNT out of $FILE_COUNT files${NC}"
        echo
        echo -e "${YELLOW}Tip: Review the changes with 'git diff' before committing${NC}"
    fi
fi

echo
echo -e "${BLUE}Code formatting completed.${NC}"