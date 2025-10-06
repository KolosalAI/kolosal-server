# Kolosal Server Installer

## Building the Installer

### Prerequisites

1. **NSIS (Nullsoft Scriptable Install System)**
   - Download from: https://nsis.sourceforge.io/Download
   - Add to PATH or use full path to `makensis.exe`

2. **Build the Project**
   ```powershell
   cd build
   cmake --build . --config Release
   ```

### Optional: OpenBLAS for CPU Acceleration

OpenBLAS is an optional dependency that provides optimized CPU computation for inference.

#### Downloading OpenBLAS

If you want to include OpenBLAS in the installer:

1. Download pre-built OpenBLAS for Windows:
   - From: https://github.com/OpenMathLib/OpenBLAS/releases
   - Choose the latest Windows binary (e.g., `OpenBLAS-0.3.xx-x64.zip`)

2. Extract the archive and copy `openblas.dll` (or `libopenblas.dll`) to:
   ```
   build/Release/openblas.dll
   ```

#### Building Without OpenBLAS

If OpenBLAS is not present, the installer will work fine without it. The server will use the default CPU implementation from llama.cpp which is still performant.

### Building the Installer

#### Using PowerShell Script

```powershell
cd installer
.\build-nsis-installer.ps1
```

#### Manual Build

```powershell
cd installer
makensis script.nsi
```

The installer will be created as `kolosal-server-installer-1.0.0.exe`

### Using CPack (Alternative)

You can also use CPack to create the package:

```powershell
cd build
cpack -C Release -G ZIP
```

## Installation Components

The installer includes the following components:

1. **Core Files** (Required)
   - Main executable (`kolosal-server.exe`)
   - Required DLLs (`kolosal_server.dll`, `libcurl.dll`, `llama-cpu.dll`)
   - Optional: `openblas.dll` (if available)

2. **Configuration Files**
   - Sample YAML and JSON configurations
   - Default configuration templates

3. **Documentation**
   - API guides
   - README and LICENSE

4. **Static Files**
   - Web interface files
   - HTML templates

5. **Development Headers** (Optional)
   - Header files for development

## Troubleshooting

### Missing DLL Errors

If users encounter "DLL not found" errors:

1. **Visual C++ Redistributable**
   - Most common issue
   - Download: https://aka.ms/vs/17/release/vc_redist.x64.exe

2. **OpenBLAS DLL** (Optional)
   - Only needed if built with BLAS support
   - Server will work without it using default CPU implementation
   - Can be downloaded separately and placed in installation directory

3. **libcurl.dll**
   - Should be included automatically from the build
   - If missing, check that curl is properly built

### Checking DLL Dependencies

Use the `dumpbin` tool to check dependencies:

```powershell
dumpbin /DEPENDENTS "path\to\kolosal-server.exe"
```

Or use [Dependencies](https://github.com/lucasg/Dependencies) GUI tool.

## Configuration After Installation

After installation, users should:

1. Edit `config.yaml` to set up:
   - Model paths
   - API keys (if needed)
   - Server settings (host, port)

2. Place model files in the `models` directory

3. Run the server:
   ```powershell
   cd "C:\Program Files\Kolosal Server"
   .\kolosal-server.exe
   ```

## Uninstallation

The installer creates an uninstaller that:
- Removes all installed files
- Cleans up registry entries
- Removes shortcuts
- Optionally keeps user data (logs, models, database)
