# Kolosal Server

A high-performance inference server for large language models with OpenAI-compatible API endpoints. Now available for both **Windows** and **Linux** systems!

## Platform Support

- 🪟 **Windows**: Full support with Visual Studio and MSVC
- 🐧 **Linux**: Native support with GCC/Clang
- 🎮 **GPU Acceleration**: NVIDIA CUDA and Vulkan support
- 📦 **Easy Installation**: Direct binary installation or build from source

## Features

- 🚀 **Fast Inference**: Built with llama.cpp for optimized model inference
- 🔗 **OpenAI Compatible**: Drop-in replacement for OpenAI API endpoints
- 📡 **Streaming Support**: Real-time streaming responses for chat completions
- 🎛️ **Multi-Model Management**: Load and manage multiple models simultaneously
- 📊 **Real-time Metrics**: Monitor completion performance with TPS, TTFT, and success rates
- ⚙️ **Lazy Loading**: Defer model loading until first request with `load_immediately=false`
- 🔧 **Configurable**: Flexible model loading parameters and inference settings
- 🔒 **Authentication**: API key and rate limiting support
- 🌐 **Cross-Platform**: Windows and Linux native builds

## Quick Start

### Linux (Recommended)

#### Prerequisites

**System Requirements:**
- Ubuntu 20.04+ or equivalent Linux distribution (CentOS 8+, Fedora 32+, Arch Linux)
- GCC 9+ or Clang 10+
- CMake 3.14+
- Git with submodule support
- At least 4GB RAM (8GB+ recommended for larger models)
- CUDA Toolkit 11.0+ (optional, for NVIDIA GPU acceleration)
- Vulkan SDK (optional, for alternative GPU acceleration)

**Install Dependencies:**

**Ubuntu/Debian:**
```bash
# Update package list
sudo apt update

# Install essential build tools
sudo apt install -y build-essential cmake git pkg-config

# Install required libraries
sudo apt install -y libcurl4-openssl-dev libyaml-cpp-dev

# Optional: Install CUDA for GPU support
# Follow NVIDIA's official installation guide for your distribution
```

**CentOS/RHEL/Fedora:**
```bash
# For CentOS/RHEL 8+
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git curl-devel yaml-cpp-devel

# For Fedora
sudo dnf install gcc-c++ cmake git libcurl-devel yaml-cpp-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake git curl yaml-cpp
```

#### Building from Source

**1. Clone the Repository:**
```bash
git clone https://github.com/kolosalai/kolosal-server.git
cd kolosal-server
```

**2. Initialize Submodules:**
```bash
git submodule update --init --recursive
```

**3. Create Build Directory:**
```bash
mkdir build && cd build
```

**4. Configure Build:**

**Standard Build (CPU-only):**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

**With CUDA Support:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DLLAMA_CUDA=ON ..
```

**With Vulkan Support:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DLLAMA_VULKAN=ON ..
```

**Debug Build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**5. Build the Project:**
```bash
# Use all available CPU cores
make -j$(nproc)

# Or specify number of cores manually
make -j4
```

**6. Verify Build:**
```bash
# Check if the executable was created
ls -la kolosal-server

# Test basic functionality
./kolosal-server --help
```

#### Running the Server

**Start the Server:**
```bash
# From build directory
./kolosal-server

# Or specify a config file
./kolosal-server --config ../config.yaml
```

**Background Service:**
```bash
# Run in background
nohup ./kolosal-server > server.log 2>&1 &

# Check if running
ps aux | grep kolosal-server
```

**Check Server Status:**
```bash
# Test if server is responding
curl http://localhost:8080/v1/health
```

#### Alternative Installation Methods

**Install to System Path:**
```bash
# Install binary to /usr/local/bin
sudo cp build/kolosal-server /usr/local/bin/

# Make it executable
sudo chmod +x /usr/local/bin/kolosal-server

# Now you can run from anywhere
kolosal-server --help
```

**Install with Package Manager (Future):**
```bash
# Note: Package manager installation will be available in future releases
# For now, use the build from source method above
```

#### Installation as System Service

**Create Service File:**
```bash
sudo tee /etc/systemd/system/kolosal-server.service > /dev/null << EOF
[Unit]
Description=Kolosal Server - LLM Inference Server
After=network.target

[Service]
Type=simple
User=kolosal
Group=kolosal
WorkingDirectory=/opt/kolosal-server
ExecStart=/opt/kolosal-server/kolosal-server --config /etc/kolosal-server/config.yaml
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
```

**Enable and Start Service:**
```bash
# Create user for service
sudo useradd -r -s /bin/false kolosal

# Install binary and config
sudo mkdir -p /opt/kolosal-server /etc/kolosal-server
sudo cp build/kolosal-server /opt/kolosal-server/
sudo cp config.example.yaml /etc/kolosal-server/config.yaml
sudo chown -R kolosal:kolosal /opt/kolosal-server

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable kolosal-server
sudo systemctl start kolosal-server

# Check status
sudo systemctl status kolosal-server
```

#### Troubleshooting

**Common Build Issues:**

1. **Missing dependencies:**
   ```bash
   # Check for missing packages
   ldd build/kolosal-server
   
   # Install missing development packages
   sudo apt install -y libssl-dev libcurl4-openssl-dev
   ```

2. **CMake version too old:**
   ```bash
   # Install newer CMake from Kitware APT repository
   wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
   sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
   sudo apt update && sudo apt install cmake
   ```

3. **CUDA compilation errors:**
   ```bash
   # Verify CUDA installation
   nvcc --version
   nvidia-smi
   
   # Set CUDA environment variables if needed
   export CUDA_HOME=/usr/local/cuda
   export PATH=$CUDA_HOME/bin:$PATH
   export LD_LIBRARY_PATH=$CUDA_HOME/lib64:$LD_LIBRARY_PATH
   ```

4. **Permission issues:**
   ```bash
   # Fix ownership
   sudo chown -R $USER:$USER ./build
   
   # Make executable
   chmod +x build/kolosal-server
   ```

**Performance Optimization:**

1. **CPU Optimization:**
   ```bash
   # Build with native optimizations
   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=native" ..
   ```

2. **Memory Settings:**
   ```bash
   # For systems with limited RAM, reduce parallel jobs
   make -j2
   
   # Set memory limits in config
   echo "server.max_memory_mb: 4096" >> config.yaml
   ```

3. **GPU Memory:**
   ```bash
   # Monitor GPU usage
   watch nvidia-smi
   
   # Adjust GPU layers in model config
   # Reduce n_gpu_layers if running out of VRAM
   ```

### Windows

**Prerequisites:**
- Windows 10/11
- Visual Studio 2019 or later
- CMake 3.20+
- CUDA Toolkit (optional, for GPU acceleration)

**Building:**
```bash
git clone https://github.com/kolosalai/kolosal-server.git
cd kolosal-server
mkdir build && cd build
cmake ..
cmake --build . --config Debug
```

### Running the Server

```bash
./Debug/kolosal-server.exe
```

The server will start on `http://localhost:8080` by default.

## Configuration

Kolosal Server supports configuration through JSON and YAML files for advanced setup including authentication, logging, model preloading, and server parameters.

### Quick Configuration Examples

#### Minimal Configuration (`config.yaml`)

```yaml
server:
  port: "8080"

models:
  - id: "my-model"
    path: "./models/model.gguf"
    load_at_startup: true
```

#### Production Configuration

```yaml
server:
  port: "8080"
  max_connections: 500
  worker_threads: 8

auth:
  enabled: true
  require_api_key: true
  api_keys:
    - "sk-your-api-key-here"

models:
  - id: "gpt-3.5-turbo"
    path: "./models/gpt-3.5-turbo.gguf"
    load_at_startup: true
    main_gpu_id: 0
    load_params:
      n_ctx: 4096
      n_gpu_layers: 50

features:
  metrics: true  # Enable /metrics and /completion-metrics
```

For complete configuration documentation including all parameters, authentication setup, CORS configuration, and more examples, see the **[Configuration Guide](docs/CONFIGURATION.md)**.

## API Usage

### 1. Add a Model Engine

Before using chat completions, you need to add a model engine:

```bash
curl -X POST http://localhost:8080/engines \
  -H "Content-Type: application/json" \
  -d '{
    "engine_id": "my-model",
    "model_path": "path/to/your/model.gguf",
    "load_immediately": true,
    "n_ctx": 2048,
    "n_gpu_layers": 0,
    "main_gpu_id": 0
  }'
```

#### Lazy Loading

For faster startup times, you can defer model loading until first use:

```bash
curl -X POST http://localhost:8080/engines \
  -H "Content-Type: application/json" \
  -d '{
    "engine_id": "my-model",
    "model_path": "https://huggingface.co/model-repo/model.gguf",
    "load_immediately": false,
    "n_ctx": 4096,
    "n_gpu_layers": 30,
    "main_gpu_id": 0
  }'
```

### 2. Chat Completions

#### Non-Streaming Chat Completion

```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "my-model",
    "messages": [
      {
        "role": "user",
        "content": "Hello, how are you today?"
      }
    ],
    "stream": false,
    "temperature": 0.7,
    "max_tokens": 100
  }'
```

**Response:**
```json
{
  "choices": [
    {
      "finish_reason": "stop",
      "index": 0,
      "message": {
        "content": "Hello! I'm doing well, thank you for asking. How can I help you today?",
        "role": "assistant"
      }
    }
  ],
  "created": 1749981228,
  "id": "chatcmpl-80HTkM01z7aaaThFbuALkbTu",
  "model": "my-model",
  "object": "chat.completion",
  "system_fingerprint": "fp_4d29efe704",
  "usage": {
    "completion_tokens": 15,
    "prompt_tokens": 9,
    "total_tokens": 24
  }
}
```

#### Streaming Chat Completion

```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -d '{
    "model": "my-model",
    "messages": [
      {
        "role": "user",
        "content": "Tell me a short story about a robot."
      }
    ],
    "stream": true,
    "temperature": 0.8,
    "max_tokens": 150
  }'
```

**Response (Server-Sent Events):**
```
data: {"choices":[{"delta":{"content":"","role":"assistant"},"finish_reason":null,"index":0}],"created":1749981242,"id":"chatcmpl-1749981241-1","model":"my-model","object":"chat.completion.chunk","system_fingerprint":"fp_4d29efe704"}

data: {"choices":[{"delta":{"content":"Once"},"finish_reason":null,"index":0}],"created":1749981242,"id":"chatcmpl-1749981241-1","model":"my-model","object":"chat.completion.chunk","system_fingerprint":"fp_4d29efe704"}

data: {"choices":[{"delta":{"content":" upon"},"finish_reason":null,"index":0}],"created":1749981242,"id":"chatcmpl-1749981241-1","model":"my-model","object":"chat.completion.chunk","system_fingerprint":"fp_4d29efe704"}

data: {"choices":[{"delta":{"content":""},"finish_reason":"stop","index":0}],"created":1749981242,"id":"chatcmpl-1749981241-1","model":"my-model","object":"chat.completion.chunk","system_fingerprint":"fp_4d29efe704"}

data: [DONE]
```

#### Multi-Message Conversation

```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "my-model",
    "messages": [
      {
        "role": "system",
        "content": "You are a helpful programming assistant."
      },
      {
        "role": "user",
        "content": "How do I create a simple HTTP server in Python?"
      },
      {
        "role": "assistant",
        "content": "You can create a simple HTTP server in Python using the built-in http.server module..."
      },
      {
        "role": "user",
        "content": "Can you show me the code?"
      }
    ],
    "stream": false,
    "temperature": 0.7,
    "max_tokens": 200
  }'
```

#### Advanced Parameters

```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "my-model",
    "messages": [
      {
        "role": "user",
        "content": "What is the capital of France?"
      }
    ],
    "stream": false,
    "temperature": 0.1,
    "top_p": 0.9,
    "max_tokens": 50,
    "seed": 42,
    "presence_penalty": 0.0,
    "frequency_penalty": 0.0
  }'
```

### 3. Completions

#### Non-Streaming Completion

```bash
curl -X POST http://localhost:8080/v1/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "my-model",
    "prompt": "The future of artificial intelligence is",
    "stream": false,
    "temperature": 0.7,
    "max_tokens": 100
  }'
```

**Response:**
```json
{
  "choices": [
    {
      "finish_reason": "stop",
      "index": 0,
      "text": " bright and full of possibilities. As we continue to advance in machine learning and deep learning technologies, we can expect to see significant improvements in various fields..."
    }
  ],
  "created": 1749981288,
  "id": "cmpl-80HTkM01z7aaaThFbuALkbTu",
  "model": "my-model",
  "object": "text_completion",
  "usage": {
    "completion_tokens": 25,
    "prompt_tokens": 8,
    "total_tokens": 33
  }
}
```

#### Streaming Completion

```bash
curl -X POST http://localhost:8080/v1/completions \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -d '{
    "model": "my-model",
    "prompt": "Write a haiku about programming:",
    "stream": true,
    "temperature": 0.8,
    "max_tokens": 50
  }'
```

**Response (Server-Sent Events):**
```
data: {"choices":[{"finish_reason":"","index":0,"text":""}],"created":1749981290,"id":"cmpl-1749981289-1","model":"my-model","object":"text_completion"}

data: {"choices":[{"finish_reason":"","index":0,"text":"Code"}],"created":1749981290,"id":"cmpl-1749981289-1","model":"my-model","object":"text_completion"}

data: {"choices":[{"finish_reason":"","index":0,"text":" flows"}],"created":1749981290,"id":"cmpl-1749981289-1","model":"my-model","object":"text_completion"}

data: {"choices":[{"finish_reason":"stop","index":0,"text":""}],"created":1749981290,"id":"cmpl-1749981289-1","model":"my-model","object":"text_completion"}

data: [DONE]
```

#### Multiple Prompts

```bash
curl -X POST http://localhost:8080/v1/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "my-model",
    "prompt": [
      "The weather today is",
      "In other news,"
    ],
    "stream": false,
    "temperature": 0.5,
    "max_tokens": 30
  }'
```

#### Advanced Completion Parameters

```bash
curl -X POST http://localhost:8080/v1/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "my-model",
    "prompt": "Explain quantum computing:",
    "stream": false,
    "temperature": 0.2,
    "top_p": 0.9,
    "max_tokens": 100,
    "seed": 123,
    "presence_penalty": 0.0,
    "frequency_penalty": 0.1
  }'
```

### 4. Engine Management

#### List Available Engines

```bash
curl -X GET http://localhost:8080/v1/engines
```

#### Get Engine Status

```bash
curl -X GET http://localhost:8080/engines/my-model/status
```

#### Remove an Engine

```bash
curl -X DELETE http://localhost:8080/engines/my-model
```

### 5. Completion Metrics and Monitoring

The server provides real-time completion metrics for monitoring performance and usage:

#### Get Completion Metrics

```bash
curl -X GET http://localhost:8080/completion-metrics
```

**Response:**
```json
{
  "completion_metrics": {
    "summary": {
      "total_requests": 15,
      "completed_requests": 14,
      "failed_requests": 1,
      "success_rate_percent": 93.33,
      "total_input_tokens": 120,
      "total_output_tokens": 350,
      "avg_turnaround_time_ms": 1250.5,
      "avg_tps": 12.8,
      "avg_output_tps": 8.4,
      "avg_ttft_ms": 245.2,
      "avg_rps": 0.85
    },
    "per_engine": [
      {
        "model_name": "my-model",
        "engine_id": "default",
        "total_requests": 15,
        "completed_requests": 14,
        "failed_requests": 1,
        "total_input_tokens": 120,
        "total_output_tokens": 350,
        "tps": 12.8,
        "output_tps": 8.4,
        "avg_ttft": 245.2,
        "rps": 0.85,
        "last_updated": "2025-06-16T17:04:12.123Z"
      }
    ],
    "timestamp": "2025-06-16T17:04:12.123Z"
  }
}
```

**Alternative endpoints:**
```bash
# OpenAI-style endpoint
curl -X GET http://localhost:8080/v1/completion-metrics

# Alternative path
curl -X GET http://localhost:8080/completion/metrics
```

#### Metrics Explained

| Metric | Description |
|--------|-------------|
| `total_requests` | Total number of completion requests received |
| `completed_requests` | Number of successfully completed requests |
| `failed_requests` | Number of requests that failed |
| `success_rate_percent` | Success rate as a percentage |
| `total_input_tokens` | Total input tokens processed |
| `total_output_tokens` | Total output tokens generated |
| `avg_turnaround_time_ms` | Average time from request to completion (ms) |
| `avg_tps` | Average tokens per second (input + output) |
| `avg_output_tps` | Average output tokens per second |
| `avg_ttft_ms` | Average time to first token (ms) |
| `avg_rps` | Average requests per second |

#### PowerShell Example

```powershell
# Get completion metrics
$metrics = Invoke-RestMethod -Uri "http://localhost:8080/completion-metrics" -Method GET
Write-Output "Success Rate: $($metrics.completion_metrics.summary.success_rate_percent)%"
Write-Output "Average TPS: $($metrics.completion_metrics.summary.avg_tps)"
```

### 6. Health Check

```bash
curl -X GET http://localhost:8080/v1/health
```

## Parameters Reference

### Chat Completion Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `model` | string | required | The ID of the model to use |
| `messages` | array | required | List of message objects |
| `stream` | boolean | false | Whether to stream responses |
| `temperature` | number | 1.0 | Sampling temperature (0.0-2.0) |
| `top_p` | number | 1.0 | Nucleus sampling parameter |
| `max_tokens` | integer | 128 | Maximum tokens to generate |
| `seed` | integer | random | Random seed for reproducible outputs |
| `presence_penalty` | number | 0.0 | Presence penalty (-2.0 to 2.0) |
| `frequency_penalty` | number | 0.0 | Frequency penalty (-2.0 to 2.0) |

### Completion Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `model` | string | required | The ID of the model to use |
| `prompt` | string/array | required | Text prompt or array of prompts |
| `stream` | boolean | false | Whether to stream responses |
| `temperature` | number | 1.0 | Sampling temperature (0.0-2.0) |
| `top_p` | number | 1.0 | Nucleus sampling parameter |
| `max_tokens` | integer | 16 | Maximum tokens to generate |
| `seed` | integer | random | Random seed for reproducible outputs |
| `presence_penalty` | number | 0.0 | Presence penalty (-2.0 to 2.0) |
| `frequency_penalty` | number | 0.0 | Frequency penalty (-2.0 to 2.0) |

### Message Object

| Field | Type | Description |
|-------|------|-------------|
| `role` | string | Role: "system", "user", or "assistant" |
| `content` | string | The content of the message |

### Engine Loading Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `engine_id` | string | required | Unique identifier for the engine |
| `model_path` | string | required | Path to the GGUF model file or URL |
| `load_immediately` | boolean | true | Whether to load the model immediately or defer until first use |
| `n_ctx` | integer | 4096 | Context window size |
| `n_gpu_layers` | integer | 100 | Number of layers to offload to GPU |
| `main_gpu_id` | integer | 0 | Primary GPU device ID |

## Error Handling

The server returns standard HTTP status codes and JSON error responses:

```json
{
  "error": {
    "message": "Model 'non-existent-model' not found or could not be loaded",
    "type": "invalid_request_error",
    "param": null,
    "code": null
  }
}
```

Common error codes:
- `400` - Bad Request (invalid JSON, missing parameters)
- `404` - Not Found (model/engine not found)
- `500` - Internal Server Error (inference failures)

## Examples with PowerShell

For Windows users, here are PowerShell equivalents:

### Add Engine
```powershell
$body = @{
    engine_id = "my-model"
    model_path = "C:\path\to\model.gguf"
    load_immediately = $true
    n_ctx = 2048
    n_gpu_layers = 0
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:8080/engines" -Method POST -Body $body -ContentType "application/json"
```

### Chat Completion
```powershell
$body = @{
    model = "my-model"
    messages = @(
        @{
            role = "user"
            content = "Hello, how are you?"
        }
    )
    stream = $false
    temperature = 0.7
    max_tokens = 100
} | ConvertTo-Json -Depth 3

Invoke-RestMethod -Uri "http://localhost:8080/v1/chat/completions" -Method POST -Body $body -ContentType "application/json"
```

### Completion
```powershell
$body = @{
    model = "my-model"
    prompt = "The future of AI is"
    stream = $false
    temperature = 0.7
    max_tokens = 50
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:8080/v1/completions" -Method POST -Body $body -ContentType "application/json"
```

## 📚 Developer Documentation

For developers looking to contribute to or extend Kolosal Server, comprehensive documentation is available in the [`docs/`](docs/) directory:

### 🚀 Getting Started
- **[Developer Guide](docs/DEVELOPER_GUIDE.md)** - Complete setup, architecture, and development workflows
- **[Configuration Guide](docs/CONFIGURATION.md)** - Complete server configuration in JSON and YAML formats
- **[Architecture Overview](docs/ARCHITECTURE.md)** - Detailed system design and component relationships

### 🔧 Implementation Guides
- **[Adding New Routes](docs/ADDING_ROUTES.md)** - Step-by-step guide for implementing API endpoints
- **[Adding New Models](docs/ADDING_MODELS.md)** - Guide for creating data models and JSON handling
- **[API Specification](docs/API_SPECIFICATION.md)** - Complete API reference with examples

### 📖 Quick Links
- [Documentation Index](docs/README.md) - Complete documentation overview
- [Project Structure](docs/DEVELOPER_GUIDE.md#project-structure) - Understanding the codebase
- [Contributing Guidelines](docs/DEVELOPER_GUIDE.md#contributing) - How to contribute

## Acknowledgments

Kolosal Server is built on top of excellent open-source projects and we want to acknowledge their contributions:

### llama.cpp
This project is powered by [llama.cpp](https://github.com/ggml-org/llama.cpp), developed by [Georgi Gerganov](https://github.com/ggerganov) and the [ggml-org](https://github.com/ggml-org) community. llama.cpp provides the high-performance inference engine that makes Kolosal Server possible.

- **Project**: [https://github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- **License**: MIT License
- **Description**: Inference of Meta's LLaMA model (and others) in pure C/C++

We extend our gratitude to the llama.cpp team for their incredible work on optimized LLM inference, which forms the foundation of our server's performance capabilities.

### Other Dependencies
- **[yaml-cpp](https://github.com/jbeder/yaml-cpp)**: YAML parsing and emitting library
- **[nlohmann/json](https://github.com/nlohmann/json)**: JSON library for Modern C++
- **[libcurl](https://curl.se/libcurl/)**: Client-side URL transfer library
- **[prometheus-cpp](https://github.com/jupp0r/prometheus-cpp)**: Prometheus metrics library for C++

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Contributing

We welcome contributions! Please see our [Developer Documentation](docs/) for detailed guides on:

1. **Getting Started**: [Developer Guide](docs/DEVELOPER_GUIDE.md)
2. **Understanding the System**: [Architecture Overview](docs/ARCHITECTURE.md)
3. **Adding Features**: [Route](docs/ADDING_ROUTES.md) and [Model](docs/ADDING_MODELS.md) guides
4. **API Changes**: [API Specification](docs/API_SPECIFICATION.md)

### Quick Contributing Steps
1. Fork the repository
2. Follow the [Developer Guide](docs/DEVELOPER_GUIDE.md) for setup
3. Create a feature branch
4. Implement your changes following our guides
5. Add tests and update documentation
6. Submit a Pull Request

## Support

- **Issues**: Report bugs and feature requests on [GitHub Issues](https://github.com/your-org/kolosal-server/issues)
- **Documentation**: Check the [docs/](docs/) directory for comprehensive guides
- **Discussions**: Join [Kolosal AI Discord](https://discord.gg/NCufxNCB) for questions and community support
