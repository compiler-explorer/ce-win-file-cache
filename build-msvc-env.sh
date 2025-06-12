#!/bin/bash
# Script to build with MSVC using proper environment variables

# MSVC and Windows SDK paths
MSVC_ROOT="D:/efs/compilers/msvc/14.40.33807-14.40.33811.0"
WINDOWS_SDK_ROOT="D:/efs/compilers/windows-kits-10"
WINDOWS_SDK_VERSION="10.0.22621.0"

# Set up MSVC environment variables (equivalent to vcvars64.bat)
export PATH="$MSVC_ROOT/bin/Hostx64/x64:$WINDOWS_SDK_ROOT/bin/$WINDOWS_SDK_VERSION/x64:$WINDOWS_SDK_ROOT/bin/x64:D:/efs/compilers/ninja:D:/efs/cmake-3.31.7/bin:$PATH"

export INCLUDE="$MSVC_ROOT/include;$WINDOWS_SDK_ROOT/Include/$WINDOWS_SDK_VERSION/ucrt;$WINDOWS_SDK_ROOT/Include/$WINDOWS_SDK_VERSION/shared;$WINDOWS_SDK_ROOT/Include/$WINDOWS_SDK_VERSION/um;$WINDOWS_SDK_ROOT/Include/$WINDOWS_SDK_VERSION/winrt;$WINDOWS_SDK_ROOT/Include/$WINDOWS_SDK_VERSION/cppwinrt"

export LIB="$MSVC_ROOT/lib/x64;$WINDOWS_SDK_ROOT/Lib/$WINDOWS_SDK_VERSION/ucrt/x64;$WINDOWS_SDK_ROOT/Lib/$WINDOWS_SDK_VERSION/um/x64"

export LIBPATH="$MSVC_ROOT/lib/x64;$WINDOWS_SDK_ROOT/Lib/$WINDOWS_SDK_VERSION/ucrt/x64;$WINDOWS_SDK_ROOT/Lib/$WINDOWS_SDK_VERSION/um/x64"

# Clean and configure
echo "Cleaning build directory..."
rm -rf build-msvc
mkdir -p build-msvc
cd build-msvc

echo "Configuring with CMake..."
/mnt/d/efs/cmake-3.31.7/bin/cmake.exe .. \
    -DCMAKE_TOOLCHAIN_FILE=D:/opt/ce-win-file-cache/msvc-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -G "Ninja" \
    -DCMAKE_MAKE_PROGRAM=D:/efs/compilers/ninja/ninja.exe

if [ $? -eq 0 ]; then
    echo "Building..."
    /mnt/d/efs/cmake-3.31.7/bin/cmake.exe --build "D:/opt/ce-win-file-cache/build-msvc" --config Release
else
    echo "Configuration failed!"
    exit 1
fi