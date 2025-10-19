# Portable ZIP Package Structure

## Overview

The portable ZIP package has been restructured to have a cleaner organization with all DLLs in a dedicated `bin` folder while keeping the executable and README at the root.

## Directory Structure

```
kolosal-server/
├── kolosal-server.exe          # Main executable (root level)
├── start-kolosal-server.bat    # Launcher script (automatically sets PATH)
├── README.txt                  # Quick start guide
├── README.md                   # Full documentation
├── LICENSE                     # License file
├── bin/                        # All DLL dependencies
│   ├── kolosal_server.dll
│   ├── llama-cpu.dll           # CPU inference engine
│   ├── llama-vulkan.dll        # Vulkan inference engine (if available)
│   ├── libopenblas.dll
│   ├── libcurl.dll
│   └── ... (other runtime DLLs)
├── config/                     # Configuration files
│   ├── config.yaml
│   └── config.json
├── models/                     # Place model files here
├── logs/                       # Server logs
├── data/                       # Runtime data
│   └── faiss_index/
├── docs/                       # Documentation
└── static/                     # Web UI files
```

## Running the Server

### Option 1: Using the Launcher (Recommended)
Simply run `start-kolosal-server.bat` which automatically adds the `bin` folder to PATH.

### Option 2: Direct Execution
Run `kolosal-server.exe` directly. Windows will automatically search for DLLs in the same directory and the `bin` subdirectory.

## Building with Multiple Inference Engines

To include both CPU and Vulkan inference engines in the portable package:

### Step 1: Build with CPU backend
```powershell
cmake -B build -DUSE_VULKAN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Step 2: Copy llama-cpu.dll
```powershell
Copy-Item build\Release\llama-cpu.dll -Destination saved-dlls\
```

### Step 3: Build with Vulkan backend
```powershell
cmake -B build -DUSE_VULKAN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Step 4: Restore llama-cpu.dll
```powershell
Copy-Item saved-dlls\llama-cpu.dll -Destination build\Release\
```

### Step 5: Create portable package
```powershell
cmake --build build --config Release --target portable
```

The resulting ZIP will contain both `llama-cpu.dll` and `llama-vulkan.dll` in the `bin` folder.

## Automatic DLL Discovery

The build system automatically:
- Creates a `bin` folder in the build directory
- Copies all DLLs to both the root and `bin` folder during build
- Searches for required system DLLs (OpenBLAS, MinGW runtime, etc.)
- Includes both llama-cpu and llama-vulkan if present in the build directory

## DLL Search Order

The `start-kolosal-server.bat` launcher adds `bin` to PATH, ensuring:
1. DLLs in `bin` folder are found first
2. System DLLs are used as fallback
3. No need to modify system PATH permanently

## Notes

- The main executable remains at the root for easy access
- All DLL dependencies are isolated in the `bin` folder
- The launcher script ensures proper PATH configuration
- Both CPU and GPU (Vulkan) inference engines can coexist
- The server will automatically select the appropriate inference engine based on configuration
