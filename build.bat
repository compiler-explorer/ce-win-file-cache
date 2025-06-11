@echo off
setlocal

set BUILD_TYPE=Release
if "%1"=="debug" set BUILD_TYPE=Debug

echo Building CompilerCacheFS (%BUILD_TYPE%)...

if not exist build mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

cmake --build . --config %BUILD_TYPE%
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo Build completed successfully!
echo Executable: build\bin\%BUILD_TYPE%\CompilerCacheFS.exe

cd ..