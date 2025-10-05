#!/bin/bash
# Master Build Script for Kolosal Server macOS Installer
# This script builds DMG installer for macOS
# Usage: ./build-mac-installer.sh [-v <version>] [-b <build_dir>] [--skip-build] [--build-project]

# Default values
VERSION="1.0.0"
BUILD_DIR="../build/Release"
SKIP_BUILD=false
BUILD_PROJECT=false
USE_DIST=false

# Color output functions
print_success() {
    echo -e "\033[0;32m✓ $1\033[0m"
}

print_info() {
    echo -e "\033[0;36mℹ $1\033[0m"
}

print_warning() {
    echo -e "\033[0;33m⚠ $1\033[0m"
}

print_error() {
    echo -e "\033[0;31m✗ $1\033[0m"
}

print_header() {
    echo ""
    echo -e "\033[0;36m═══════════════════════════════════════════════════════\033[0m"
    echo -e "\033[0;36m   $1\033[0m"
    echo -e "\033[0;36m═══════════════════════════════════════════════════════\033[0m"
    echo ""
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        -b|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --build-project)
            BUILD_PROJECT=true
            shift
            ;;
        --use-dist)
            USE_DIST=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [-v <version>] [-b <build_dir>] [--skip-build] [--build-project] [--use-dist]"
            echo ""
            echo "Options:"
            echo "  -v, --version <version>     Set version number (default: 1.0.0)"
            echo "  -b, --build-dir <path>      Set build directory (default: ../build/Release)"
            echo "  --skip-build                Skip DMG creation"
            echo "  --build-project             Build the project before creating installer"
            echo "  --use-dist                  Use dist directory instead of Release"
            echo "  -h, --help                  Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Main script
print_header "Kolosal Server - macOS Installer Builder v$VERSION"

START_TIME=$(date +%s)
PROJECT_BUILD_STATUS="Not Started"
DMG_BUILD_STATUS="Not Started"

# Step 1: Build the project (optional)
if [ "$BUILD_PROJECT" = true ]; then
    print_header "Step 1: Building Kolosal Server Project"
    
    print_info "Checking for CMake build directory..."
    if [ ! -d "../build" ]; then
        print_warning "Build directory not found. Creating..."
        mkdir -p "../build"
    fi
    
    print_info "Configuring CMake..."
    cd ../build
    
    if cmake -DCMAKE_BUILD_TYPE=Release .. ; then
        print_success "CMake configuration completed"
        
        print_info "Building project (Release configuration)..."
        if cmake --build . --config Release ; then
            print_success "Project built successfully"
            PROJECT_BUILD_STATUS="Success"
        else
            print_error "Project build failed!"
            cd - > /dev/null
            exit 1
        fi
    else
        print_error "CMake configuration failed!"
        cd - > /dev/null
        exit 1
    fi
    
    cd - > /dev/null
else
    print_info "Skipping project build (use --build-project to build)"
    print_info "Assuming project is already built..."
    
    # Verify build exists
    BUILD_LOCATIONS=(
        "../build/Release/kolosal-server"
        "../build/dist/kolosal-server"
    )
    
    BUILD_FOUND=false
    for location in "${BUILD_LOCATIONS[@]}"; do
        if [ -f "$location" ]; then
            print_success "Found built executable: $location"
            BUILD_FOUND=true
            PROJECT_BUILD_STATUS="Already Built"
            break
        fi
    done
    
    if [ "$BUILD_FOUND" = false ]; then
        print_error "No built executable found!"
        echo ""
        echo -e "\033[0;33mPlease either:\033[0m"
        echo -e "\033[0;37m  1. Build the project first, OR\033[0m"
        echo -e "\033[0;37m  2. Use --build-project flag to build automatically\033[0m"
        echo ""
        exit 1
    fi
fi

# Step 2: Build DMG Installer
if [ "$SKIP_BUILD" = false ]; then
    print_header "Step 2: Building DMG Installer"
    
    DMG_ARGS="-v $VERSION"
    
    if [ "$USE_DIST" = true ]; then
        DMG_ARGS="$DMG_ARGS -d"
    else
        DMG_ARGS="$DMG_ARGS -b $BUILD_DIR"
    fi
    
    if bash ./create-dmg-installer.sh $DMG_ARGS ; then
        print_success "DMG installer created successfully"
        DMG_BUILD_STATUS="Success"
    else
        print_error "DMG installer creation failed"
        DMG_BUILD_STATUS="Failed"
    fi
else
    print_info "Skipping DMG installer (use without --skip-build to build)"
    DMG_BUILD_STATUS="Skipped"
fi

# Final Summary
END_TIME=$(date +%s)
TOTAL_DURATION=$((END_TIME - START_TIME))

print_header "Build Summary"

echo -e "\033[0;36mResults:\033[0m"
echo -n "  Project Build:   "
if [[ "$PROJECT_BUILD_STATUS" == "Success" || "$PROJECT_BUILD_STATUS" == "Already Built" ]]; then
    echo -e "\033[0;32m$PROJECT_BUILD_STATUS\033[0m"
else
    echo -e "\033[0;33m$PROJECT_BUILD_STATUS\033[0m"
fi

echo -n "  DMG Installer:   "
if [ "$DMG_BUILD_STATUS" = "Success" ]; then
    echo -e "\033[0;32m$DMG_BUILD_STATUS\033[0m"
elif [ "$DMG_BUILD_STATUS" = "Skipped" ]; then
    echo -e "\033[0;33m$DMG_BUILD_STATUS\033[0m"
else
    echo -e "\033[0;31m$DMG_BUILD_STATUS\033[0m"
fi

echo ""
echo -e "\033[0;36mTotal time: $TOTAL_DURATION seconds\033[0m"
echo ""

# List generated files
echo -e "\033[0;36mGenerated files:\033[0m"
DMG_FILE="../build/kolosal-server-$VERSION-macOS.dmg"
TAR_FILE="../build/kolosal-server-$VERSION-macOS.tar.gz"

if [ -f "$DMG_FILE" ]; then
    DMG_SIZE=$(du -m "$DMG_FILE" | awk '{print $1}')
    echo -e "  📦 \033[0;33m$(basename $DMG_FILE)\033[0m \033[0;90m(${DMG_SIZE} MB)\033[0m"
fi

if [ -f "$TAR_FILE" ]; then
    TAR_SIZE=$(du -m "$TAR_FILE" | awk '{print $1}')
    echo -e "  📦 \033[0;33m$(basename $TAR_FILE)\033[0m \033[0;90m(${TAR_SIZE} MB)\033[0m"
fi

echo ""

# Success check
if [[ "$DMG_BUILD_STATUS" == "Success" || "$DMG_BUILD_STATUS" == "Skipped" ]]; then
    echo -e "\033[0;32m✓ Installer built successfully!\033[0m"
    echo ""
    echo -e "\033[0;36mNext steps:\033[0m"
    echo -e "\033[0;37m  1. Test the installer on a clean macOS machine\033[0m"
    echo -e "\033[0;37m  2. Verify all features work correctly\033[0m"
    echo -e "\033[0;37m  3. (Optional) Sign the application with Apple Developer ID\033[0m"
    echo -e "\033[0;37m  4. (Optional) Notarize the application for macOS Gatekeeper\033[0m"
    echo -e "\033[0;37m  5. Upload to GitHub Releases or your distribution platform\033[0m"
    echo ""
else
    print_warning "Installer build failed. Check the output above for details."
    echo ""
    exit 1
fi
