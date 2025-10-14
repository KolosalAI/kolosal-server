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

### Optional: Vulkan GPU Acceleration

Vulkan provides cross-platform GPU acceleration for inference.

#### Installing Vulkan SDK

If you want to include Vulkan support in the installer:

1. Download and install the Vulkan SDK:
   - From: https://vulkan.lunarg.com/sdk/home
   - Download the latest Windows installer
   - Run the installer and follow the setup wizard
   - The installer will set up the required environment variables

2. Verify Vulkan installation:
   ```powershell
   $env:VULKAN_SDK
   # Should show the SDK installation path
   ```

#### Building with Vulkan Support

**Option 1: Using the build script (Recommended)**
```powershell
cd installer
.\build-nsis-installer.ps1 -EnableVulkan
```

This will:
- Configure the build with `-DUSE_VULKAN=ON`
- Build both `llama-cpu.dll` and `llama-vulkan.dll`
- Package both DLLs in the installer
- Include `vulkan-1.dll` if available

**Option 2: Manual CMake configuration**
```powershell
cd build
cmake .. -DUSE_VULKAN=ON
cmake --build . --config Release
```

The Vulkan-enabled build will produce:
- `llama-cpu.dll` in `build/Release/` (always built as fallback)
- `llama-vulkan.dll` in `build/Release/` (for GPU acceleration)
- Both DLLs will be automatically included in the installer

#### Building Without Vulkan (CPU-only)

If Vulkan SDK is not installed, use the standard build:
```powershell
cd installer
.\build-nsis-installer.ps1
```

This will:
- Build only `llama-cpu.dll` (required for basic operation)
- Package CPU inference engine in the installer
- The server will use CPU-only inference

**Important:** The `llama-cpu.dll` is **required** for the installer to work properly. If this file is missing:
1. The installer will display a warning during installation
2. The server will fail to load models
3. You must rebuild the project to generate this DLL

To ensure `llama-cpu.dll` is built:
```powershell
cd build
cmake ..
cmake --build . --config Release --target llama-cpu
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

**Standard Build (CPU only):**
```powershell
cd installer
.\build-nsis-installer.ps1
```

**With Vulkan Support:**
```powershell
cd installer
.\build-nsis-installer.ps1 -EnableVulkan
```

**Additional Options:**
```powershell
# Skip build check
.\build-nsis-installer.ps1 -SkipBuildCheck

# Custom NSIS path
.\build-nsis-installer.ps1 -NsisPath "C:\Path\To\NSIS\makensis.exe"

# Custom version
.\build-nsis-installer.ps1 -Version "1.0.1"

# Combined options
.\build-nsis-installer.ps1 -EnableVulkan -Version "1.0.1"
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

## Installation Structure

The installer creates the following directory structure:

```
C:\Program Files\Kolosal Server\
├── kolosal-server.exe          # Main executable
├── *.dll                        # Core library dependencies
├── lib\
│   ├── libllama-cpu.dll        # CPU inference engine
│   ├── libllama-vulkan.dll     # Vulkan GPU engine (if built with Vulkan)
│   └── libllama-cuda.dll       # CUDA GPU engine (if built with CUDA)
├── assets\                      # Application icons and resources
├── configs\                     # Sample configuration files
├── static\                      # Web UI files
├── docs\                        # Documentation
└── data\                        # Runtime data directory

C:\ProgramData\Kolosal\
├── config.yaml                  # System-wide configuration
├── bin\
│   ├── libllama-cpu.dll        # Backup inference engines
│   ├── libllama-vulkan.dll
│   └── libllama-cuda.dll
├── models\                      # Model storage
├── data\
│   └── faiss_index\            # Vector database
└── logs\                        # Application logs
```

**Important:** The inference engine DLLs MUST be in the `lib/` subdirectory of the installation directory for the server to find them automatically.

## Installation Components

The installer includes the following components:

1. **Core Files** (Required)
   - Main executable (`kolosal-server.exe`)
   - Required DLLs (`kolosal_server.dll`, `libcurl.dll`)
   - Inference engines in `lib/` subdirectory:
     - `lib/libllama-cpu.dll` (CPU inference)
     - `lib/libllama-vulkan.dll` (Vulkan GPU, if built with Vulkan support)
     - `lib/libllama-cuda.dll` (CUDA GPU, if built with CUDA support)
   - Optional: `openblas.dll` (for CPU acceleration)

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

2. **Vulkan Runtime** (For Vulkan builds)
   - Required if using `llama-vulkan.dll`
   - Download: https://vulkan.lunarg.com/sdk/home
   - Or install GPU drivers with Vulkan support
   - Server will fall back to CPU if Vulkan is unavailable

3. **OpenBLAS DLL** (Optional)
   - Only needed if built with BLAS support
   - Server will work without it using default CPU implementation
   - Can be downloaded separately and placed in installation directory

4. **libcurl.dll**
   - Should be included automatically from the build
   - If missing, check that curl is properly built

### Checking DLL Dependencies

Use the `dumpbin` tool to check dependencies:

```powershell
dumpbin /DEPENDENTS "path\to\kolosal-server.exe"
```

Or use [Dependencies](https://github.com/lucasg/Dependencies) GUI tool.

## Configuration Management (Updated)

### Configuration File Priority

The server loads configuration in this order:
1. **User Config** (Highest): `%APPDATA%\Kolosal\config.yaml`
2. **System Config** (Recommended): `%PROGRAMDATA%\Kolosal\config.yaml`
3. **Install Dir**: `C:\Program Files\Kolosal Server\config.yaml`
4. **Working Dir**: `.\config.yaml`

### What the Installer Does

The installer now:
1. Creates fresh config at: `%PROGRAMDATA%\Kolosal\config.yaml`
2. Backs up old user config if found: `%APPDATA%\Kolosal\backup\config.yaml.backup`
3. Installs inference engine DLLs to: `%PROGRAMDATA%\Kolosal\bin\`
4. Creates data directories: `%PROGRAMDATA%\Kolosal\data\`
5. Notifies user about old configuration backups

### Installation-Ready Configuration

The installer uses `config-install.yaml` which:
- Has paths pre-configured for `%PROGRAMDATA%\Kolosal\`
- Uses safe defaults (localhost-only, no public access)
- Has empty models list (users add their own)
- Points to installed inference engines in `%PROGRAMDATA%\Kolosal\bin\`

### After Installation

Users should:

1. **Review** the Post-Installation Guide (`POST_INSTALL_README.md`)

2. **For fresh installation**: Edit the config at:
   ```
   %PROGRAMDATA%\Kolosal\config.yaml
   ```
   Or: `C:\ProgramData\Kolosal\config.yaml`

3. **For upgrade**: Either:
   - Delete old config at: `%APPDATA%\Kolosal\config.yaml` to use fresh config
   - Or update old config with new paths from the backup location

4. **Add models** to the config or place in: `C:\ProgramData\Kolosal\models\`

5. **Run the server**:
   ```powershell
   cd "C:\Program Files\Kolosal Server"
   .\kolosal-server.exe
   ```

### Configuration Shortcuts

The installer creates Start Menu shortcuts to:
- Configuration file: Direct link to `%PROGRAMDATA%\Kolosal\config.yaml`
- Post-Installation Guide: Setup instructions
- Documentation: Full documentation

## GPU Acceleration Support

### Vulkan

Vulkan provides cross-platform GPU acceleration and works with most modern GPUs (NVIDIA, AMD, Intel).

**Advantages:**
- Cross-platform (Windows, Linux, macOS)
- Works with most GPU vendors
- Good performance for inference
- No proprietary drivers required (uses standard GPU drivers)

**Requirements:**
- Vulkan-capable GPU
- Updated GPU drivers with Vulkan support
- Vulkan SDK (for building only, not required for end users with modern GPU drivers)

**To build with Vulkan:**
```powershell
.\build-nsis-installer.ps1 -EnableVulkan
```

**To verify Vulkan support after installation:**
```powershell
# Check if llama-vulkan.dll exists
Test-Path "C:\Program Files\Kolosal Server\llama-vulkan.dll"

# Check Vulkan runtime availability
vulkaninfo
```

### CUDA (Alternative)

For NVIDIA GPUs, CUDA can also be used instead of Vulkan:

**To build with CUDA:**
```powershell
cd build
cmake .. -DUSE_CUDA=ON
cmake --build . --config Release
```

Note: CUDA requires NVIDIA GPU and CUDA Toolkit installation.

## Uninstallation

The installer creates an uninstaller that:
- Removes all installed files
- Cleans up registry entries
- Removes shortcuts
- Optionally keeps user data (logs, models, database)
