# Kolosal Server CLI Configuration
# This file shows how to configure CMake to enable the CLI interface

## Basic CLI Configuration

### 1. Enable CLI Interface
```bash
# Configure with CLI enabled (default)
cmake .. -DENABLE_CLI=ON

# Configure with CLI disabled
cmake .. -DENABLE_CLI=OFF
```

### 2. Build Types
```bash
# Release build with CLI
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_CLI=ON

# Debug build with CLI
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_CLI=ON -DDEBUG=ON
```

### 3. Platform-Specific Configuration

#### Windows (Visual Studio)
```bash
# Visual Studio 2022 with CLI
cmake .. -G "Visual Studio 17 2022" -A x64 -DENABLE_CLI=ON

# Visual Studio 2019 with CLI
cmake .. -G "Visual Studio 16 2019" -A x64 -DENABLE_CLI=ON
```

#### Linux/macOS
```bash
# Unix Makefiles with CLI
cmake .. -G "Unix Makefiles" -DENABLE_CLI=ON

# Ninja build with CLI
cmake .. -G "Ninja" -DENABLE_CLI=ON
```

## Advanced Configuration

### 4. CLI with All Features
```bash
# Full feature build with CLI, agents, and acceleration
cmake .. \
  -DENABLE_CLI=ON \
  -DENABLE_AGENTS=ON \
  -DUSE_CUDA=ON \
  -DCMAKE_BUILD_TYPE=Release
```

### 5. CLI-Only Build (Minimal)
```bash
# Minimal build with just CLI and basic server
cmake .. \
  -DENABLE_CLI=ON \
  -DENABLE_AGENTS=OFF \
  -DUSE_CUDA=OFF \
  -DUSE_VULKAN=OFF \
  -DCMAKE_BUILD_TYPE=Release
```

### 6. Development Build
```bash
# Development build with all debugging features
cmake .. \
  -DENABLE_CLI=ON \
  -DENABLE_AGENTS=ON \
  -DDEBUG=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## CMake Options Reference

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_CLI` | `ON` | Enable CLI interface |
| `ENABLE_AGENTS` | `ON` | Enable multi-agent system |
| `USE_CUDA` | `OFF` | Enable CUDA acceleration |
| `USE_VULKAN` | `OFF` | Enable Vulkan acceleration |
| `USE_MPI` | `OFF` | Enable MPI support |
| `DEBUG` | `OFF` | Enable debug information |

## Build Commands

### Windows
```bash
# Configure
cmake .. -G "Visual Studio 17 2022" -A x64 -DENABLE_CLI=ON

# Build
cmake --build . --config Release

# Run CLI
Release\kolosal-server.exe --cli
```

### Linux/macOS
```bash
# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_CLI=ON

# Build
make -j$(nproc)

# Run CLI
./kolosal-server --cli
```

## Complete Examples

### Example 1: Quick CLI Setup
```bash
# One-liner for quick CLI setup
mkdir build && cd build && cmake .. -DENABLE_CLI=ON && cmake --build . --config Release
```

### Example 2: Production CLI Build
```bash
mkdir build-cli
cd build-cli
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_CLI=ON \
  -DENABLE_AGENTS=ON \
  -DUSE_CUDA=ON \
  -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
make install
```

### Example 3: Cross-Platform Development
```bash
# Windows
cmake .. -G "Visual Studio 17 2022" -A x64 -DENABLE_CLI=ON -DDEBUG=ON

# Linux
cmake .. -G "Unix Makefiles" -DENABLE_CLI=ON -DDEBUG=ON

# macOS
cmake .. -G "Xcode" -DENABLE_CLI=ON -DDEBUG=ON
```

## Verification

### Check if CLI is Enabled
```bash
# Check compile definitions
cmake .. -DENABLE_CLI=ON
grep -r "KOLOSAL_CLI_ENABLED" build/

# Check built binary
./kolosal-server --help
# Should show: "-c, --cli    Start in CLI mode instead of server mode"
```

### Test CLI Functionality
```bash
# Start CLI mode
./kolosal-server --cli

# In CLI, test commands:
kolosal> help
kolosal> status
kolosal> /version
kolosal> exit
```

## Troubleshooting

### Common Issues

1. **CLI option not showing in help**
   - Solution: Ensure `-DENABLE_CLI=ON` was used during configuration
   - Rebuild completely: `rm -rf build && mkdir build && cd build`

2. **Missing CLI source files**
   - Error: `No such file or directory: 'cli_interface.cpp'`
   - Solution: Ensure all CLI source files are present in `src/` directory

3. **Compilation errors with structured bindings**
   - Error: `binary '=': no operator found`
   - Solution: Use C++17 compatible compiler or fix structured bindings

4. **Runtime CLI failures**
   - Error: `CLI mode is not enabled in this build`
   - Solution: Recompile with `-DENABLE_CLI=ON`

### Debug Configuration
```bash
# Debug build with verbose output
cmake .. -DENABLE_CLI=ON -DDEBUG=ON -DCMAKE_VERBOSE_MAKEFILE=ON
```

## Integration with IDE

### Visual Studio Code
```json
// .vscode/settings.json
{
    "cmake.configureArgs": [
        "-DENABLE_CLI=ON",
        "-DENABLE_AGENTS=ON",
        "-DCMAKE_BUILD_TYPE=Debug"
    ]
}
```

### CLion
```bash
# CMake options in CLion settings
-DENABLE_CLI=ON -DENABLE_AGENTS=ON -DCMAKE_BUILD_TYPE=Debug
```
