#!/bin/bash

set -e

BUILD_TYPE=Release
if [ "$1" = "debug" ]; then
    BUILD_TYPE=Debug
fi

echo "Building CompilerCacheFS with Wine ($BUILD_TYPE)..."

# Check if Wine development packages are installed
if ! command -v winegcc &> /dev/null; then
    echo "Error: Wine development tools not found. Please install:"
    echo "  Ubuntu/Debian: sudo apt-get install wine-dev"
    echo "  Fedora/RHEL:   sudo dnf install wine-devel"
    echo "  Arch:          sudo pacman -S wine"
    exit 1
fi

# Check for Wine headers
WINE_HEADERS_FOUND=0
for path in /usr/include/wine/windows /usr/local/include/wine/windows /opt/wine-stable/include/wine/windows /usr/include/wine-development/windows; do
    if [ -f "$path/windows.h" ]; then
        echo "Found Wine headers at: $path"
        WINE_HEADERS_FOUND=1
        break
    fi
done

if [ $WINE_HEADERS_FOUND -eq 0 ]; then
    echo "Error: Wine Windows headers not found. Please install wine-dev package."
    exit 1
fi

# Create build directory
mkdir -p build-wine
cd build-wine

# Configure with Wine
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DUSE_WINE_WINDOWS_API=ON \
    -DCMAKE_VERBOSE_MAKEFILE=ON

if [ $? -ne 0 ]; then
    echo "CMake configuration failed"
    exit 1
fi

# Build
cmake --build . --config $BUILD_TYPE -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

echo "Build completed successfully!"
echo "Executable: build-wine/bin/CompilerCacheFS"
echo ""
echo "To run with Wine:"
echo "  wine build-wine/bin/CompilerCacheFS.exe --help"

cd ..