#!/bin/bash
# Kolosal Server DMG Installer Creator for macOS
# This script creates a DMG package of Kolosal Server for macOS
# Usage: ./create-dmg-installer.sh [-b <build_dir>] [-o <output_dir>] [-v <version>]

# Default values
BUILD_DIR="../build/Release"
OUTPUT_DIR="../build"
VERSION="1.0.0"
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
while getopts "b:o:v:d" opt; do
    case $opt in
        b) BUILD_DIR="$OPTARG" ;;
        o) OUTPUT_DIR="$OPTARG" ;;
        v) VERSION="$OPTARG" ;;
        d) USE_DIST=true ;;
        \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
    esac
done

# Main script
print_header "Kolosal Server DMG Installer Creator v$VERSION"

# Determine source directory
if [ "$USE_DIST" = true ]; then
    SOURCE_DIR="../build/dist"
    print_info "Using dist directory: $SOURCE_DIR"
else
    SOURCE_DIR="$BUILD_DIR"
    print_info "Using build directory: $SOURCE_DIR"
fi

# Check if source directory exists
if [ ! -d "$SOURCE_DIR" ]; then
    print_error "Source directory not found: $SOURCE_DIR"
    print_info "Please build the project first or use -d flag if you've run 'cmake --build build --target dist'"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Define paths
APP_NAME="Kolosal Server"
STAGING_DIR="$OUTPUT_DIR/kolosal-server-staging-$VERSION"
DMG_NAME="kolosal-server-$VERSION-macOS.dmg"
DMG_PATH="$OUTPUT_DIR/$DMG_NAME"
VOLUME_NAME="Kolosal Server $VERSION"

print_info "Source: $SOURCE_DIR"
print_info "Output: $OUTPUT_DIR"
print_info "Staging: $STAGING_DIR"
echo ""

# Clean up existing staging directory
if [ -d "$STAGING_DIR" ]; then
    print_info "Cleaning up existing staging directory..."
    rm -rf "$STAGING_DIR"
fi

# Create staging directory structure
print_info "Creating directory structure..."
mkdir -p "$STAGING_DIR"
mkdir -p "$STAGING_DIR/lib"
mkdir -p "$STAGING_DIR/configs"
mkdir -p "$STAGING_DIR/docs"
mkdir -p "$STAGING_DIR/assets"
mkdir -p "$STAGING_DIR/data/faiss_index"
mkdir -p "$STAGING_DIR/logs"
mkdir -p "$STAGING_DIR/models"
print_success "Directory structure created"

# Copy executable and libraries
print_info "Copying executable and libraries..."
EXE_FOUND=false
DYLIB_COUNT=0

# Try multiple locations for the executable
EXE_LOCATIONS=(
    "$SOURCE_DIR/kolosal-server"
    "$SOURCE_DIR/bin/kolosal-server"
    "../build/dist/kolosal-server"
)

for exe_path in "${EXE_LOCATIONS[@]}"; do
    if [ -f "$exe_path" ]; then
        cp "$exe_path" "$STAGING_DIR/"
        chmod +x "$STAGING_DIR/kolosal-server"
        print_success "Copied kolosal-server from $(dirname $exe_path)"
        EXE_FOUND=true
        break
    fi
done

if [ "$EXE_FOUND" = false ]; then
    print_error "kolosal-server executable not found in any expected location"
    print_warning "Tried: ${EXE_LOCATIONS[*]}"
    print_info "Please build the project first"
    exit 1
fi

# Copy dylib files from source directory
if compgen -G "$SOURCE_DIR/*.dylib" > /dev/null; then
    for dylib in "$SOURCE_DIR"/*.dylib; do
        cp "$dylib" "$STAGING_DIR/"
        DYLIB_COUNT=$((DYLIB_COUNT + 1))
    done
fi

# Copy lib directory if it exists
if [ -d "$SOURCE_DIR/lib" ]; then
    cp -R "$SOURCE_DIR/lib/"* "$STAGING_DIR/lib/" 2>/dev/null || true
    LIB_COUNT=$(find "$STAGING_DIR/lib" -type f | wc -l)
    if [ $LIB_COUNT -gt 0 ]; then
        print_success "Copied $LIB_COUNT files from lib directory"
    fi
fi

print_success "Copied $DYLIB_COUNT dylib files"

# Copy configuration files
print_info "Copying configuration files..."
CONFIG_COUNT=0
CONFIG_FILES=(
    "../configs/config.yaml"
    "../configs/config.json"
    "../configs/config_rms.yaml"
    "../configs/local-retrieval-config.yaml"
)

for config in "${CONFIG_FILES[@]}"; do
    if [ -f "$config" ]; then
        cp "$config" "$STAGING_DIR/configs/"
        CONFIG_COUNT=$((CONFIG_COUNT + 1))
    fi
done
print_success "Copied $CONFIG_COUNT configuration files"

# Copy documentation
print_info "Copying documentation..."
DOC_COUNT=0
DOC_FILES=(
    "../README.md"
    "../LICENSE"
    "../changes.log"
)

for doc in "${DOC_FILES[@]}"; do
    if [ -f "$doc" ]; then
        cp "$doc" "$STAGING_DIR/"
        DOC_COUNT=$((DOC_COUNT + 1))
    fi
done

# Copy docs directory
if [ -d "../docs" ]; then
    cp -R ../docs/* "$STAGING_DIR/docs/" 2>/dev/null || true
    DOCS_COUNT=$(find "$STAGING_DIR/docs" -type f | wc -l)
    if [ $DOCS_COUNT -gt 0 ]; then
        DOC_COUNT=$((DOC_COUNT + DOCS_COUNT))
    fi
fi
print_success "Copied $DOC_COUNT documentation files"

# Copy assets
print_info "Copying assets..."
ASSET_COUNT=0
if [ -d "../assets" ]; then
    for asset in ../assets/*.{ico,png,icns}; do
        if [ -f "$asset" ]; then
            cp "$asset" "$STAGING_DIR/assets/" 2>/dev/null || true
            ASSET_COUNT=$((ASSET_COUNT + 1))
        fi
    done
fi
print_success "Copied $ASSET_COUNT asset files"

# Copy static files if they exist
if [ -d "../static" ]; then
    print_info "Copying static web files..."
    mkdir -p "$STAGING_DIR/static"
    cp -R ../static/* "$STAGING_DIR/static/" 2>/dev/null || true
    print_success "Static files copied"
fi

# Create shell script for easy startup
print_info "Creating startup scripts..."
cat > "$STAGING_DIR/start-server.sh" << 'EOF'
#!/bin/bash
echo "========================================"
echo "   Kolosal Server v${VERSION}"
echo "========================================"
echo ""
echo "Starting Kolosal Server..."
echo "Server will be available at http://localhost:8080"
echo ""
echo "Press Ctrl+C to stop the server"
echo ""

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

./kolosal-server
EOF

# Replace ${VERSION} with actual version
sed -i.bak "s/\${VERSION}/$VERSION/g" "$STAGING_DIR/start-server.sh"
rm "$STAGING_DIR/start-server.sh.bak" 2>/dev/null || true

chmod +x "$STAGING_DIR/start-server.sh"
print_success "Created start-server.sh"

# Create README for the macOS version
cat > "$STAGING_DIR/MACOS-README.txt" << EOF
# Kolosal Server for macOS v$VERSION

This is a distribution of Kolosal Server for macOS.

## Quick Start

### Using Shell Script (Recommended)
1. Open Terminal
2. Navigate to this directory
3. Run: ./start-server.sh
4. The server will start on http://localhost:8080
5. Press Ctrl+C to stop

### Manual Start
1. Open Terminal in this directory
2. Run: ./kolosal-server
3. Or specify a config: ./kolosal-server --config configs/config.yaml

## Directory Structure

- kolosal-server - Main executable
- *.dylib - Required shared libraries
- lib/ - Additional shared libraries
- configs/ - Sample configuration files
- docs/ - Documentation
- assets/ - Icons and images
- data/ - Runtime data and FAISS indexes
- logs/ - Server logs
- models/ - Place your GGUF model files here

## Configuration

Copy one of the sample configs from the configs directory:
\`\`\`bash
cp configs/config.yaml config.yaml
\`\`\`

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

If you get "Permission denied" errors:
\`\`\`bash
chmod +x kolosal-server
chmod +x start-server.sh
\`\`\`

If you get security warnings about unidentified developer:
1. System Preferences > Security & Privacy
2. Click "Allow Anyway" for kolosal-server

## Support

- GitHub: https://github.com/KolosalAI/kolosal-server
- Documentation: See the docs folder
- Issues: https://github.com/KolosalAI/kolosal-server/issues

## Version Information

- Version: $VERSION
- Build Date: $(date +"%Y-%m-%d")
- Platform: macOS
- Package Type: DMG

EOF
print_success "Created MACOS-README.txt"

# Create a sample config file in the root if none exists
DEFAULT_CONFIG="$STAGING_DIR/config.yaml"
SAMPLE_CONFIG="$STAGING_DIR/configs/config.yaml"
if [ -f "$SAMPLE_CONFIG" ] && [ ! -f "$DEFAULT_CONFIG" ]; then
    cp "$SAMPLE_CONFIG" "$DEFAULT_CONFIG"
    print_success "Created default config.yaml"
fi

# Create the DMG file
echo ""
print_info "Creating DMG archive..."
if [ -f "$DMG_PATH" ]; then
    rm -f "$DMG_PATH"
fi

# Create temporary DMG
TEMP_DMG="$OUTPUT_DIR/temp-$DMG_NAME"

# Check if hdiutil is available (macOS only)
if command -v hdiutil &> /dev/null; then
    # Calculate size needed (add 50MB buffer)
    SIZE_NEEDED=$(du -sm "$STAGING_DIR" | awk '{print $1 + 50}')
    
    # Create DMG
    hdiutil create -volname "$VOLUME_NAME" -srcfolder "$STAGING_DIR" -ov -format UDZO "$DMG_PATH" > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        print_success "DMG archive created: $DMG_NAME"
        
        # Get file size
        DMG_SIZE=$(du -m "$DMG_PATH" | awk '{print $1}')
        print_success "Package size: ${DMG_SIZE} MB"
    else
        print_error "Failed to create DMG"
        exit 1
    fi
else
    # Fallback to tar.gz for non-macOS systems or if hdiutil is not available
    print_warning "hdiutil not available, creating tar.gz instead..."
    TAR_NAME="kolosal-server-$VERSION-macOS.tar.gz"
    TAR_PATH="$OUTPUT_DIR/$TAR_NAME"
    
    cd "$OUTPUT_DIR"
    tar -czf "$TAR_NAME" -C "$STAGING_DIR" .
    
    if [ $? -eq 0 ]; then
        print_success "tar.gz archive created: $TAR_NAME"
        
        # Get file size
        TAR_SIZE=$(du -m "$TAR_PATH" | awk '{print $1}')
        print_success "Package size: ${TAR_SIZE} MB"
        DMG_PATH="$TAR_PATH"
        DMG_NAME="$TAR_NAME"
    else
        print_error "Failed to create tar.gz"
        exit 1
    fi
    cd - > /dev/null
fi

# Clean up staging directory
print_info "Cleaning up staging directory..."
rm -rf "$STAGING_DIR"
print_success "Cleanup complete"

# Summary
echo ""
echo -e "\033[0;32m═══════════════════════════════════════════════════════\033[0m"
echo -e "\033[0;32m   macOS Installer Created Successfully!\033[0m"
echo -e "\033[0;32m═══════════════════════════════════════════════════════\033[0m"
echo ""
echo -e "Output: \033[0;33m$DMG_PATH\033[0m"
echo ""
echo -e "\033[0;36mTo distribute:\033[0m"
echo -e "\033[0;37m  1. Upload $DMG_NAME to your release page\033[0m"
echo -e "\033[0;37m  2. Users can mount the DMG and run start-server.sh\033[0m"
echo -e "\033[0;37m  3. Or extract and run kolosal-server directly\033[0m"
echo ""
