# Kolosal Server DMG Installer Creator for macOS (PowerShell Version)
# This script creates a DMG package of Kolosal Server for macOS
# Usage: .\create-dmg-installer.ps1 [-BuildDir <path>] [-OutputDir <path>] [-Version <version>] [-UseDist]

param(
    [string]$BuildDir = "../build/Release",
    [string]$OutputDir = "../build",
    [string]$Version = "1.0.0",
    [switch]$UseDist = $false
)

# Color output functions
function Write-Success {
    param([string]$Message)
    Write-Host "✓ $Message" -ForegroundColor Green
}

function Write-Info {
    param([string]$Message)
    Write-Host "ℹ $Message" -ForegroundColor Cyan
}

function Write-Warning {
    param([string]$Message)
    Write-Host "⚠ $Message" -ForegroundColor Yellow
}

function Write-ErrorMsg {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor Red
}

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "   $Message" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
}

# Main script
Write-Header "Kolosal Server DMG Installer Creator v$Version"

# Determine source directory
if ($UseDist) {
    $SourceDir = "../build/dist"
    Write-Info "Using dist directory: $SourceDir"
} else {
    $SourceDir = $BuildDir
    Write-Info "Using build directory: $SourceDir"
}

# Check if source directory exists
if (-not (Test-Path $SourceDir)) {
    Write-ErrorMsg "Source directory not found: $SourceDir"
    Write-Info "Please build the project first or use -UseDist flag if you've run 'cmake --build build --target dist'"
    exit 1
}

# Create output directory if it doesn't exist
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Define paths
$AppName = "Kolosal Server"
$StagingDir = Join-Path $OutputDir "kolosal-server-staging-$Version"
$DmgName = "kolosal-server-$Version-macOS.dmg"
$DmgPath = Join-Path $OutputDir $DmgName
$VolumeName = "Kolosal Server $Version"

Write-Info "Source: $SourceDir"
Write-Info "Output: $OutputDir"
Write-Info "Staging: $StagingDir"
Write-Host ""

# Clean up existing staging directory
if (Test-Path $StagingDir) {
    Write-Info "Cleaning up existing staging directory..."
    Remove-Item -Path $StagingDir -Recurse -Force
}

# Create staging directory structure
Write-Info "Creating directory structure..."
New-Item -ItemType Directory -Path $StagingDir -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir/lib" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir/configs" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir/docs" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir/assets" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir/data/faiss_index" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir/logs" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir/models" -Force | Out-Null
Write-Success "Directory structure created"

# Copy executable and libraries
Write-Info "Copying executable and libraries..."
$ExeFound = $false
$DylibCount = 0

# Determine executable extensions based on platform
$ExeExtensions = @("", ".exe")  # Try without extension first (macOS/Linux), then with .exe (Windows)

# Try multiple locations for the executable
$ExeLocations = @(
    "$SourceDir/kolosal-server",
    "$SourceDir/bin/kolosal-server",
    "../build/dist/kolosal-server",
    "../build/Release/kolosal-server",
    "../build/Debug/kolosal-server"
)

foreach ($exePath in $ExeLocations) {
    foreach ($ext in $ExeExtensions) {
        $fullPath = $exePath + $ext
        if (Test-Path $fullPath) {
            $targetName = "kolosal-server" + $ext
            Copy-Item $fullPath -Destination "$StagingDir/$targetName" -Force
            Write-Success "Copied $targetName from $(Split-Path $fullPath -Parent)"
            $ExeFound = $true
            break
        }
    }
    if ($ExeFound) { break }
}

if (-not $ExeFound) {
    Write-ErrorMsg "kolosal-server executable not found in any expected location"
    Write-Warning "Tried locations with extensions (.exe): $($ExeLocations -join ', ')"
    Write-Info "Please build the project first"
    exit 1
}

# Copy dylib/dll files from source directory
$LibExtensions = @("*.dylib", "*.dll", "*.so")
$LibCount = 0
foreach ($ext in $LibExtensions) {
    $LibFiles = Get-ChildItem -Path $SourceDir -Filter $ext -ErrorAction SilentlyContinue
    foreach ($lib in $LibFiles) {
        Copy-Item $lib.FullName -Destination "$StagingDir/" -Force
        $LibCount++
    }
}
$DylibCount = $LibCount

# Copy lib directory if it exists
if (Test-Path "$SourceDir/lib") {
    $LibFiles = Get-ChildItem -Path "$SourceDir/lib" -Recurse -File -ErrorAction SilentlyContinue
    foreach ($libFile in $LibFiles) {
        $RelativePath = $libFile.FullName.Substring((Resolve-Path "$SourceDir/lib").Path.Length + 1)
        $DestPath = Join-Path "$StagingDir/lib" $RelativePath
        $DestDir = Split-Path $DestPath -Parent
        if (-not (Test-Path $DestDir)) {
            New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
        }
        Copy-Item $libFile.FullName -Destination $DestPath -Force
    }
    $LibCount = (Get-ChildItem -Path "$StagingDir/lib" -Recurse -File).Count
    if ($LibCount -gt 0) {
        Write-Success "Copied $LibCount files from lib directory"
    }
}

Write-Success "Copied $DylibCount dylib files"

# Copy configuration files
Write-Info "Copying configuration files..."
$ConfigCount = 0
$ConfigFiles = @(
    "../configs/config.yaml",
    "../configs/config.json",
    "../configs/config_rms.yaml",
    "../configs/local-retrieval-config.yaml"
)

foreach ($config in $ConfigFiles) {
    if (Test-Path $config) {
        Copy-Item $config -Destination "$StagingDir/configs/" -Force
        $ConfigCount++
    }
}
Write-Success "Copied $ConfigCount configuration files"

# Copy documentation
Write-Info "Copying documentation..."
$DocCount = 0
$DocFiles = @(
    "../README.md",
    "../LICENSE",
    "../changes.log"
)

foreach ($doc in $DocFiles) {
    if (Test-Path $doc) {
        Copy-Item $doc -Destination "$StagingDir/" -Force
        $DocCount++
    }
}

# Copy docs directory
if (Test-Path "../docs") {
    $DocsFiles = Get-ChildItem -Path "../docs" -Recurse -File -ErrorAction SilentlyContinue
    foreach ($docFile in $DocsFiles) {
        $RelativePath = $docFile.FullName.Substring((Resolve-Path "../docs").Path.Length + 1)
        $DestPath = Join-Path "$StagingDir/docs" $RelativePath
        $DestDir = Split-Path $DestPath -Parent
        if (-not (Test-Path $DestDir)) {
            New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
        }
        Copy-Item $docFile.FullName -Destination $DestPath -Force
    }
    $DocsCount = (Get-ChildItem -Path "$StagingDir/docs" -Recurse -File).Count
    if ($DocsCount -gt 0) {
        $DocCount += $DocsCount
    }
}
Write-Success "Copied $DocCount documentation files"

# Copy assets
Write-Info "Copying assets..."
$AssetCount = 0
if (Test-Path "../assets") {
    $AssetFiles = Get-ChildItem -Path "../assets" -Include "*.ico", "*.png", "*.icns" -File -ErrorAction SilentlyContinue
    foreach ($asset in $AssetFiles) {
        Copy-Item $asset.FullName -Destination "$StagingDir/assets/" -Force -ErrorAction SilentlyContinue
        $AssetCount++
    }
}
Write-Success "Copied $AssetCount asset files"

# Copy static files if they exist
if (Test-Path "../static") {
    Write-Info "Copying static web files..."
    New-Item -ItemType Directory -Path "$StagingDir/static" -Force | Out-Null
    Copy-Item -Path "../static/*" -Destination "$StagingDir/static/" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Success "Static files copied"
}

# Create startup scripts
Write-Info "Creating startup scripts..."

# Bash script for macOS/Linux
$StartServerScript = @"
#!/bin/bash
echo "========================================"
echo "   Kolosal Server v$Version"
echo "========================================"
echo ""
echo "Starting Kolosal Server..."
echo "Server will be available at http://localhost:8080"
echo ""
echo "Press Ctrl+C to stop the server"
echo ""

# Get the directory where the script is located
SCRIPT_DIR="`$( cd "`$( dirname "`${BASH_SOURCE[0]}" )" && pwd )"
cd "`$SCRIPT_DIR"

./kolosal-server
"@

Set-Content -Path "$StagingDir/start-server.sh" -Value $StartServerScript -NoNewline
Write-Success "Created start-server.sh"

# PowerShell script for Windows
$StartServerPS = @"
# Kolosal Server Startup Script
Write-Host "========================================"
Write-Host "   Kolosal Server v$Version"
Write-Host "========================================"
Write-Host ""
Write-Host "Starting Kolosal Server..."
Write-Host "Server will be available at http://localhost:8080"
Write-Host ""
Write-Host "Press Ctrl+C to stop the server"
Write-Host ""

# Get the directory where the script is located
`$ScriptDir = Split-Path -Parent `$MyInvocation.MyCommand.Path
Set-Location `$ScriptDir

if (Test-Path "./kolosal-server.exe") {
    ./kolosal-server.exe
} elseif (Test-Path "./kolosal-server") {
    ./kolosal-server
} else {
    Write-Host "Error: kolosal-server executable not found!" -ForegroundColor Red
    exit 1
}
"@

Set-Content -Path "$StagingDir/start-server.ps1" -Value $StartServerPS
Write-Success "Created start-server.ps1"

# Batch script for Windows
$StartServerBat = @"
@echo off
echo ========================================
echo    Kolosal Server v$Version
echo ========================================
echo.
echo Starting Kolosal Server...
echo Server will be available at http://localhost:8080
echo.
echo Press Ctrl+C to stop the server
echo.

cd /d "%~dp0"

if exist "kolosal-server.exe" (
    kolosal-server.exe
) else if exist "kolosal-server" (
    kolosal-server
) else (
    echo Error: kolosal-server executable not found!
    exit /b 1
)
"@

Set-Content -Path "$StagingDir/start-server.bat" -Value $StartServerBat
Write-Success "Created start-server.bat"

# Create README for the package
$PackageReadme = @"
# Kolosal Server v$Version

This is a distribution package of Kolosal Server.

## Quick Start

### Windows
1. Double-click `start-server.bat` or right-click and "Run with PowerShell" on `start-server.ps1`
2. Or open PowerShell/Command Prompt in this directory and run: `.\kolosal-server.exe`

### macOS/Linux
1. Open Terminal
2. Navigate to this directory
3. Run: `chmod +x start-server.sh && ./start-server.sh`
4. Or run: `chmod +x kolosal-server && ./kolosal-server`

The server will start on http://localhost:8080
Press Ctrl+C to stop

### Manual Start with Config
- Windows: `.\kolosal-server.exe --config configs/config.yaml`
- macOS/Linux: `./kolosal-server --config configs/config.yaml`

## Directory Structure

- kolosal-server(.exe) - Main executable
- *.dylib / *.dll - Required shared libraries
- lib/ - Additional shared libraries
- configs/ - Sample configuration files
- docs/ - Documentation
- assets/ - Icons and images
- data/ - Runtime data and FAISS indexes
- logs/ - Server logs
- models/ - Place your GGUF model files here
- start-server.sh - Startup script for macOS/Linux
- start-server.ps1 - PowerShell startup script for Windows
- start-server.bat - Batch startup script for Windows

## Configuration

Copy one of the sample configs from the configs directory:
``````bash
cp configs/config.yaml config.yaml
``````

Then edit config.yaml to customize:
- Server port
- Model paths
- Authentication settings
- And more...

## Adding Models

1. Download GGUF model files
2. Place them in the models directory
3. Configure them in config.yaml or add via API

## Documentation

See the docs directory for comprehensive documentation:
- API usage guides
- Configuration reference
- Developer documentation

## Web Interface

After starting the server, access:
- Server API: http://localhost:8080
- Health check: http://localhost:8080/v1/health

## Troubleshooting

### macOS/Linux
If you get "Permission denied" errors:
``````bash
chmod +x kolosal-server
chmod +x start-server.sh
``````

If you get security warnings about unidentified developer (macOS):
1. System Preferences > Security & Privacy
2. Click "Allow Anyway" for kolosal-server

### Windows
If you get "Windows protected your PC" warning:
1. Click "More info"
2. Click "Run anyway"

If DLLs are missing, ensure all .dll files are in the same directory as kolosal-server.exe

## Support

- GitHub: https://github.com/KolosalAI/kolosal-server
- Documentation: See the docs folder
- Issues: https://github.com/KolosalAI/kolosal-server/issues

## Version Information

- Version: $Version
- Build Date: $(Get-Date -Format "yyyy-MM-dd")
- Platform: Cross-platform (Windows/macOS/Linux)
- Package Type: Archive

"@

Set-Content -Path "$StagingDir/README.txt" -Value $PackageReadme
Write-Success "Created README.txt"

# Create a sample config file in the root if none exists
$DefaultConfig = "$StagingDir/config.yaml"
$SampleConfig = "$StagingDir/configs/config.yaml"
if ((Test-Path $SampleConfig) -and (-not (Test-Path $DefaultConfig))) {
    Copy-Item $SampleConfig -Destination $DefaultConfig -Force
    Write-Success "Created default config.yaml"
}

# Create the archive
Write-Host ""
Write-Info "Creating archive..."
if (Test-Path $DmgPath) {
    Remove-Item -Path $DmgPath -Force
}

# Check if running on macOS with hdiutil available
$IsHdiutilAvailable = $false
if ($IsMacOS -or $IsLinux) {
    $HdiutilCheck = Get-Command hdiutil -ErrorAction SilentlyContinue
    if ($HdiutilCheck) {
        $IsHdiutilAvailable = $true
    }
}

if ($IsHdiutilAvailable) {
    Write-Info "Using hdiutil to create DMG..."
    try {
        # Execute hdiutil command
        $HdiutilArgs = @(
            "create",
            "-volname", $VolumeName,
            "-srcfolder", $StagingDir,
            "-ov",
            "-format", "UDZO",
            $DmgPath
        )
        
        $Process = Start-Process -FilePath "hdiutil" -ArgumentList $HdiutilArgs -NoNewWindow -Wait -PassThru
        
        if ($Process.ExitCode -eq 0) {
            Write-Success "DMG archive created: $DmgName"
            
            # Get file size
            $DmgSize = [math]::Round((Get-Item $DmgPath).Length / 1MB, 2)
            Write-Success "Package size: $DmgSize MB"
        } else {
            throw "hdiutil failed with exit code $($Process.ExitCode)"
        }
    } catch {
        Write-ErrorMsg "Failed to create DMG: $_"
        $IsHdiutilAvailable = $false
    }
}

if (-not $IsHdiutilAvailable) {
    # Fallback to tar.gz or zip
    Write-Warning "hdiutil not available, creating tar.gz instead..."
    $TarName = "kolosal-server-$Version-macOS.tar.gz"
    $TarPath = Join-Path $OutputDir $TarName
    
    # Check if tar is available
    $TarCommand = Get-Command tar -ErrorAction SilentlyContinue
    if ($TarCommand) {
        try {
            Push-Location $StagingDir
            tar -czf $TarPath *
            Pop-Location
            
            if (Test-Path $TarPath) {
                Write-Success "tar.gz archive created: $TarName"
                
                # Get file size
                $TarSize = [math]::Round((Get-Item $TarPath).Length / 1MB, 2)
                Write-Success "Package size: $TarSize MB"
                $DmgPath = $TarPath
                $DmgName = $TarName
            } else {
                throw "tar.gz file was not created"
            }
        } catch {
            Write-ErrorMsg "Failed to create tar.gz: $_"
            
            # Final fallback to zip
            Write-Warning "Creating zip archive instead..."
            $ZipName = "kolosal-server-$Version-macOS.zip"
            $ZipPath = Join-Path $OutputDir $ZipName
            
            Compress-Archive -Path "$StagingDir\*" -DestinationPath $ZipPath -Force
            
            if (Test-Path $ZipPath) {
                Write-Success "ZIP archive created: $ZipName"
                
                # Get file size
                $ZipSize = [math]::Round((Get-Item $ZipPath).Length / 1MB, 2)
                Write-Success "Package size: $ZipSize MB"
                $DmgPath = $ZipPath
                $DmgName = $ZipName
            } else {
                Write-ErrorMsg "Failed to create ZIP"
                exit 1
            }
        }
    } else {
        # Use PowerShell's built-in Compress-Archive
        Write-Warning "tar not available, creating zip archive instead..."
        $ZipName = "kolosal-server-$Version-macOS.zip"
        $ZipPath = Join-Path $OutputDir $ZipName
        
        Compress-Archive -Path "$StagingDir\*" -DestinationPath $ZipPath -Force
        
        if (Test-Path $ZipPath) {
            Write-Success "ZIP archive created: $ZipName"
            
            # Get file size
            $ZipSize = [math]::Round((Get-Item $ZipPath).Length / 1MB, 2)
            Write-Success "Package size: $ZipSize MB"
            $DmgPath = $ZipPath
            $DmgName = $ZipName
        } else {
            Write-ErrorMsg "Failed to create ZIP"
            exit 1
        }
    }
}

# Clean up staging directory
Write-Info "Cleaning up staging directory..."
Remove-Item -Path $StagingDir -Recurse -Force
Write-Success "Cleanup complete"

# Summary
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "   Package Created Successfully!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host ""
Write-Host "Output: " -NoNewline
Write-Host $DmgPath -ForegroundColor Yellow
Write-Host ""
Write-Host "To distribute:" -ForegroundColor Cyan
Write-Host "  1. Upload $DmgName to your release page" -ForegroundColor White
Write-Host "  2. Users can extract and run the appropriate startup script:" -ForegroundColor White
Write-Host "     - Windows: start-server.bat or start-server.ps1" -ForegroundColor White
Write-Host "     - macOS/Linux: start-server.sh" -ForegroundColor White
Write-Host "  3. Or run kolosal-server executable directly" -ForegroundColor White
Write-Host ""
