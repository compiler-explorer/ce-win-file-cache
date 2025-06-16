@echo off
REM Build with cache tests enabled

echo Setting up MSVC environment...

REM Set temp directory to H:\tmp to avoid disk space issues
set "TMP=H:\tmp"
set "TEMP=H:\tmp"
if not exist H:\tmp mkdir H:\tmp

REM MSVC and Windows SDK paths
set "MSVC_ROOT=D:\efs\compilers\msvc\14.40.33807-14.40.33811.0"
set "WINDOWS_SDK_ROOT=D:\efs\compilers\windows-kits-10"
set "WINDOWS_SDK_VERSION=10.0.22621.0"

REM Set up MSVC environment variables (equivalent to vcvars64.bat)
set "PATH=%MSVC_ROOT%\bin\Hostx64\x64;%WINDOWS_SDK_ROOT%\bin\%WINDOWS_SDK_VERSION%\x64;%WINDOWS_SDK_ROOT%\bin\x64;D:\efs\compilers\ninja;D:\efs\cmake-3.31.7\bin;C:\PROGRA~2\WinFsp\bin;%PATH%"

set "INCLUDE=%MSVC_ROOT%\include;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\ucrt;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\shared;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\um;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\winrt;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\cppwinrt"

set "LIB=%MSVC_ROOT%\lib\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\ucrt\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\um\x64"

REM Visual Studio identification  
set "LIBPATH=%MSVC_ROOT%\lib\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\ucrt\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\um\x64"

echo Configuring with CMake (with tests)...
cmake.exe . -B build-msvc ^
    -DCMAKE_TOOLCHAIN_FILE=D:/opt/ce-win-file-cache/msvc-toolchain.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_CACHE_TEST=ON ^
    -G "Ninja" ^
    -DCMAKE_MAKE_PROGRAM=ninja.exe

if %errorlevel% neq 0 (
    echo Configuration failed!
    exit /b 1
)

echo Building all targets...
cmake.exe --build build-msvc --config Release

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Build completed successfully!
echo Test executables are in build-msvc/bin/