#!/bin/bash
# Kolosal CLI Launcher for Unix/Linux/macOS
# This script builds and launches the Kolosal CLI

echo "🚀 Kolosal CLI Launcher"
echo "===================="

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Build the project
echo "🔨 Building Kolosal Server..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_CLI=ON
if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed"
    exit 1
fi

make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo "✅ Build successful!"
echo "🎯 Starting Kolosal CLI..."
echo

# Launch CLI mode
./kolosal-server --cli
if [ $? -ne 0 ]; then
    echo "❌ CLI failed to start"
    exit 1
fi

cd ..
