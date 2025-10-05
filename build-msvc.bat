@echo off
setlocal enabledelayedexpansion
REM Set up MSVC environment and build with CMake

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

set "LIBPATH=%MSVC_ROOT%\lib\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\ucrt\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\um\x64"

REM Clean and configure
REM echo Cleaning build directory...
REM if exist build-msvc rmdir /s /q build-msvc
REM mkdir build-msvc

echo Configuring with CMake...
cmake.exe . -B build-msvc ^
    -DCMAKE_TOOLCHAIN_FILE=H:/opt/ce-win-file-cache/msvc-toolchain.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -G "Ninja" ^
    -DCMAKE_MAKE_PROGRAM=ninja.exe

if !errorlevel! neq 0 (
    echo Configuration failed!
    exit /b 1
)

echo Building...
cmake.exe --build build-msvc --config Release

if !errorlevel! neq 0 (
    echo Build failed!
    exit /b 1
)

echo Build completed successfully!

echo Copying WinFsp DLL to executable directory...
copy "C:\PROGRA~2\WinFsp\bin\winfsp-x64.dll" "build-msvc\bin\"

echo Copying config file to executable directory...
copy "compilers.json" "build-msvc\bin\"

echo Setup complete!


@REM echo Running
@REM cd build-msvc\bin
@REM .\CeWinFileCacheFS.exe -d 1 --mount "M:" --log-level debug
