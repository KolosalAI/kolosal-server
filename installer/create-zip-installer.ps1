# Kolosal Server ZIP Installer Creator
# This script creates a portable ZIP package of Kolosal Server for Windows
# Usage: .\create-zip-installer.ps1 [-BuildDir <path>] [-OutputDir <path>] [-Version <version>]

param(
    [Parameter(Mandatory=$false)]
    [string]$BuildDir = "..\build\Release",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "..\build",
    
    [Parameter(Mandatory=$false)]
    [string]$Version = "1.0.0",
    
    [Parameter(Mandatory=$false)]
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

function Write-Error {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor Red
}

# Main script
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   Kolosal Server ZIP Installer Creator v$Version" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Determine source directory
if ($UseDist) {
    $SourceDir = "..\build\dist"
    Write-Info "Using dist directory: $SourceDir"
} else {
    $SourceDir = $BuildDir
    Write-Info "Using build directory: $SourceDir"
}

# Convert to absolute paths
$SourceDir = Resolve-Path $SourceDir -ErrorAction SilentlyContinue
if (-not $SourceDir) {
    Write-Error "Source directory not found: $BuildDir"
    Write-Info "Please build the project first or specify -UseDist if you've run 'cmake --build build --target dist'"
    exit 1
}

$OutputDir = if (Test-Path $OutputDir) { 
    Resolve-Path $OutputDir 
} else { 
    New-Item -ItemType Directory -Path $OutputDir -Force | Select-Object -ExpandProperty FullName
}

# Create temporary staging directory
$StagingDir = Join-Path $OutputDir "kolosal-server-staging-$Version"
$ZipName = "kolosal-server-portable-$Version-win64.zip"
$ZipPath = Join-Path $OutputDir $ZipName

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
New-Item -ItemType Directory -Path "$StagingDir\lib" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir\configs" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir\docs" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir\assets" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir\data\faiss_index" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir\logs" -Force | Out-Null
New-Item -ItemType Directory -Path "$StagingDir\models" -Force | Out-Null
Write-Success "Directory structure created"

# Copy executable and DLLs
Write-Info "Copying executable and libraries..."
$exeFound = $false
$dllCount = 0

# Try multiple locations for the executable
$exeLocations = @(
    (Join-Path $SourceDir "kolosal-server.exe"),
    (Join-Path $SourceDir "bin\kolosal-server.exe"),
    "..\build\dist\kolosal-server.exe"
)

foreach ($exePath in $exeLocations) {
    if (Test-Path $exePath) {
        Copy-Item -Path $exePath -Destination $StagingDir -Force
        Write-Success "Copied kolosal-server.exe from $(Split-Path $exePath -Parent)"
        $exeFound = $true
        break
    }
}

if (-not $exeFound) {
    Write-Error "kolosal-server.exe not found in any expected location"
    Write-Warning "Tried: $($exeLocations -join ', ')"
    Write-Info "Please build the project first"
    exit 1
}

# Copy DLL files from source directory
$dllFiles = Get-ChildItem -Path $SourceDir -Filter "*.dll" -ErrorAction SilentlyContinue
foreach ($dll in $dllFiles) {
    Copy-Item -Path $dll.FullName -Destination $StagingDir -Force
    $dllCount++
}

# Copy lib directory if it exists
if (Test-Path (Join-Path $SourceDir "lib")) {
    Copy-Item -Path (Join-Path $SourceDir "lib\*") -Destination "$StagingDir\lib" -Recurse -Force -ErrorAction SilentlyContinue
    $libFiles = Get-ChildItem -Path "$StagingDir\lib" -Recurse -File
    if ($libFiles.Count -gt 0) {
        Write-Success "Copied $($libFiles.Count) files from lib directory"
    }
}

Write-Success "Copied $dllCount DLL files"

# Copy configuration files
Write-Info "Copying configuration files..."
$configCount = 0
$configFiles = @(
    "..\configs\config.yaml",
    "..\configs\config.json", 
    "..\configs\config_rms.yaml",
    "..\configs\local-retrieval-config.yaml"
)

foreach ($config in $configFiles) {
    if (Test-Path $config) {
        Copy-Item -Path $config -Destination "$StagingDir\configs" -Force
        $configCount++
    }
}
Write-Success "Copied $configCount configuration files"

# Copy documentation
Write-Info "Copying documentation..."
$docCount = 0
$docFiles = @(
    "..\README.md",
    "..\LICENSE",
    "..\changes.log"
)

foreach ($doc in $docFiles) {
    if (Test-Path $doc) {
        Copy-Item -Path $doc -Destination $StagingDir -Force
        $docCount++
    }
}

# Copy docs directory
if (Test-Path "..\docs") {
    Copy-Item -Path "..\docs\*" -Destination "$StagingDir\docs" -Recurse -Force -ErrorAction SilentlyContinue
    $docsFiles = Get-ChildItem -Path "$StagingDir\docs" -Recurse -File
    if ($docsFiles.Count -gt 0) {
        $docCount += $docsFiles.Count
    }
}
Write-Success "Copied $docCount documentation files"

# Copy assets
Write-Info "Copying assets..."
$assetCount = 0
if (Test-Path "..\assets") {
    $assetFiles = Get-ChildItem -Path "..\assets" -Include "*.ico","*.png" -File -ErrorAction SilentlyContinue
    foreach ($asset in $assetFiles) {
        Copy-Item -Path $asset.FullName -Destination "$StagingDir\assets" -Force
        $assetCount++
    }
}
Write-Success "Copied $assetCount asset files"

# Copy static files if they exist
if (Test-Path "..\static") {
    Write-Info "Copying static web files..."
    New-Item -ItemType Directory -Path "$StagingDir\static" -Force | Out-Null
    Copy-Item -Path "..\static\*" -Destination "$StagingDir\static" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Success "Static files copied"
}

# Create batch file for easy startup
Write-Info "Creating startup scripts..."
$startBatch = @"
@echo off
echo ========================================
echo   Kolosal Server v$Version
echo ========================================
echo.
echo Starting Kolosal Server...
echo Server will be available at http://localhost:8080
echo.
echo Press Ctrl+C to stop the server
echo.
start /B kolosal-server.exe
echo.
echo Server started! Check server.log for details.
echo.
pause
"@
Set-Content -Path "$StagingDir\start-server.bat" -Value $startBatch -Encoding ASCII
Write-Success "Created start-server.bat"

# Create PowerShell startup script
$startPs1 = @"
# Kolosal Server Startup Script
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   Kolosal Server v$Version" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Starting Kolosal Server..." -ForegroundColor Green
Write-Host "Server will be available at http://localhost:8080" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press Ctrl+C to stop the server" -ForegroundColor Yellow
Write-Host ""

Start-Process -FilePath ".\kolosal-server.exe" -NoNewWindow -Wait
"@
Set-Content -Path "$StagingDir\start-server.ps1" -Value $startPs1 -Encoding UTF8
Write-Success "Created start-server.ps1"

# Create README for the portable version
$portableReadme = @"
# Kolosal Server Portable Edition v$Version

This is a portable version of Kolosal Server that can run without installation.

## Quick Start

### Windows Batch File (Recommended for beginners)
1. Double-click \`start-server.bat\` to start the server
2. The server will start on http://localhost:8080
3. Press Ctrl+C in the window to stop

### PowerShell Script (Recommended for advanced users)
1. Right-click \`start-server.ps1\` and select "Run with PowerShell"
2. Or run: \`powershell -ExecutionPolicy Bypass -File start-server.ps1\`

### Manual Start
1. Open Command Prompt or PowerShell in this directory
2. Run: \`.\kolosal-server.exe\`
3. Or specify a config: \`.\kolosal-server.exe --config configs\config.yaml\`

## Directory Structure

- \`kolosal-server.exe\` - Main executable
- \`*.dll\` - Required libraries
- \`lib/\` - Additional shared libraries
- \`configs/\` - Sample configuration files
- \`docs/\` - Documentation
- \`assets/\` - Icons and images
- \`data/\` - Runtime data and FAISS indexes
- \`logs/\` - Server logs
- \`models/\` - Place your GGUF model files here

## Configuration

Copy one of the sample configs from the \`configs\` directory:
\`\`\`
copy configs\config.yaml config.yaml
\`\`\`

Then edit \`config.yaml\` to customize:
- Server port
- Model paths
- Authentication settings
- And more...

## Adding Models

1. Download GGUF model files
2. Place them in the \`models\` directory
3. Configure them in \`config.yaml\` or add via API

## Documentation

See the \`docs\` directory for comprehensive documentation:
- API usage guides
- Configuration reference
- Developer documentation

## Web Interface

After starting the server, access:
- Server API: http://localhost:8080
- Health check: http://localhost:8080/v1/health

## Support

- GitHub: https://github.com/KolosalAI/kolosal-server
- Documentation: See the \`docs\` folder
- Issues: https://github.com/KolosalAI/kolosal-server/issues

## Version Information

- Version: $Version
- Build Date: $(Get-Date -Format "yyyy-MM-dd")
- Package Type: Portable ZIP

"@
Set-Content -Path "$StagingDir\PORTABLE-README.txt" -Value $portableReadme -Encoding UTF8
Write-Success "Created PORTABLE-README.txt"

# Create a sample config file in the root if none exists
$defaultConfigPath = "$StagingDir\config.yaml"
$sampleConfigPath = "$StagingDir\configs\config.yaml"
if ((Test-Path $sampleConfigPath) -and (-not (Test-Path $defaultConfigPath))) {
    Copy-Item -Path $sampleConfigPath -Destination $defaultConfigPath -Force
    Write-Success "Created default config.yaml"
}

# Create the ZIP file
Write-Host ""
Write-Info "Creating ZIP archive..."
if (Test-Path $ZipPath) {
    Remove-Item -Path $ZipPath -Force
}

try {
    # Use .NET compression for better compatibility
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory($StagingDir, $ZipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
    Write-Success "ZIP archive created: $ZipName"
} catch {
    Write-Error "Failed to create ZIP: $_"
    exit 1
}

# Get file size
$zipSize = (Get-Item $ZipPath).Length / 1MB
Write-Success "Package size: $([math]::Round($zipSize, 2)) MB"

# Clean up staging directory
Write-Info "Cleaning up staging directory..."
Remove-Item -Path $StagingDir -Recurse -Force
Write-Success "Cleanup complete"

# Summary
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "   ZIP Installer Created Successfully!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host ""
Write-Host "Output: " -NoNewline
Write-Host $ZipPath -ForegroundColor Yellow
Write-Host "Size: " -NoNewline
Write-Host "$([math]::Round($zipSize, 2)) MB" -ForegroundColor Yellow
Write-Host ""
Write-Host "To distribute:" -ForegroundColor Cyan
Write-Host "  1. Upload $ZipName to your release page" -ForegroundColor White
Write-Host "  2. Users can extract and run start-server.bat" -ForegroundColor White
Write-Host "  3. No installation required!" -ForegroundColor White
Write-Host ""
