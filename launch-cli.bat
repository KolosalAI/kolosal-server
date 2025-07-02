@echo off
REM Kolosal CLI Launcher for Windows
REM This script builds and launches the Kolosal CLI

echo 🚀 Kolosal CLI Launcher
echo ====================

REM Check if build directory exists
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd build

REM Build the project
echo 🔨 Building Kolosal Server...
cmake .. -G "Visual Studio 17 2022" -A x64 -DENABLE_CLI=ON
if errorlevel 1 (
    echo ❌ CMake configuration failed
    pause
    exit /b 1
)

cmake --build . --config Release
if errorlevel 1 (
    echo ❌ Build failed
    pause
    exit /b 1
)

echo ✅ Build successful!
echo 🎯 Starting Kolosal CLI...
echo.

REM Launch CLI mode
Release\kolosal-server.exe --cli
if errorlevel 1 (
    echo ❌ CLI failed to start
    pause
    exit /b 1
)

cd ..
pause
