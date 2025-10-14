# Build NSIS Installer for Kolosal Server
# This script automates the creation of the NSIS installer
# Usage: .\build-nsis-installer.ps1 [-NsisPath <path>] [-Version <version>] [-SkipBuildCheck] [-AutoBuild] [-EnableVulkan]

param(
    [Parameter(Mandatory=$false)]
    [string]$NsisPath = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Version = "1.0.0",
    
    [Parameter(Mandatory=$false)]
    [switch]$SkipBuildCheck = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$AutoBuild = $true,
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableVulkan = $false
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
Write-Host "   Kolosal Server NSIS Installer Builder v$Version" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

if ($EnableVulkan) {
    Write-Info "Vulkan support: ENABLED"
    Write-Warning "Note: Vulkan SDK must be installed for this build to succeed!"
    Write-Info "The installer will include both llama-cpu.dll and llama-vulkan.dll"
} else {
    Write-Info "Vulkan support: DISABLED (use -EnableVulkan to enable)"
    Write-Info "The installer will include llama-cpu.dll only"
}
Write-Host ""

# Find NSIS installation
Write-Info "Searching for NSIS installation..."

if ($NsisPath -eq "") {
    # Common NSIS installation paths
    $nsisSearchPaths = @(
        "${env:ProgramFiles}\NSIS\makensis.exe",
        "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
        "C:\Program Files\NSIS\makensis.exe",
        "C:\Program Files (x86)\NSIS\makensis.exe"
    )
    
    foreach ($path in $nsisSearchPaths) {
        if (Test-Path $path) {
            $NsisPath = $path
            Write-Success "Found NSIS at: $NsisPath"
            break
        }
    }
    
    # Try to find it in PATH
    if ($NsisPath -eq "") {
        try {
            $makensis = Get-Command makensis -ErrorAction SilentlyContinue
            if ($makensis) {
                $NsisPath = $makensis.Source
                Write-Success "Found NSIS in PATH: $NsisPath"
            }
        } catch {
            # NSIS not in PATH
        }
    }
}

if ($NsisPath -eq "" -or -not (Test-Path $NsisPath)) {
    Write-Error "NSIS not found!"
    Write-Host ""
    Write-Host "Please install NSIS from: https://nsis.sourceforge.io/Download" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Or specify the path manually:" -ForegroundColor Yellow
    Write-Host "  .\build-nsis-installer.ps1 -NsisPath 'C:\Path\To\NSIS\makensis.exe'" -ForegroundColor White
    Write-Host ""
    exit 1
}

# Check if build exists and build if necessary
if (-not $SkipBuildCheck) {
    Write-Info "Checking for built binaries..."
    
    $buildLocations = @(
        "..\build\Release\kolosal-server.exe",
        "..\build\dist\kolosal-server.exe"
    )
    
    $buildFound = $false
    $executablePath = ""
    foreach ($location in $buildLocations) {
        if (Test-Path $location) {
            Write-Success "Found built executable: $location"
            $buildFound = $true
            $executablePath = $location
            break
        }
    }
    
    if (-not $buildFound) {
        Write-Warning "No built executable found!"
        
        if ($AutoBuild) {
            Write-Info "Attempting to build the project automatically..."
            Write-Host ""
            
            # Check if build directory exists
            if (-not (Test-Path "..\build")) {
                Write-Info "Creating build directory..."
                try {
                    New-Item -ItemType Directory -Path "..\build" -Force | Out-Null
                    Write-Success "Build directory created"
                } catch {
                    Write-Error "Failed to create build directory: $_"
                    exit 1
                }
            }
            
            # Navigate to build directory and configure cmake if needed
            Push-Location "..\build"
            try {
                # Check if CMakeCache.txt exists
                if (-not (Test-Path "CMakeCache.txt")) {
                    Write-Info "Configuring CMake..."
                    
                    # Build CMake arguments
                    $cmakeArgs = @("..")
                    if ($EnableVulkan) {
                        Write-Info "Enabling Vulkan support..."
                        $cmakeArgs += "-DUSE_VULKAN=ON"
                    }
                    
                    & cmake @cmakeArgs 2>&1 | Out-Host
                    if ($LASTEXITCODE -ne 0) {
                        Write-Error "CMake configuration failed"
                        Pop-Location
                        exit 1
                    }
                    Write-Success "CMake configuration completed"
                } elseif ($EnableVulkan) {
                    # Reconfigure if Vulkan is requested but cache exists
                    Write-Info "Reconfiguring CMake with Vulkan support..."
                    & cmake .. -DUSE_VULKAN=ON 2>&1 | Out-Host
                    if ($LASTEXITCODE -ne 0) {
                        Write-Error "CMake reconfiguration failed"
                        Pop-Location
                        exit 1
                    }
                    Write-Success "CMake reconfiguration completed"
                }
                
                # Build the project
                Write-Info "Building the project (Release configuration)..."
                if ($EnableVulkan) {
                    Write-Info "Building with Vulkan support - Vulkan SDK must be installed!"
                }
                Write-Info "This may take several minutes..."
                Write-Host ""
                
                & cmake --build . --config Release 2>&1 | Out-Host
                if ($LASTEXITCODE -ne 0) {
                    Write-Error "Build failed!"
                    Pop-Location
                    exit 1
                }
                
                Write-Host ""
                Write-Success "Build completed successfully!"
                
            } catch {
                Write-Error "Build process failed: $_"
                Pop-Location
                exit 1
            } finally {
                Pop-Location
            }
            
            # Verify the build was successful
            $buildFound = $false
            foreach ($location in $buildLocations) {
                if (Test-Path $location) {
                    Write-Success "Built executable found: $location"
                    $buildFound = $true
                    $executablePath = $location
                    break
                }
            }
            
            if (-not $buildFound) {
                Write-Error "Build completed but executable not found!"
                Write-Host ""
                Write-Host "Expected executable locations:" -ForegroundColor Yellow
                foreach ($location in $buildLocations) {
                    Write-Host "  $location" -ForegroundColor White
                }
                Write-Host ""
                exit 1
            }
            
            # Verify inference engine DLLs exist
            Write-Host ""
            Write-Info "Checking for inference engine DLLs..."
            $dllLocations = @(
                "..\build\Release\llama-cpu.dll",
                "..\build\bin\llama-cpu.dll",
                "..\inference\build\Release\llama-cpu.dll"
            )
            
            $cpuDllFound = $false
            foreach ($location in $dllLocations) {
                if (Test-Path $location) {
                    Write-Success "Found llama-cpu.dll at: $location"
                    $cpuDllFound = $true
                    break
                }
            }
            
            if (-not $cpuDllFound) {
                Write-Error "llama-cpu.dll not found!"
                Write-Warning "The installer will be missing the CPU inference engine."
                Write-Host ""
                Write-Host "To fix this, make sure the inference engine is built:" -ForegroundColor Yellow
                Write-Host "  cd ..\build" -ForegroundColor White
                Write-Host "  cmake --build . --config Release --target llama-cpu" -ForegroundColor White
                Write-Host ""
                $response = Read-Host "Continue anyway? (y/n)"
                if ($response -ne "y" -and $response -ne "Y") {
                    exit 1
                }
            }
            
            # Check for Vulkan DLL if enabled
            if ($EnableVulkan) {
                $vulkanDllLocations = @(
                    "..\build\Release\llama-vulkan.dll",
                    "..\build\bin\llama-vulkan.dll",
                    "..\inference\build\Release\llama-vulkan.dll"
                )
                
                $vulkanDllFound = $false
                foreach ($location in $vulkanDllLocations) {
                    if (Test-Path $location) {
                        Write-Success "Found llama-vulkan.dll at: $location"
                        $vulkanDllFound = $true
                        break
                    }
                }
                
                if (-not $vulkanDllFound) {
                    Write-Warning "llama-vulkan.dll not found despite -EnableVulkan flag!"
                    Write-Host ""
                    Write-Host "Vulkan support was requested but the DLL is missing." -ForegroundColor Yellow
                    Write-Host "Make sure to build with Vulkan support:" -ForegroundColor Yellow
                    Write-Host "  cd ..\build" -ForegroundColor White
                    Write-Host "  cmake .. -DUSE_VULKAN=ON" -ForegroundColor White
                    Write-Host "  cmake --build . --config Release" -ForegroundColor White
                    Write-Host ""
                }
            }
            Write-Host ""
        } else {
            Write-Host ""
            Write-Host "Please build the project first:" -ForegroundColor Yellow
            Write-Host "  cd ..\build" -ForegroundColor White
            Write-Host "  cmake --build . --config Release" -ForegroundColor White
            Write-Host ""
            Write-Host "Or use -AutoBuild to build automatically" -ForegroundColor Yellow
            Write-Host "Or use -SkipBuildCheck to bypass this check" -ForegroundColor Yellow
            Write-Host ""
            
            $response = Read-Host "Continue anyway? (y/n)"
            if ($response -ne "y" -and $response -ne "Y") {
                exit 1
            }
        }
    }
}

# Check for required assets
Write-Info "Checking for required assets..."
$assetsOk = $true

$requiredAssets = @(
    @{Path = "..\assets\icon.ico"; Name = "Icon file"},
    @{Path = "..\LICENSE"; Name = "License file"}
)

foreach ($asset in $requiredAssets) {
    if (Test-Path $asset.Path) {
        Write-Success "$($asset.Name) found"
    } else {
        Write-Warning "$($asset.Name) not found: $($asset.Path)"
        $assetsOk = $false
    }
}

if (-not $assetsOk) {
    Write-Warning "Some assets are missing, but we'll continue..."
    Write-Info "The installer may not include all branding elements"
}

# Update version in script.nsi if needed
Write-Info "Checking version in script.nsi..."
$nsiContent = Get-Content "script.nsi" -Raw
if ($nsiContent -match '!define PRODUCT_VERSION "([^"]+)"') {
    $currentVersion = $matches[1]
    if ($currentVersion -ne $Version) {
        Write-Warning "Version mismatch: script.nsi has $currentVersion, but you specified $Version"
        $response = Read-Host "Update script.nsi to version $Version? (y/n)"
        if ($response -eq "y" -or $response -eq "Y") {
            $nsiContent = $nsiContent -replace '!define PRODUCT_VERSION "[^"]+"', "!define PRODUCT_VERSION `"$Version`""
            Set-Content -Path "script.nsi" -Value $nsiContent -NoNewline
            Write-Success "Updated script.nsi to version $Version"
        }
    } else {
        Write-Success "Version matches: $Version"
    }
}

# Build the installer
Write-Host ""
Write-Info "Building NSIS installer..."
Write-Info "This may take a few minutes..."
Write-Host ""

$startTime = Get-Date

try {
    # Run makensis with verbose output
    $process = Start-Process -FilePath $NsisPath -ArgumentList "script.nsi" -NoNewWindow -Wait -PassThru
    
    if ($process.ExitCode -eq 0) {
        $endTime = Get-Date
        $duration = ($endTime - $startTime).TotalSeconds
        
        Write-Host ""
        Write-Success "NSIS installer built successfully!"
        Write-Info "Build time: $([math]::Round($duration, 2)) seconds"
        
        # Find the generated installer
        $installerName = "kolosal-server-installer-$Version.exe"
        if (Test-Path $installerName) {
            $installerSize = (Get-Item $installerName).Length / 1MB
            Write-Success "Installer created: $installerName"
            Write-Success "Size: $([math]::Round($installerSize, 2)) MB"
            
            # Show full path
            $fullPath = (Resolve-Path $installerName).Path
            Write-Host ""
            Write-Host "Full path: " -NoNewline
            Write-Host $fullPath -ForegroundColor Yellow
        } else {
            Write-Warning "Installer file not found: $installerName"
            Write-Info "Check the current directory for the .exe file"
        }
        
    } else {
        Write-Error "NSIS compilation failed with exit code: $($process.ExitCode)"
        Write-Host ""
        Write-Host "Common issues:" -ForegroundColor Yellow
        Write-Host "  - Missing files referenced in script.nsi" -ForegroundColor White
        Write-Host "  - Incorrect file paths" -ForegroundColor White
        Write-Host "  - Syntax errors in script.nsi" -ForegroundColor White
        Write-Host ""
        Write-Host "Review the output above for specific errors" -ForegroundColor Yellow
        exit 1
    }
} catch {
    Write-Error "Failed to run NSIS: $_"
    exit 1
}

# Summary
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "   NSIS Installer Created Successfully!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. Test the installer on a clean Windows machine" -ForegroundColor White
Write-Host "  2. Verify all features work correctly" -ForegroundColor White
Write-Host "  3. (Optional) Sign the installer with your certificate" -ForegroundColor White
Write-Host "  4. Upload to your distribution platform" -ForegroundColor White
Write-Host ""
Write-Host "To sign the installer:" -ForegroundColor Cyan
Write-Host "  signtool sign /f certificate.pfx /p password /t http://timestamp.digicert.com $installerName" -ForegroundColor White
Write-Host ""
