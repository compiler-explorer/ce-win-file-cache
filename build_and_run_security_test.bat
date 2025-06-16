@echo off
REM Build and run security descriptor test program

echo Setting up MSVC environment...

REM MSVC and Windows SDK paths
set "MSVC_ROOT=D:\efs\compilers\msvc\14.40.33807-14.40.33811.0"
set "WINDOWS_SDK_ROOT=D:\efs\compilers\windows-kits-10"
set "WINDOWS_SDK_VERSION=10.0.22621.0"

REM Set up MSVC environment variables
set "PATH=%MSVC_ROOT%\bin\Hostx64\x64;%WINDOWS_SDK_ROOT%\bin\%WINDOWS_SDK_VERSION%\x64;%WINDOWS_SDK_ROOT%\bin\x64;%PATH%"
set "INCLUDE=%MSVC_ROOT%\include;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\ucrt;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\shared;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\um;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\winrt;%WINDOWS_SDK_ROOT%\Include\%WINDOWS_SDK_VERSION%\cppwinrt"
set "LIB=%MSVC_ROOT%\lib\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\ucrt\x64;%WINDOWS_SDK_ROOT%\Lib\%WINDOWS_SDK_VERSION%\um\x64"

echo Compiling security descriptor test...
cl.exe /nologo test_security_descriptors.cpp /Fe:test_security_descriptors.exe advapi32.lib

if %errorlevel% neq 0 (
    echo Compilation failed!
    exit /b 1
)

echo Compilation successful!
echo.
echo Running security descriptor test...
echo ===================================
test_security_descriptors.exe

if %errorlevel% neq 0 (
    echo Test execution failed!
    exit /b 1
)

echo.
echo Test completed successfully!