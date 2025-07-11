# Inference Engine Library

This directory contains the inference engine as a separate shared library that can be compiled with different acceleration backends (CPU, CUDA, Vulkan).

## Structure

```
inference/
├── CMakeLists.txt          # Build configuration for the inference library
├── include/
│   ├── inference.h         # Main inference engine interface
│   └── inference_interface.h # Abstract interface definitions
└── src/
    └── inference.cpp       # Implementation
```

## Build Options

The inference library supports several build-time options:

- `USE_CUDA=ON`: Enable CUDA acceleration (creates `llama-cuda` library)
- `USE_VULKAN=ON`: Enable Vulkan acceleration (creates `llama-vulkan` library)
- `USE_MPI=ON`: Enable MPI support for distributed inference
- `DEBUG=ON`: Enable debug information
- `ENABLE_NATIVE_OPTIMIZATION=ON`: Enable native CPU optimizations (-march=native)

## Generated Libraries

Based on the build options, the following shared libraries are created:

- `llama-cpu`: CPU-only inference (default)
- `llama-cuda`: CUDA-accelerated inference
- `llama-vulkan`: Vulkan-accelerated inference

## Dependencies

The inference library automatically handles all dependencies including:

- llama.cpp (built as static libraries)
- CURL (for model downloading)
- yaml-cpp (for configuration)
- OpenSSL and zlib (on Linux)
- CUDA SDK (when USE_CUDA=ON)
- Vulkan SDK (when USE_VULKAN=ON)
- MPI implementation (when USE_MPI=ON)

## Integration

The main kolosal-server automatically links against the appropriate inference library. The library name is determined at build time based on the acceleration options selected.

### Example Usage

```cpp
#include "inference.h"

// Create inference engine
InferenceEngine engine;

// Load model
LoadingParameters loadParams;
loadParams.n_ctx = 4096;
loadParams.n_gpu_layers = 32;  // GPU acceleration
if (!engine.loadModel("model.gguf", loadParams)) {
    // Handle error
}

// Text completion
CompletionParameters params;
params.prompt = "Hello, world!";
params.maxNewTokens = 50;
params.temperature = 0.7f;

int jobId = engine.submitCompletionsJob(params);
engine.waitForJob(jobId);

if (!engine.hasJobError(jobId)) {
    CompletionResult result = engine.getJobResult(jobId);
    std::cout << "Generated: " << result.text << std::endl;
}
```

## Building

The inference library is built automatically when building the main kolosal-server project:

```bash
# Build with CPU acceleration (default)
cmake -B build
cmake --build build

# Build with CUDA acceleration
cmake -B build -DUSE_CUDA=ON
cmake --build build

# Build with Vulkan acceleration
cmake -B build -DUSE_VULKAN=ON
cmake --build build
```

## Output

The compiled libraries are placed in:
- **Libraries**: `build/lib/`
- **Executables**: `build/bin/`
- **Headers**: Installed to `include/` during installation

## Platform Support

- **Windows**: MSVC, MinGW
- **Linux**: GCC, Clang
- **Architecture**: x86_64, ARM64/AArch64

## Cross-Platform Notes

### Windows
- Automatically handles libcurl.dll copying
- Uses bundled CURL libraries from `external/curl`
- Supports both static and dynamic linking

### Linux
- Uses system CURL libraries
- Automatically detects and links OpenSSL and zlib
- Supports package managers: apt, yum, dnf, pacman

### ARM64
- Automatically enables position-independent code (-fPIC)
- Optimized for ARM64 processors
