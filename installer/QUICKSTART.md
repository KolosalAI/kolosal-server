# Quick Start Guide - Building Installers

This guide will help you quickly build installers for Kolosal Server on Windows.

## Prerequisites

Before you start, make sure you have:

1. **Built the Kolosal Server project** (or will build it as part of the process)
2. **PowerShell 5.1 or later** (comes with Windows 10/11)
3. **(Optional) NSIS** - Download from https://nsis.sourceforge.io/Download

## Quick Build Commands

### Option 1: Build Everything at Once (Recommended)

This builds the project and both installers in one command:

```powershell
cd installer
.\build-all-installers.ps1 -BuildProject -Version "1.0.0"
```

### Option 2: Build Just the ZIP Installer

If you've already built the project and just want the portable ZIP:

```powershell
cd installer
.\create-zip-installer.ps1
```

### Option 3: Build Just the NSIS Installer

If you've already built the project and want the professional installer:

```powershell
cd installer
.\build-nsis-installer.ps1
```

## Step-by-Step Instructions

### For Beginners

1. **Open PowerShell**
   - Press `Win + X` and select "Windows PowerShell" or "Terminal"

2. **Navigate to the project directory**
   ```powershell
   cd "D:\Works\Genta\codes\kolosal-server"
   ```

3. **Go to the installer directory**
   ```powershell
   cd installer
   ```

4. **Build everything**
   ```powershell
   .\build-all-installers.ps1 -BuildProject
   ```

5. **Wait for completion**
   - The script will build the project
   - Create the ZIP installer
   - Create the NSIS installer (if NSIS is installed)

6. **Find your installers**
   - ZIP: `build\kolosal-server-portable-1.0.0-win64.zip`
   - NSIS: `installer\kolosal-server-installer-1.0.0.exe`

### For Advanced Users

#### Build Only What You Need

```powershell
# Build only ZIP (skip NSIS)
.\build-all-installers.ps1 -SkipNsis

# Build only NSIS (skip ZIP)
.\build-all-installers.ps1 -SkipZip

# Use dist directory instead of Release
.\build-all-installers.ps1 -UseDist
```

#### Custom Version

```powershell
.\build-all-installers.ps1 -Version "1.2.3" -BuildProject
```

#### Use Custom Build Directory

```powershell
.\create-zip-installer.ps1 -BuildDir "..\custom\build\path"
```

## What Each Installer Does

### ZIP Portable Installer
- **File**: `kolosal-server-portable-{version}-win64.zip`
- **Size**: ~50-100 MB (depends on dependencies)
- **Use case**: 
  - Quick testing
  - Portable deployment
  - No installation required
  - Can run from USB drive

### NSIS Installer
- **File**: `kolosal-server-installer-{version}.exe`
- **Size**: ~50-100 MB (depends on dependencies)
- **Use case**:
  - Professional deployment
  - System-wide installation
  - Start menu shortcuts
  - Proper uninstallation

## Testing Your Installers

### Test ZIP Installer

1. Extract the ZIP to a test folder
2. Double-click `start-server.bat`
3. Open browser to http://localhost:8080/v1/health
4. Verify the server responds

### Test NSIS Installer

1. Run the `.exe` file
2. Follow the installation wizard
3. Launch from Start Menu
4. Open browser to http://localhost:8080/v1/health
5. Test uninstallation from Control Panel

## Common Issues and Solutions

### Issue: "Script cannot be loaded"
**Error**: `cannot be loaded because running scripts is disabled`

**Solution**:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Issue: "NSIS not found"
**Error**: `NSIS not found!`

**Solution**: Either:
1. Install NSIS from https://nsis.sourceforge.io/Download, OR
2. Skip NSIS and build only ZIP: `.\build-all-installers.ps1 -SkipNsis`

### Issue: "kolosal-server.exe not found"
**Error**: `kolosal-server.exe not found in any expected location`

**Solution**: Build the project first:
```powershell
cd ..\build
cmake --build . --config Release
cd ..\installer
```

Or use the auto-build option:
```powershell
.\build-all-installers.ps1 -BuildProject
```

### Issue: "CMake not found"
**Error**: `cmake is not recognized`

**Solution**: Install CMake from https://cmake.org/download/

### Issue: Missing DLLs
**Error**: The application fails to start with missing DLL errors

**Solution**: Make sure you're using Release build, not Debug:
```powershell
cmake --build build --config Release
```

## Directory Structure After Building

```
kolosal-server/
├── build/
│   ├── Release/
│   │   ├── kolosal-server.exe
│   │   └── *.dll
│   └── kolosal-server-portable-1.0.0-win64.zip ← ZIP installer here
├── installer/
│   ├── kolosal-server-installer-1.0.0.exe ← NSIS installer here
│   ├── script.nsi
│   ├── create-zip-installer.ps1
│   ├── build-nsis-installer.ps1
│   └── build-all-installers.ps1
└── ...
```

## Distribution Checklist

Before distributing your installers:

- [ ] Test on a clean Windows 10/11 machine
- [ ] Verify the server starts correctly
- [ ] Test all API endpoints
- [ ] Check that configuration files are included
- [ ] Verify documentation is accessible
- [ ] Test uninstallation (NSIS only)
- [ ] (Optional) Sign the NSIS installer
- [ ] Create release notes
- [ ] Upload to GitHub Releases or distribution platform

## Tips for Best Results

1. **Always build in Release mode** - Debug builds are much larger and slower
2. **Test on clean machine** - Your dev machine has dependencies that users might not have
3. **Include sample configs** - Make it easy for users to get started
4. **Keep documentation updated** - Users appreciate good docs
5. **Version your installers** - Use semantic versioning (e.g., 1.0.0)

## Need Help?

- Check the main [README.md](README.md) for detailed documentation
- Review the [CMakeLists.txt](../CMakeLists.txt) for build configuration
- Check logs in the build directory
- Create an issue on GitHub with:
  - Your Windows version
  - PowerShell version (`$PSVersionTable.PSVersion`)
  - Error messages
  - Build logs

## Advanced: Automation

### Create a Build Script

Create `build-release.ps1` in the project root:

```powershell
# Build and create installers in one go
param([string]$Version = "1.0.0")

Write-Host "Building Kolosal Server v$Version..." -ForegroundColor Cyan

# Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release

# Create installers
cd installer
.\build-all-installers.ps1 -Version $Version

Write-Host "Done! Check installer\ and build\ for installers" -ForegroundColor Green
```

Then run:
```powershell
.\build-release.ps1 -Version "1.0.0"
```

### GitHub Actions CI/CD

See [README.md](README.md) for GitHub Actions workflow examples to automate installer creation on every release.

---

**Happy Building! 🚀**

For more information, see the complete documentation in [README.md](README.md).
