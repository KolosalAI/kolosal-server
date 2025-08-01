# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Common Development Commands

### Building the Inference Engine

**Standard CPU Build:**
```bash
# Linux/macOS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)              # Linux
make -j$(sysctl -n hw.ncpu) # macOS

# Windows (Visual Studio)
mkdir build; cd build
cmake ..
cmake --build . --config Release --parallel

# Windows (MinGW)
mkdir build; cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j$(nproc)
```

### Acceleration Backends

**CUDA Build (NVIDIA GPUs):**
```bash
cmake .. -DUSE_CUDA=ON -DCMAKE_BUILD_TYPE=Release
# Verify CUDA installation first: nvidia-smi
```

**Vulkan Build (Cross-platform GPU):**
```bash
cmake .. -DUSE_VULKAN=ON -DCMAKE_BUILD_TYPE=Release
# Install Vulkan SDK first: https://vulkan.lunarg.com/
```

**Metal Build (macOS only):**
```bash
cmake .. -DUSE_METAL=ON -DCMAKE_BUILD_TYPE=Release
# Automatically enabled on Apple Silicon Macs
```

**MPI Build (Distributed inference):**
```bash
cmake .. -DUSE_MPI=ON -DCMAKE_BUILD_TYPE=Release
# Requires MPI implementation (OpenMPI, MPICH, etc.)
```

### Debug Builds
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DDEBUG=ON
```

### Running Tests
```bash
# Build and run unit tests (if available)
cd build
ctest --verbose

# Run specific test
ctest -R test_name --verbose
```

### Platform-Specific Notes

**Windows MSVC Runtime:**
- All builds use Release runtime (/MD) to match pre-compiled dependencies
- This prevents LNK2038 runtime library mismatch errors
- Debug builds also use /MD with _ITERATOR_DEBUG_LEVEL=0

**Linux Dependencies:**
```bash
# Ubuntu/Debian
sudo apt install libcurl4-openssl-dev libssl-dev zlib1g-dev

# RHEL/CentOS/Fedora
sudo yum install libcurl-devel openssl-devel zlib-devel

# Arch
sudo pacman -S curl openssl zlib
```

## High-Level Architecture

### Inference Engine Overview

The kolosal-server inference engine is a modular LLM inference library that:
- Provides a unified C++ API for multiple acceleration backends
- Dynamically loads as a shared library (llama-cpu, llama-cuda, llama-vulkan, llama-metal)
- Built on top of llama.cpp for model compatibility
- Supports GGUF model format

### Key Components

1. **InferenceInterface (include/inference_interface.h)**
   - Pure virtual interface defining the inference API
   - Supports text completion, chat completion, and embeddings
   - Job-based asynchronous processing model

2. **InferenceEngine (include/inference.h, src/inference.cpp)**
   - Concrete implementation of InferenceInterface
   - Manages llama.cpp integration
   - Handles model loading, job queue, and result management

3. **Acceleration Backends**
   - **CPU**: Default, uses OpenBLAS for acceleration
   - **CUDA**: NVIDIA GPU acceleration via GGML_CUDA
   - **Vulkan**: Cross-platform GPU via GGML_VULKAN
   - **Metal**: Apple GPU acceleration via GGML_METAL

### Build System

- Uses CMake with position-independent code (-fPIC) for shared libraries
- Automatically configures llama.cpp as static library dependency
- Platform detection for optimal defaults
- Runtime library management for Windows compatibility

### API Usage Pattern

```cpp
// 1. Create engine instance
InferenceEngine engine;

// 2. Load model
LoadingParameters params;
params.n_ctx = 4096;
params.n_gpu_layers = -1; // Use all GPU layers
engine.loadModel("model.gguf", params);

// 3. Submit inference job
CompletionParameters completion;
completion.prompt = "Hello, world!";
completion.maxNewTokens = 100;
int jobId = engine.submitCompletionsJob(completion);

// 4. Wait for completion
engine.waitForJob(jobId);

// 5. Get results
if (!engine.hasJobError(jobId)) {
    CompletionResult result = engine.getJobResult(jobId);
}
```

### Grammar-Constrained Generation

The inference engine supports GBNF (Grammar-Based Natural Format) grammars to constrain output:

```cpp
// Define a JSON grammar
std::string json_grammar = R"(
root   ::= object
value  ::= object | array | string | number | ("true" | "false" | "null") ws
object ::= "{" ws (string ":" ws value ("," ws string ":" ws value)*)? "}" ws
array  ::= "[" ws (value ("," ws value)*)? "]" ws
string ::= "\"" ([^"\\] | "\\" (["\\/bfnrt] | "u" [0-9a-fA-F]{4}))* "\"" ws
number ::= ("-"? ([0-9] | [1-9] [0-9]*)) ("." [0-9]+)? ([eE] [-+]? [0-9]+)? ws
ws     ::= ([ \t\n] ws)?
)";

// Use grammar in completion
CompletionParameters params;
params.prompt = "Generate a JSON object:";
params.grammar = json_grammar;

// Works with chat completions too
ChatCompletionParameters chat_params;
chat_params.grammar = json_grammar;
```

See `examples/grammar_example.cpp` for more examples.

### JSON Schema Mode

The inference engine also supports JSON Schema for easier structured output generation:

```cpp
// Define a JSON schema
nlohmann::json schema = {
    {"type", "object"},
    {"properties", {
        {"name", {"type", "string"}},
        {"age", {
            {"type", "integer"},
            {"minimum", 0},
            {"maximum", 150}
        }},
        {"email", {
            {"type", "string"},
            {"format", "email"}
        }}
    }},
    {"required", {"name", "age", "email"}}
};

// Use with text completion
CompletionParameters params;
params.prompt = "Generate a person's information:";
params.jsonSchema = schema.dump();

// Use with chat completion
ChatCompletionParameters chat_params;
chat_params.jsonSchema = schema.dump();
```

The JSON schema is automatically converted to the appropriate GBNF grammar. This provides:
- Easier schema definition using standard JSON Schema
- Type validation (string, number, integer, boolean, array, object)
- Constraints (minimum, maximum, minLength, maxLength, enum, etc.)
- Required fields enforcement
- Nested object and array support

See `examples/json_schema_example.cpp` for comprehensive examples.

### Integration with kolosal-server

- Built as part of kolosal-server but isolated in inference/ subdirectory
- Loaded dynamically by main server based on available hardware
- Provides the core inference capabilities for the HTTP API endpoints

### Memory Management

- Models are memory-mapped for efficient loading
- Context size (n_ctx) determines maximum sequence length
- GPU layers (n_gpu_layers) control GPU memory usage
- Automatic fallback to CPU if GPU memory exhausted

### Thread Safety

- Job submission and retrieval are thread-safe
- Multiple concurrent inference requests supported
- Each job maintains its own context state

## Common Tasks

### Adding New Acceleration Backend
1. Add new CMake option in CMakeLists.txt
2. Configure GGML backend flags appropriately
3. Update TARGET_NAME logic for library naming
4. Add platform-specific dependency handling

### Debugging Inference Issues
1. Enable debug build: `-DDEBUG=ON`
2. Check model compatibility with llama.cpp
3. Verify memory requirements (RAM/VRAM)
4. Use smaller context size if OOM occurs

### Performance Optimization
- Adjust n_gpu_layers based on available VRAM
- Use appropriate n_batch for throughput
- Enable CUDA graphs if using CUDA backend
- Consider quantization for memory efficiency

## Troubleshooting Common Issues

### Build Failures

**"CUDA not found" Error:**
```bash
# Verify CUDA installation
nvidia-smi
nvcc --version

# Set CUDA paths explicitly
export CUDA_HOME=/usr/local/cuda
export PATH=$CUDA_HOME/bin:$PATH
export LD_LIBRARY_PATH=$CUDA_HOME/lib64:$LD_LIBRARY_PATH
```

**"Vulkan not found" Warning:**
```bash
# Install Vulkan SDK from https://vulkan.lunarg.com/
# Or use package manager:
sudo apt install libvulkan-dev vulkan-tools  # Ubuntu/Debian
sudo dnf install vulkan-devel vulkan-tools   # Fedora
```

**Windows Runtime Library Conflicts:**
- All dependencies must use /MD runtime
- Check third-party libraries match runtime configuration
- Use `dumpbin /DIRECTIVES library.lib` to verify

**"undefined reference to -fPIC" Errors:**
- Ensure CMAKE_POSITION_INDEPENDENT_CODE is ON
- Clean build directory and rebuild from scratch
- Check all static dependencies were built with -fPIC

### Runtime Issues

**"Model not found" or Loading Failures:**
- Verify model path is absolute
- Check file permissions
- Ensure sufficient disk space for memory mapping
- Validate GGUF format with `xxd -l 32 model.gguf`

**Out of Memory (OOM) Errors:**
```bash
# Reduce context size
params.n_ctx = 2048;  # Instead of 4096

# Reduce GPU layers
params.n_gpu_layers = 20;  # Instead of -1 (all)

# Use quantized models (Q4_K_M, Q5_K_M)
```

**Slow Inference Performance:**
- Check if using correct acceleration backend
- Verify GPU is being utilized: `nvidia-smi` or `vulkaninfo`
- Monitor CPU usage to detect CPU fallback
- Consider batch size optimization

**Segmentation Faults:**
- Enable debug build to get stack traces
- Check for memory corruption with valgrind
- Verify model compatibility with llama.cpp version
- Ensure thread safety in multi-threaded usage

### Platform-Specific Issues

**macOS Metal Errors:**
- Requires macOS 12.0+ for full Metal support
- Check Metal support: `system_profiler SPDisplaysDataType`
- May need to sign binaries for hardened runtime

**Linux Library Loading:**
```bash
# Check library dependencies
ldd llama-cpu.so

# Fix missing libraries
sudo ldconfig

# Set library path if needed
export LD_LIBRARY_PATH=/path/to/libs:$LD_LIBRARY_PATH
```

**Windows DLL Issues:**
- Ensure Visual C++ Redistributables installed
- Check PATH includes build/bin directory
- Use Dependency Walker to diagnose DLL issues