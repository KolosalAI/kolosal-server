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

## Uninstallation

The installer creates an uninstaller that:
- Removes all installed files
- Cleans up registry entries
- Removes shortcuts
- Optionally keeps user data (logs, models, database)
