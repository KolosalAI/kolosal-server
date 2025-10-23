# Portable ZIP Package Structure

## Overview

The portable ZIP package has been restructured to have a cleaner organization with all DLLs in a dedicated `bin` folder while keeping the executable and README at the root.

## Directory Structure

```
kolosal-server/
├── kolosal-server.exe          # Main executable
├── kolosal_server.dll          # Main server library (in root!)
├── llama-cpu.dll               # CPU inference engine (in root!)
├── llama-vulkan.dll            # Vulkan inference engine (in root, if available)
├── libopenblas.dll             # And all other runtime DLLs in root
├── libcurl.dll
├── msvcp140.dll
├── vcruntime140.dll
├── ... (other runtime DLLs)
├── start-kolosal-server.bat    # Launcher script (optional)
├── README.txt                  # Quick start guide
├── README.md                   # Full documentation
├── LICENSE                     # License file
├── bin/                        # Backup copy of all DLL dependencies
│   ├── kolosal_server.dll
│   ├── llama-cpu.dll
│   ├── llama-vulkan.dll
│   ├── libopenblas.dll
│   └── ... (all other DLLs)
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

### Option 1: Direct Execution (Recommended)
Simply double-click or run `kolosal-server.exe` directly. All required DLLs are in the same directory, so Windows will find them automatically.

### Option 2: Using the Launcher
Run `start-kolosal-server.bat` which adds the `bin` folder to PATH (useful if you move DLLs to the bin folder).

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
- Creates a `bin` folder in the build directory as backup
- Copies all DLLs to the root directory alongside the executable for direct execution
- Copies all DLLs to the `bin` folder as well for alternative deployment
- Searches for required system DLLs (OpenBLAS, MinGW runtime, etc.)
- Includes both llama-cpu and llama-vulkan if present in the build directory

## DLL Search Order

When running `kolosal-server.exe` directly:
1. Windows finds DLLs in the same directory (root) first - this is where all DLLs are now located
2. No launcher script needed
3. No need to modify system PATH

Alternatively, using `start-kolosal-server.bat`:
1. Adds `bin` folder to PATH for that session
2. Useful if you reorganize DLLs into the bin folder

## Notes

- **All DLL dependencies are now in the root directory alongside the executable**
- This provides the best out-of-the-box experience - just run the .exe
- A backup copy of all DLLs is also in the `bin` folder
- Both CPU and GPU (Vulkan) inference engines can coexist
- The server will automatically select the appropriate inference engine based on configuration
