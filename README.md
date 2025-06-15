# Kolosal Server

A high-performance inference server for large language models with OpenAI-compatible API endpoints.

## Features

- 🚀 **Fast Inference**: Built with llama.cpp for optimized model inference
- 🔗 **OpenAI Compatible**: Drop-in replacement for OpenAI API endpoints
- 📡 **Streaming Support**: Real-time streaming responses for chat completions
- 🎛️ **Multi-Model Management**: Load and manage multiple models simultaneously
- 🔧 **Configurable**: Flexible model loading parameters and inference settings

## Quick Start

### Prerequisites

- Windows 10/11
- Visual Studio 2019 or later
- CMake 3.20+
- CUDA Toolkit (optional, for GPU acceleration)

### Building

```bash
git clone https://github.com/your-org/kolosal-server.git
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

## API Usage

### 1. Add a Model Engine

Before using chat completions, you need to add a model engine:

```bash
curl -X POST http://localhost:8080/engines \
  -H "Content-Type: application/json" \
  -d '{
    "engine_id": "my-model",
    "model_path": "path/to/your/model.gguf",
    "n_ctx": 2048,
    "n_gpu_layers": 0,
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

### 4. Health Check

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
| `model_path` | string | required | Path to the GGUF model file |
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

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

## Support

For issues and questions, please open an issue on GitHub.