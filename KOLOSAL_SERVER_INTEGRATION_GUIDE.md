# Kolosal Server Integration Guide

This guide provides comprehensive information for AI systems working with the Kolosal Server - a high-performance inference server for Large Language Models (LLMs) with OpenAI-compatible APIs.

## Table of Contents

1. [Overview](#overview)
2. [Server Configuration](#server-configuration)
3. [Executable Parameters](#executable-parameters)
4. [API Endpoints](#api-endpoints)
5. [Authentication & Security](#authentication--security)
6. [Monitoring & Metrics](#monitoring--metrics)
7. [Download Management](#download-management)
8. [Error Handling](#error-handling)
9. [Best Practices](#best-practices)
10. [Integration Examples](#integration-examples)

## Overview

**Kolosal Server** is an enterprise-grade LLM inference server that provides:
- **OpenAI-compatible API** for seamless integration
- **Dynamic model loading** with GGUF format support
- **GPU acceleration** (CUDA/Vulkan)
- **Real-time monitoring** and metrics
- **Download management** for remote models
- **Authentication & rate limiting**
- **Cross-platform support** (Windows/Linux)

**Base URL**: `http://localhost:8080` (configurable)

## Server Configuration

The server supports both JSON and YAML configuration formats. Create a `config.yaml` or `config.json` file:

### Complete Configuration Schema

```yaml
# Server settings
server:
  port: "8080"                    # Server port (string)
  host: "0.0.0.0"                # Bind host (0.0.0.0 for all interfaces)
  max_connections: 100            # Maximum concurrent connections
  worker_threads: 0               # Worker threads (0 = auto-detect)
  request_timeout: 30             # Request timeout in seconds
  max_request_size: 16777216      # Max request size in bytes (16MB)
  idle_timeout: 300               # Connection idle timeout in seconds
  allow_public_access: false      # Allow access from other devices
  allow_internet_access: false    # Allow internet access (requires port forwarding)

# Logging configuration
logging:
  level: "INFO"                   # Log level: DEBUG, INFO, WARN, ERROR
  file: ""                        # Log file path (empty = console only)
  access_log: false               # Enable HTTP access logging

# Authentication and security
auth:
  enabled: true                   # Enable authentication system
  require_api_key: false          # Require API key for all requests
  api_key_header: "X-API-Key"     # Header name for API keys
  api_keys:                       # List of valid API keys
    - "sk-your-api-key-here"
    - "sk-another-key"
  
  # Rate limiting
  rate_limit:
    enabled: true                 # Enable rate limiting
    max_requests: 100             # Max requests per window
    window_size: 60               # Window size in seconds
  
  # CORS settings
  cors:
    enabled: true                 # Enable CORS
    allow_credentials: false      # Allow credentials in CORS requests
    max_age: 86400               # Preflight cache duration (seconds)
    allowed_origins:              # Allowed origins (* for all)
      - "*"
    allowed_methods:              # Allowed HTTP methods
      - "GET"
      - "POST"
      - "PUT"
      - "DELETE"
      - "OPTIONS"
      - "HEAD"
      - "PATCH"
    allowed_headers:              # Allowed headers
      - "Content-Type"
      - "Authorization"
      - "X-API-Key"
      - "Accept"
      - "Origin"
      - "X-Requested-With"

# Model preloading (optional)
models:
  - id: "gpt-3.5-turbo"          # Model identifier
    path: "./models/model.gguf"   # Local path or URL
    load_immediately: true         # Load immediately on startup
    main_gpu_id: 0               # Primary GPU device ID
    load_params:                  # Model loading parameters
      n_ctx: 4096                # Context window size
      n_gpu_layers: 50           # GPU layers to offload
      use_mmap: true             # Use memory mapping
      use_mlock: false           # Lock model in memory

# Feature toggles
features:
  health_check: true             # Enable /health endpoint
  metrics: true                  # Enable metrics endpoints
```

### Environment Variables

Override configuration with environment variables:

```bash
export KOLOSAL_PORT="8080"
export KOLOSAL_HOST="0.0.0.0"
export KOLOSAL_API_KEY="sk-your-key"
export KOLOSAL_LOG_LEVEL="INFO"
export KOLOSAL_REQUIRE_API_KEY="true"
```

## Executable Parameters

### Command Line Usage

```bash
# Basic usage
./kolosal-server --config config.yaml

# With command line overrides
./kolosal-server --port 8080 --host 0.0.0.0 --api-key "sk-key"

# List all options
./kolosal-server --help

# Show version
./kolosal-server --version
```

### Complete Parameter List

#### Server Parameters
```bash
--config FILE                    # Configuration file path
--port PORT                      # Server port (default: 8080)
--host HOST                      # Bind host (default: 0.0.0.0)
--max-connections N              # Maximum connections (default: 100)
--worker-threads N               # Worker threads (default: auto)
--request-timeout SECONDS        # Request timeout (default: 30)
--max-request-size BYTES         # Maximum request size (default: 16MB)
```

#### Logging Parameters
```bash
--log-level LEVEL                # Log level: DEBUG|INFO|WARN|ERROR
--log-file FILE                  # Log file path
--enable-access-log              # Enable HTTP access logging
```

#### Authentication Parameters
```bash
--enable-auth                    # Enable authentication
--disable-auth                   # Disable authentication
--require-api-key                # Require API key for all requests
--api-key KEY                    # Add an allowed API key (repeatable)
--api-key-header HEADER          # Header name for API key (default: X-API-Key)
```

#### Rate Limiting Parameters
```bash
--rate-limit N                   # Maximum requests per window (default: 100)
--rate-window SECONDS            # Rate limit window in seconds (default: 60)
--disable-rate-limit             # Disable rate limiting
```

#### CORS Parameters
```bash
--cors-origin ORIGIN             # Add allowed CORS origin (repeatable)
--cors-method METHOD             # Add allowed CORS method (repeatable)
--cors-header HEADER             # Add allowed CORS header (repeatable)
--enable-cors-credentials        # Allow credentials in CORS requests
--cors-max-age SECONDS           # CORS preflight cache duration
```

#### Model Parameters
```bash
--model-path PATH                # Preload model from path
--model-id ID                    # Model identifier for preloaded model
--gpu-layers N                   # GPU layers to offload (default: 100)
--context-size N                 # Context window size (default: 4096)
--main-gpu ID                    # Primary GPU device ID (default: 0)
```

#### Feature Parameters
```bash
--enable-metrics                 # Enable metrics endpoints
--disable-health-check           # Disable health check endpoint
```

#### Help and Information
```bash
--help, -h                       # Show help message
--version, -v                    # Show version information
```

## API Endpoints

### OpenAI-Compatible Endpoints

#### 1. Chat Completions
```http
POST /v1/chat/completions
Content-Type: application/json
```

**Request:**
```json
{
  "model": "gpt-3.5-turbo",
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "Hello!"}
  ],
  "stream": false,
  "temperature": 0.7,
  "top_p": 1.0,
  "max_tokens": 150,
  "seed": null,
  "presence_penalty": 0.0,
  "frequency_penalty": 0.0
}
```

**Response:**
```json
{
  "id": "chatcmpl-123",
  "object": "chat.completion",
  "created": 1677652288,
  "model": "gpt-3.5-turbo",
  "choices": [{
    "index": 0,
    "message": {
      "role": "assistant",
      "content": "Hello! How can I help you today?"
    },
    "finish_reason": "stop"
  }],
  "usage": {
    "prompt_tokens": 12,
    "completion_tokens": 8,
    "total_tokens": 20
  }
}
```

#### 2. Text Completions
```http
POST /v1/completions
Content-Type: application/json
```

**Request:**
```json
{
  "model": "gpt-3.5-turbo",
  "prompt": "The future of AI is",
  "stream": false,
  "temperature": 0.7,
  "max_tokens": 100
}
```

### Engine Management Endpoints

#### 1. Add Engine
```http
POST /engines
Content-Type: application/json
```

**Request:**
```json
{
  "engine_id": "llama-7b",
  "model_path": "/path/to/model.gguf",
  "n_ctx": 4096,
  "n_gpu_layers": 50,
  "main_gpu_id": 0
}
```

**Response:**
```json
{
  "engine_id": "llama-7b",
  "status": "loaded",
  "model_path": "/path/to/model.gguf",
  "parameters": {
    "n_ctx": 4096,
    "n_gpu_layers": 50,
    "main_gpu_id": 0
  }
}
```

#### 2. List Engines
```http
GET /engines
```

**Response:**
```json
{
  "engines": [
    {
      "engine_id": "llama-7b",
      "status": "loaded",
      "model_path": "/path/to/model.gguf"
    }
  ]
}
```

#### 3. Remove Engine
```http
DELETE /engines/{engine_id}
```

#### 4. Engine Status
```http
GET /engines/{engine_id}/status
```

**Response:**
```json
{
  "engine_id": "llama-7b",
  "status": "loaded",
  "model_path": "/path/to/model.gguf",
  "parameters": {
    "n_ctx": 4096,
    "n_gpu_layers": 50,
    "main_gpu_id": 0
  },
  "memory_usage": {
    "model_size_mb": 3584,
    "context_size_mb": 128
  },
  "performance": {
    "last_inference_tps": 42.5,
    "total_requests": 157
  }
}
```

### Health and Monitoring Endpoints

#### 1. Health Check
```http
GET /v1/health
GET /health
```

**Response:**
```json
{
  "status": "healthy",
  "timestamp": 1677652288,
  "version": "1.0.0",
  "engines": {
    "loaded": 2,
    "unloaded": 1,
    "total": 3
  },
  "system": {
    "uptime_seconds": 3600,
    "memory_usage_mb": 8192,
    "cpu_usage_percent": 15.2
  }
}
```

#### 2. Combined Metrics
```http
GET /metrics
GET /v1/metrics
```

**Response:**
```json
{
  "timestamp": "2025-06-17T06:22:02.238Z",
  "system": {
    "cpu": {"usage_percent": 12.26},
    "memory": {
      "total_bytes": 8295342080,
      "used_bytes": 7390986240,
      "utilization_percent": 89.1
    },
    "gpus": [{
      "id": 0,
      "name": "NVIDIA GeForce RTX 4090",
      "utilization_percent": 45.2,
      "memory_utilization_percent": 67.8,
      "temperature_celsius": 72.0,
      "power_usage_watts": 425.0
    }]
  },
  "completion": {
    "total_requests": 1847,
    "successful_requests": 1823,
    "success_rate_percent": 98.7,
    "average_tokens_per_second": 42.3,
    "average_time_to_first_token_ms": 156.7
  }
}
```

#### 3. System Metrics Only
```http
GET /metrics/system
GET /v1/metrics/system
```

#### 4. Completion Metrics Only
```http
GET /metrics/completion
GET /v1/metrics/completion
```

## Authentication & Security

### API Key Authentication

**Headers:**
```http
X-API-Key: sk-your-api-key-here
# OR
Authorization: Bearer sk-your-api-key-here
# OR (custom header)
Custom-Auth-Header: sk-your-api-key-here
```

### Authentication Configuration Endpoints

#### 1. Get Authentication Config
```http
GET /v1/auth/config
```

**Response:**
```json
{
  "rate_limiter": {
    "enabled": true,
    "max_requests": 100,
    "window_seconds": 60
  },
  "cors": {
    "enabled": true,
    "allowed_origins": ["*"],
    "allowed_methods": ["GET", "POST"],
    "allow_credentials": false
  },
  "api_key": {
    "enabled": true,
    "required": false,
    "header_name": "X-API-Key",
    "keys_count": 2
  }
}
```

#### 2. Update Authentication Config
```http
PUT /v1/auth/config
Content-Type: application/json
```

**Request:**
```json
{
  "rate_limiter": {
    "enabled": true,
    "max_requests": 50,
    "window_size": 60
  },
  "cors": {
    "allowed_origins": ["https://example.com"],
    "allow_credentials": true
  },
  "api_key": {
    "enabled": true,
    "required": true,
    "header_name": "Authorization",
    "api_keys": ["sk-1234567890abcdef"]
  }
}
```

#### 3. Get Rate Limiting Statistics
```http
GET /v1/auth/stats
```

**Response:**
```json
{
  "rate_limit_stats": {
    "total_clients": 5,
    "clients": {
      "192.168.1.100": {"request_count": 25},
      "192.168.1.101": {"request_count": 10}
    }
  }
}
```

#### 4. Clear Rate Limit Data
```http
POST /v1/auth/clear
Content-Type: application/json
```

**Clear specific client:**
```json
{
  "client_ip": "192.168.1.100"
}
```

**Clear all clients:**
```json
{
  "clear_all": true
}
```

### Rate Limiting Headers

The server includes rate limiting information in response headers:

```http
X-Rate-Limit-Limit: 100
X-Rate-Limit-Remaining: 95
X-Rate-Limit-Reset: 45
Retry-After: 60  # (on 429 responses)
```

## Monitoring & Metrics

### Available Metrics

#### System Metrics
- **CPU Usage**: Real-time CPU utilization percentage
- **Memory Usage**: RAM usage, total, used, free (bytes and formatted)
- **GPU Metrics**: Per-GPU utilization, memory, temperature, power usage
- **System Summary**: Aggregated resource utilization

#### Completion Metrics
- **Request Statistics**: Total, successful, failed requests
- **Performance Metrics**: Tokens per second (TPS), Time to First Token (TTFT)
- **Per-Engine Metrics**: Engine-specific performance data
- **Success Rates**: Request success/failure percentages

### Metrics Endpoints Summary

| Endpoint | Description | Requirements |
|----------|-------------|--------------|
| `/metrics` | Combined system + completion metrics | `features.metrics: true` |
| `/v1/metrics` | Combined system + completion metrics | `features.metrics: true` |
| `/metrics/system` | System metrics only | `features.metrics: true` |
| `/v1/metrics/system` | System metrics only | `features.metrics: true` |
| `/metrics/completion` | Completion metrics only | `features.metrics: true` |
| `/v1/metrics/completion` | Completion metrics only | `features.metrics: true` |

## Download Management

The server supports downloading models from URLs with progress tracking and cancellation.

### Download Endpoints

#### 1. Download Progress for Specific Model
```http
GET /download-progress/{model_id}
GET /v1/download-progress/{model_id}
```

**Response:**
```json
{
  "model_id": "my-model",
  "status": "downloading",
  "url": "https://huggingface.co/model/file.gguf",
  "local_path": "./models/file.gguf",
  "progress": {
    "downloaded_bytes": 1048576,
    "total_bytes": 10485760,
    "percentage": 10.0,
    "download_speed_bps": 524288
  },
  "timing": {
    "start_time": 1703097600000,
    "elapsed_seconds": 10,
    "estimated_remaining_seconds": 90
  },
  "engine_creation": {
    "engine_id": "my-model",
    "load_immediately": true,
    "main_gpu_id": 0
  }
}
```

#### 2. All Active Downloads
```http
GET /downloads
GET /v1/downloads
```

**Response:**
```json
{
  "active_downloads": [
    {
      "model_id": "model1",
      "status": "downloading",
      "download_type": "startup",
      "url": "https://example.com/model1.gguf",
      "progress": {
        "downloaded_bytes": 1048576,
        "total_bytes": 10485760,
        "percentage": 10.0
      },
      "timing": {
        "start_time": 1703097600000,
        "elapsed_seconds": 10
      }
    }
  ],
  "total_active": 1,
  "timestamp": 1703097610000
}
```

#### 3. Cancel Specific Download
```http
DELETE /downloads/{model_id}
POST /v1/downloads/{model_id}/cancel
```

**Response:**
```json
{
  "success": true,
  "message": "Download cancelled successfully",
  "model_id": "my-model",
  "previous_status": "downloading",
  "download_type": "startup",
  "timestamp": 1703097620000
}
```

#### 4. Cancel All Downloads
```http
DELETE /downloads
POST /v1/downloads/cancel
```

**Response:**
```json
{
  "success": true,
  "message": "Bulk download cancellation completed",
  "summary": {
    "total_downloads": 3,
    "successful_cancellations": 2,
    "failed_cancellations": 1,
    "startup_cancellations": 1,
    "regular_cancellations": 1
  },
  "cancelled_downloads": [
    {
      "model_id": "model1",
      "previous_status": "downloading",
      "download_type": "startup"
    }
  ],
  "failed_cancellations": [
    {
      "model_id": "model2",
      "status": "completed",
      "reason": "Already completed"
    }
  ]
}
```

### Download Status Values

- `"downloading"`: Download in progress
- `"completed"`: Download finished successfully
- `"failed"`: Download failed with error
- `"cancelled"`: Download cancelled by user
- `"creating_engine"`: Creating engine after successful download

## Error Handling

### Standard Error Response Format

All errors follow OpenAI-compatible format:

```json
{
  "error": {
    "message": "Error description",
    "type": "error_type",
    "param": "parameter_name",
    "code": "error_code"
  }
}
```

### Error Types

- `"invalid_request_error"`: Malformed request, missing parameters
- `"authentication_error"`: Invalid or missing API key
- `"rate_limit_error"`: Rate limit exceeded
- `"not_found_error"`: Resource not found
- `"server_error"`: Internal server error
- `"inference_error"`: Model inference failure

### HTTP Status Codes

| Code | Description | Common Causes |
|------|-------------|---------------|
| 200 | OK | Success |
| 201 | Created | Engine loaded successfully |
| 202 | Accepted | Download started |
| 400 | Bad Request | Invalid JSON, missing fields |
| 401 | Unauthorized | Invalid API key |
| 404 | Not Found | Engine/resource not found |
| 405 | Method Not Allowed | Wrong HTTP method |
| 422 | Unprocessable Entity | Invalid URL for downloads |
| 429 | Too Many Requests | Rate limit exceeded |
| 500 | Internal Server Error | Server/inference failure |
| 503 | Service Unavailable | Engine loading, resource exhaustion |

### Rate Limiting Errors

When rate limited, the server returns:

```http
HTTP/1.1 429 Too Many Requests
X-Rate-Limit-Limit: 100
X-Rate-Limit-Remaining: 0
X-Rate-Limit-Reset: 60
Retry-After: 60
Content-Type: application/json

{
  "error": {
    "message": "Rate limit exceeded. Try again in 60 seconds.",
    "type": "rate_limit_error",
    "param": null,
    "code": "rate_limit_exceeded"
  }
}
```

## Best Practices

### Performance Optimization

1. **Worker Threads**: Set `worker_threads` to match CPU cores
2. **Connection Limits**: Adjust `max_connections` based on expected load
3. **GPU Configuration**: Use `n_gpu_layers` and `main_gpu_id` for GPU acceleration
4. **Context Size**: Set appropriate `n_ctx` for your use case
5. **Memory Mapping**: Keep `use_mmap: true` for better memory efficiency

### Security Best Practices

1. **API Keys**: Always use strong, unique API keys in production
2. **Rate Limiting**: Configure appropriate rate limits for your traffic
3. **CORS**: Don't use `"*"` for allowed origins in production
4. **HTTPS**: Use reverse proxy (nginx, Apache) with SSL/TLS
5. **Network Security**: Restrict access using firewall rules

### Monitoring & Alerting

1. **Health Checks**: Regularly check `/v1/health` endpoint
2. **Metrics Monitoring**: Monitor system and completion metrics
3. **Error Rates**: Track error rates and response times
4. **Resource Usage**: Monitor CPU, memory, and GPU utilization
5. **Rate Limit Alerts**: Alert on high rate limit usage

### Configuration Management

1. **Environment-Specific Configs**: Use different configs for dev/staging/prod
2. **Secret Management**: Store API keys securely (env vars, secret managers)
3. **Configuration Validation**: Test configurations before deployment
4. **Backup Configs**: Keep backup copies of working configurations

## Integration Examples

### Basic Integration (Python)

```python
import requests
import json

class KolosalClient:
    def __init__(self, base_url="http://localhost:8080", api_key=None):
        self.base_url = base_url
        self.headers = {"Content-Type": "application/json"}
        if api_key:
            self.headers["X-API-Key"] = api_key
    
    def chat_completion(self, messages, model="gpt-3.5-turbo", **kwargs):
        url = f"{self.base_url}/v1/chat/completions"
        data = {
            "model": model,
            "messages": messages,
            **kwargs
        }
        response = requests.post(url, headers=self.headers, json=data)
        response.raise_for_status()
        return response.json()
    
    def list_engines(self):
        url = f"{self.base_url}/engines"
        response = requests.get(url, headers=self.headers)
        response.raise_for_status()
        return response.json()
    
    def health_check(self):
        url = f"{self.base_url}/v1/health"
        response = requests.get(url, headers=self.headers)
        response.raise_for_status()
        return response.json()

# Usage
client = KolosalClient(api_key="sk-your-key")

# Chat completion
response = client.chat_completion([
    {"role": "user", "content": "Hello!"}
])
print(response["choices"][0]["message"]["content"])

# Health check
health = client.health_check()
print(f"Server status: {health['status']}")
```

### Streaming Integration (Python)

```python
import requests
import json

def stream_chat_completion(messages, model="gpt-3.5-turbo", api_key=None):
    url = "http://localhost:8080/v1/chat/completions"
    headers = {
        "Content-Type": "application/json",
        "Accept": "text/event-stream"
    }
    if api_key:
        headers["X-API-Key"] = api_key
    
    data = {
        "model": model,
        "messages": messages,
        "stream": True
    }
    
    response = requests.post(url, headers=headers, json=data, stream=True)
    response.raise_for_status()
    
    for line in response.iter_lines():
        if line:
            line = line.decode('utf-8')
            if line.startswith('data: '):
                data = line[6:]  # Remove 'data: ' prefix
                if data.strip() == '[DONE]':
                    break
                try:
                    chunk = json.loads(data)
                    if chunk['choices'][0]['delta'].get('content'):
                        yield chunk['choices'][0]['delta']['content']
                except json.JSONDecodeError:
                    continue

# Usage
for chunk in stream_chat_completion([
    {"role": "user", "content": "Tell me a story"}
]):
    print(chunk, end='', flush=True)
```

### Node.js Integration

```javascript
const axios = require('axios');

class KolosalClient {
    constructor(baseUrl = 'http://localhost:8080', apiKey = null) {
        this.baseUrl = baseUrl;
        this.headers = { 'Content-Type': 'application/json' };
        if (apiKey) {
            this.headers['X-API-Key'] = apiKey;
        }
    }

    async chatCompletion(messages, model = 'gpt-3.5-turbo', options = {}) {
        const url = `${this.baseUrl}/v1/chat/completions`;
        const data = { model, messages, ...options };
        
        try {
            const response = await axios.post(url, data, { headers: this.headers });
            return response.data;
        } catch (error) {
            if (error.response) {
                throw new Error(`API Error ${error.response.status}: ${error.response.data.error?.message}`);
            }
            throw error;
        }
    }

    async addEngine(engineId, modelPath, options = {}) {
        const url = `${this.baseUrl}/engines`;
        const data = { engine_id: engineId, model_path: modelPath, ...options };
        
        const response = await axios.post(url, data, { headers: this.headers });
        return response.data;
    }

    async getMetrics() {
        const url = `${this.baseUrl}/metrics`;
        const response = await axios.get(url, { headers: this.headers });
        return response.data;
    }
}

// Usage
const client = new KolosalClient('http://localhost:8080', 'sk-your-key');

async function example() {
    try {
        // Chat completion
        const response = await client.chatCompletion([
            { role: 'user', content: 'Hello!' }
        ]);
        console.log(response.choices[0].message.content);

        // Add engine
        await client.addEngine('my-model', '/path/to/model.gguf', {
            n_ctx: 4096,
            n_gpu_layers: 50
        });

        // Get metrics
        const metrics = await client.getMetrics();
        console.log(`CPU: ${metrics.system.cpu.usage_percent}%`);
    } catch (error) {
        console.error('Error:', error.message);
    }
}

example();
```

### PowerShell Integration

```powershell
# KolosalServer PowerShell Functions

function Invoke-KolosalChatCompletion {
    param(
        [string]$BaseUrl = "http://localhost:8080",
        [string]$ApiKey,
        [array]$Messages,
        [string]$Model = "gpt-3.5-turbo",
        [int]$MaxTokens = 150
    )
    
    $headers = @{
        "Content-Type" = "application/json"
    }
    if ($ApiKey) {
        $headers["X-API-Key"] = $ApiKey
    }
    
    $body = @{
        model = $Model
        messages = $Messages
        max_tokens = $MaxTokens
    } | ConvertTo-Json -Depth 3
    
    try {
        $response = Invoke-RestMethod -Uri "$BaseUrl/v1/chat/completions" `
                                      -Method POST `
                                      -Headers $headers `
                                      -Body $body
        return $response.choices[0].message.content
    }
    catch {
        Write-Error "API Error: $($_.Exception.Message)"
        if ($_.Exception.Response) {
            $errorDetails = $_.Exception.Response | ConvertFrom-Json
            Write-Error "Details: $($errorDetails.error.message)"
        }
    }
}

function Get-KolosalHealth {
    param(
        [string]$BaseUrl = "http://localhost:8080",
        [string]$ApiKey
    )
    
    $headers = @{}
    if ($ApiKey) {
        $headers["X-API-Key"] = $ApiKey
    }
    
    Invoke-RestMethod -Uri "$BaseUrl/v1/health" -Method GET -Headers $headers
}

# Usage
$apiKey = "sk-your-key"
$response = Invoke-KolosalChatCompletion -ApiKey $apiKey -Messages @(
    @{ role = "user"; content = "Hello!" }
)
Write-Output $response

$health = Get-KolosalHealth -ApiKey $apiKey
Write-Output "Server Status: $($health.status)"
```

---

This guide provides comprehensive information for integrating with the Kolosal Server. For additional details, consult the server's documentation directory or use the `--help` parameter for the latest command-line options.
