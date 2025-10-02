# Kolosal Server Installer

This directory contains scripts and configurations for creating installers for Kolosal Server on multiple platforms (Windows, macOS).

## Available Installers

### 1. ZIP Portable Installer (Recommended for Quick Distribution)

A portable ZIP package that requires no installation - just extract and run.

**Features:**
- No installation required
- Can run from any directory
- Includes all dependencies
- Easy to distribute
- Perfect for testing and portable deployments

**Create ZIP Installer:**
```powershell
cd installer
.\create-zip-installer.ps1
```

**Options:**
```powershell
# Use custom build directory
.\create-zip-installer.ps1 -BuildDir "..\build\Release"

# Use dist directory (if you ran cmake --build build --target dist)
.\create-zip-installer.ps1 -UseDist

# Specify custom output directory and version
.\create-zip-installer.ps1 -OutputDir "D:\Releases" -Version "1.0.1"
```

**Output:**
- `kolosal-server-portable-{version}-win64.zip`

### 2. NSIS Installer (Professional Installation Package)

A professional Windows installer with GUI, shortcuts, and uninstaller.

**Features:**
- Professional installation wizard
- Start menu integration
- Desktop shortcuts (optional)
- System PATH integration (optional)
- Auto-start capability (optional)
- Clean uninstallation
- Registry integration
- Component selection

**Prerequisites:**
- [NSIS (Nullsoft Scriptable Install System)](https://nsis.sourceforge.io/Download)
- Built Kolosal Server binaries

**Create NSIS Installer:**

**Option 1: Using PowerShell script (recommended)**
```powershell
cd installer
.\build-nsis-installer.ps1
```

**Option 2: Using NSIS directly**
```powershell
cd installer
makensis script.nsi
```

**Output:**
- `kolosal-server-installer-{version}.exe`

### 3. macOS DMG Installer (macOS Distribution)

A macOS disk image installer for easy distribution on macOS.

**Features:**
- Native macOS package format
- Drag-and-drop installation style
- Includes all dependencies
- Shell script for easy startup
- No installation required - extract and run
- Portable deployment option

**Prerequisites:**
- macOS system with `hdiutil` (or Linux/WSL for tar.gz fallback)
- Built Kolosal Server binaries
- Bash shell

**Create macOS DMG Installer:**

**Option 1: Using the standalone script**
```bash
cd installer
chmod +x create-dmg-installer.sh
./create-dmg-installer.sh
```

**Option 2: Using the master build script**
```bash
cd installer
chmod +x build-mac-installer.sh
./build-mac-installer.sh
```

**Options:**
```bash
# Specify version
./create-dmg-installer.sh -v 1.0.1

# Use dist directory
./create-dmg-installer.sh -d

# Use custom build directory
./create-dmg-installer.sh -b ../build/Release

# Custom output directory
./create-dmg-installer.sh -o /path/to/output
```

**Output:**
- `kolosal-server-{version}-macOS.dmg` (on macOS with hdiutil)
- `kolosal-server-{version}-macOS.tar.gz` (fallback on Linux/WSL)

**On Windows:**
You can use WSL or Git Bash to run the macOS build scripts:
```powershell
# Using WSL
wsl bash ./create-dmg-installer.sh -v 1.0.0

# Using Git Bash
bash ./create-dmg-installer.sh -v 1.0.0
```

## Building Instructions

### Step 1: Build Kolosal Server

**For Windows:**
```powershell
# From project root
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

**For macOS/Linux:**
```bash
# From project root
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Step 2: Choose Your Installer Type

#### For ZIP Installer:

```powershell
cd ..\installer
.\create-zip-installer.ps1
```

The ZIP will be created in `build\kolosal-server-portable-1.0.0-win64.zip`

#### For NSIS Installer:

```powershell
cd ..\installer
.\build-nsis-installer.ps1
```

Or manually:
```powershell
makensis script.nsi
```

The installer will be created as `kolosal-server-installer-1.0.0.exe`

#### For macOS DMG Installer:

```bash
cd installer
chmod +x build-mac-installer.sh
./build-mac-installer.sh
```

Or directly:
```bash
chmod +x create-dmg-installer.sh
./create-dmg-installer.sh -v 1.0.0
```

The installer will be created as `kolosal-server-1.0.0-macOS.dmg`

### Step 3: Build All Installers at Once

**Windows (builds ZIP + NSIS + macOS via WSL/Bash):**
```powershell
cd installer
.\build-all-installers.ps1 -Version "1.0.0"
```

**macOS/Linux (builds DMG/tar.gz):**
```bash
cd installer
chmod +x build-mac-installer.sh
./build-mac-installer.sh --version 1.0.0
```

**Options for build-all-installers.ps1:**
```powershell
# Build everything including project
.\build-all-installers.ps1 -Version "1.0.0" -BuildProject

# Skip specific installers
.\build-all-installers.ps1 -SkipZip      # Skip ZIP
.\build-all-installers.ps1 -SkipNsis     # Skip NSIS
.\build-all-installers.ps1 -SkipMac      # Skip macOS

# Use dist directory
.\build-all-installers.ps1 -UseDist
```

## Installer Contents

All installers include:

### Core Files (Required)
- `kolosal-server.exe` / `kolosal-server` - Main executable (Windows/macOS)
- `*.dll` (Windows) / `*.dylib` (macOS) - Required runtime libraries
- `lib/` - Additional shared libraries

### Configuration Files
- `configs/config.yaml` - Server configuration
- `configs/config.json` - Alternative JSON config
- `configs/config_rms.yaml` - RMS configuration
- `configs/local-retrieval-config.yaml` - Local retrieval settings

### Documentation
- `README.md` - Main documentation
- `LICENSE` - License information
- `docs/` - API guides and developer documentation

### Assets
- `assets/icon.ico` - Application icon
- `assets/logo.png` - Logo image

### Runtime Directories
- `data/faiss_index/` - FAISS vector database indexes
- `logs/` - Server log files
- `models/` - Model files storage
- `static/` - Static web files (if applicable)

## NSIS Installer Components

The NSIS installer allows users to choose which components to install:

1. **Core Files** (Required) - Main application and libraries
2. **Configuration Files** - Sample configuration files
3. **Documentation** - User and developer documentation
4. **Static Files** - Web interface files
5. **Development Headers** - C++ header files for development

## NSIS Installation Options

During installation, users can choose:

- ✓ Create Desktop Shortcut
- ✓ Add to System PATH
- ☐ Start automatically at login

## Customization

### Updating Version

**For ZIP Installer:**
```powershell
.\create-zip-installer.ps1 -Version "1.0.1"
```

**For NSIS Installer:**
Edit `script.nsi` and change:
```nsis
!define PRODUCT_VERSION "1.0.1"
```

### Changing Installer Icon

Replace `assets/icon.ico` with your custom icon (must be .ico format)

### Modifying Installation Directory

Edit `script.nsi`:
```nsis
InstallDir "$PROGRAMFILES64\Your Custom Path"
```

### Adding New Files

**For ZIP Installer:**
Edit `create-zip-installer.ps1` and add to the appropriate copy section.

**For NSIS Installer:**
Edit `script.nsi` in the appropriate section:
```nsis
Section "Core Files" SecCore
  ; Add your files here
  File "path\to\your\file.ext"
SectionEnd
```

## Distribution

### ZIP Installer (Windows)
1. Upload to GitHub Releases or your distribution platform
2. Users download and extract
3. Users run `start-server.bat` or `kolosal-server.exe`

### NSIS Installer (Windows)
1. Upload `.exe` to GitHub Releases or your distribution platform
2. Users download and run the installer
3. Follow the installation wizard
4. Launch from Start Menu or Desktop shortcut

### macOS DMG Installer
1. Upload `.dmg` to GitHub Releases or your distribution platform
2. Users download and mount the DMG
3. Users drag the folder to Applications or run directly
4. Run `./start-server.sh` from Terminal

## Platform-Specific Notes

### macOS

**Security Settings:**
On macOS, users may need to allow the app in Security & Privacy settings:
1. System Preferences > Security & Privacy
2. Click "Allow Anyway" for kolosal-server

**Making Scripts Executable:**
```bash
chmod +x kolosal-server
chmod +x start-server.sh
```

**Code Signing (Optional but Recommended):**
For production releases, sign with Apple Developer ID:
```bash
codesign --deep --force --verify --verbose --sign "Developer ID Application: Your Name" kolosal-server
```

**Notarization (Required for macOS 10.15+):**
```bash
# Create a zip for notarization
ditto -c -k --keepParent kolosal-server.app kolosal-server.zip

# Submit for notarization
xcrun altool --notarize-app --file kolosal-server.zip --primary-bundle-id com.kolosalai.server

# Staple the notarization ticket
xcrun stapler staple kolosal-server.app
```

### Windows

**Code Signing (Optional but Recommended):**
```powershell
signtool sign /f "certificate.pfx" /p "password" /t http://timestamp.digicert.com kolosal-server.exe
```

### Linux

The macOS scripts can also create tar.gz packages that work on Linux:
```bash
./create-dmg-installer.sh -v 1.0.0
# This will create a .tar.gz file if hdiutil is not available
```

## Troubleshooting

### ZIP Installer Issues

**Problem:** Script can't find executable
**Solution:** Build the project first:
```powershell
cmake --build build --config Release
```

**Problem:** Missing DLL files
**Solution:** Make sure all dependencies are built and in the Release directory

### NSIS Installer Issues

**Problem:** NSIS not found
**Solution:** Install NSIS from https://nsis.sourceforge.io/Download

**Problem:** Can't find assets/icon.ico
**Solution:** Ensure the icon file exists in the assets directory

**Problem:** Files not found during compilation
**Solution:** Check the file paths in `script.nsi` relative to the installer directory

### macOS Installer Issues

**Problem:** Permission denied when running scripts
**Solution:** Make scripts executable:
```bash
chmod +x create-dmg-installer.sh
chmod +x build-mac-installer.sh
```

**Problem:** hdiutil not found on Linux/Windows
**Solution:** This is normal - the script will automatically create a .tar.gz instead of .dmg

**Problem:** Application won't run due to security settings
**Solution:** 
```bash
# On macOS, allow in System Preferences > Security & Privacy
# Or remove quarantine attribute:
xattr -d com.apple.quarantine kolosal-server
```

**Problem:** Bash not available on Windows
**Solution:** Install WSL or Git Bash:
```powershell
# Enable WSL
wsl --install

# Or download Git Bash from https://git-scm.com/
```

### General Issues

**Problem:** Executable doesn't run after installation
**Solution:** Check that all DLL dependencies are included

**Problem:** Configuration file errors
**Solution:** Validate YAML/JSON syntax in config files

## Testing Installers

### ZIP Installer Testing (Windows)
1. Extract to a test directory
2. Run `start-server.bat`
3. Verify server starts on http://localhost:8080
4. Check logs in `logs/` directory

### NSIS Installer Testing (Windows)
1. Install to a test directory
2. Verify shortcuts are created
3. Launch from Start Menu
4. Verify server functionality
5. Test uninstallation

### macOS DMG Installer Testing
1. Mount the DMG file
2. Copy contents to a test directory
3. Make scripts executable: `chmod +x kolosal-server start-server.sh`
4. Run `./start-server.sh`
5. Verify server starts on http://localhost:8080
6. Check logs in `logs/` directory

## Requirements

### For Building ZIP Installer
- PowerShell 5.1 or later
- Built Kolosal Server binaries

### For Building NSIS Installer
- NSIS 3.0 or later
- Built Kolosal Server binaries
- Assets (icon.ico, logo.png)

### For Building macOS DMG Installer
- macOS system with hdiutil (for .dmg) OR
- Linux/WSL/Git Bash (for .tar.gz fallback)
- Bash shell
- Built Kolosal Server binaries

### For Running the Application

**Windows:**
- Windows 10/11 (64-bit)
- Visual C++ Redistributable 2019 or later
- At least 4GB RAM (8GB+ recommended)
- Optional: NVIDIA GPU with CUDA support

**macOS:**
- macOS 10.13 or later
- At least 4GB RAM (8GB+ recommended)
- Optional: Apple Silicon or Intel CPU with GPU support

## Advanced Options

### Creating MSI Installer

For enterprise deployment, you can use WiX Toolset:

```powershell
# Install WiX
# Then use CMake's CPack
cd build
cpack -G WIX
```

### Signing the Installer

For production releases, sign your installer:

```powershell
signtool sign /f "certificate.pfx" /p "password" /t http://timestamp.digicert.com kolosal-server-installer.exe
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Build Installer

on:
  release:
    types: [created]

jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build Project
        run: |
          mkdir build
          cd build
          cmake -G "Visual Studio 17 2022" -A x64 ..
          cmake --build . --config Release
      - name: Create ZIP Installer
        run: |
          cd installer
          .\create-zip-installer.ps1 -Version "${{ github.event.release.tag_name }}"
      - name: Upload Release Asset
        uses: actions/upload-release-asset@v1
        with:
          upload_url: ${{ github.event.release.upload_url }}
          asset_path: ./build/kolosal-server-portable-${{ github.event.release.tag_name }}-win64.zip
          asset_name: kolosal-server-portable-${{ github.event.release.tag_name }}-win64.zip
          asset_content_type: application/zip
```

## Support

For issues with the installers:
- Check the [troubleshooting section](#troubleshooting)
- Review the build logs
- Create an issue on GitHub with details about your build environment

## License

The installer scripts are part of Kolosal Server and are licensed under the Apache License 2.0.

