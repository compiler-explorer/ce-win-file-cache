#!/bin/bash

# Simple Wine compilation attempt
# Note: Full compilation may fail due to WinFsp Windows-specific dependencies

echo "Attempting Wine compilation (experimental)..."
echo "Note: WinFsp requires Windows-specific headers that Wine may not provide"
echo ""

# Format all source files before compilation
echo "Formatting source files with clang-format..."
find src include/compiler_cache include/wine_stubs types utils -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
echo "Formatting complete."
echo ""

# Create build directory  
mkdir -p build-wine
cd build-wine

# Configure
echo "Configuring..."
cmake .. \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    -DUSE_WINE_WINDOWS_API=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=winegcc \
    -DCMAKE_CXX_COMPILER=wineg++

# Try to build
echo "Building (this may fail due to WinFsp dependencies)..."
make -j1

echo ""
echo "Note: Wine compilation is experimental."
echo "For full functionality, use native Windows compilation."
echo "Wine is best used for testing individual components."
