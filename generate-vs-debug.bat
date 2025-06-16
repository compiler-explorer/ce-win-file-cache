@echo off
echo Generating Visual Studio solution for debugging...

REM Create build directory for Visual Studio
if not exist build-vs mkdir build-vs
cd build-vs

REM Generate Visual Studio 2019 solution with debug info
cmake -G "Visual Studio 16 2019" -A x64 ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DBUILD_DEBUG_TOOLS=ON ^
    -DENABLE_METRICS=ON ^
    ..

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b 1
)

echo.
echo Visual Studio solution generated successfully!
echo.
echo To debug:
echo 1. Open build-vs\CeWinFileCacheFS.sln in Visual Studio
echo 2. Set CeWinFileCacheFS as the startup project
echo 3. Set breakpoints in the code
echo 4. Press F5 to start debugging
echo.
echo Debug tools available:
echo - DirectoryCacheDebugTest: Test DirectoryCache in isolation
echo - IntegrationDebugTest: Test full integration
echo.
pause