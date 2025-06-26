# Kolosal Server Configuration

The Kolosal Server supports configuration through both JSON and YAML files. Configuration files allow you to set up server parameters, model loading, authentication, logging, and various features.

## Configuration File Formats

The server accepts configuration in two formats:

- **JSON**: `config.json`
- **YAML**: `config.yaml`

The server will automatically detect and load the configuration file from the working directory. If both formats are present, JSON takes precedence.

## Configuration Structure

### Complete Configuration Example

#### JSON Format (`config.json`)

```json
{
    "server": {
        "port": "8080",
        "host": "0.0.0.0",
        "max_connections": 100,
        "worker_threads": 0,
        "request_timeout": 30,
        "max_request_size": 16777216,
        "idle_timeout": 300
    },
    "logging": {
        "level": "INFO",
        "file": "",
        "access_log": false
    },
    "auth": {
        "enabled": true,
        "require_api_key": false,
        "api_key_header": "X-API-Key",
        "api_keys": [],
        "rate_limit": {
            "enabled": true,
            "max_requests": 100,
            "window_size": 60
        },
        "cors": {
            "enabled": true,
            "allow_credentials": false,
            "max_age": 86400,
            "allowed_origins": ["*"],
            "allowed_methods": [
                "GET", "POST", "PUT", "DELETE", 
                "OPTIONS", "HEAD", "PATCH"
            ],
            "allowed_headers": [
                "Content-Type", "Authorization", 
                "X-Requested-With", "Accept", "Origin"
            ]
        }
    },
    "models": [
        {
            "id": "qwen3-0.6b",
            "path": "https://huggingface.co/kolosal/qwen3-0.6b/resolve/main/Qwen3-0.6B-UD-Q4_K_XL.gguf",
            "load_at_startup": true,
            "main_gpu_id": 0,
            "preload_context": false,
            "load_params": {
                "n_ctx": 2048,
                "n_keep": 1024,
                "use_mmap": true,
                "use_mlock": false,
                "n_parallel": 1,
                "cont_batching": true
            }
        },
        {
            "id": "gpt-3.5-turbo",
            "path": "./models/gpt-3.5-turbo.gguf",
            "load_at_startup": false,
            "main_gpu_id": 0,
            "preload_context": false,
            "load_params": {
                "n_ctx": 4096,
                "n_keep": 2048,
                "use_mmap": true,
                "use_mlock": false,
                "n_parallel": 1,
                "cont_batching": true
            }
        }
    ],
    "features": {
        "health_check": true,
        "metrics": true
    }
}
```

#### YAML Format (`config.yaml`)

```yaml
server:
  port: "8080"
  host: "0.0.0.0"
  max_connections: 100
  worker_threads: 0
  request_timeout: 30
  max_request_size: 16777216
  idle_timeout: 300

logging:
  level: "INFO"
  file: ""
  access_log: false

auth:
  enabled: true
  require_api_key: false
  api_key_header: "X-API-Key"
  api_keys: []
  rate_limit:
    enabled: true
    max_requests: 100
    window_size: 60
  cors:
    enabled: true
    allow_credentials: false
    max_age: 86400
    allowed_origins:
      - "*"
    allowed_methods:
      - "GET"
      - "POST"
      - "PUT"
      - "DELETE"
      - "OPTIONS"
      - "HEAD"
      - "PATCH"
    allowed_headers:
      - "Content-Type"
      - "Authorization"
      - "X-Requested-With"
      - "Accept"
      - "Origin"

models:
  - id: "qwen3-0.6b"
    path: "https://huggingface.co/kolosal/qwen3-0.6b/resolve/main/Qwen3-0.6B-UD-Q4_K_XL.gguf"
    load_at_startup: true
    main_gpu_id: 0
    preload_context: false
    load_params:
      n_ctx: 2048
      n_keep: 1024
      use_mmap: true
      use_mlock: false
      n_parallel: 1
      cont_batching: true
  - id: "gpt-3.5-turbo"
    path: "./models/gpt-3.5-turbo.gguf"
    load_at_startup: false
    main_gpu_id: 0
    preload_context: false
    load_params:
      n_ctx: 4096
      n_keep: 2048
      use_mmap: true
      use_mlock: false
      n_parallel: 1
      cont_batching: true

features:
  health_check: true
  metrics: true
```

## Configuration Sections

### Server Configuration

The `server` section configures basic server parameters:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `port` | string | `"8080"` | Port number for the HTTP server |
| `host` | string | `"0.0.0.0"` | Host address to bind to (0.0.0.0 for all interfaces) |
| `max_connections` | integer | `100` | Maximum concurrent connections |
| `worker_threads` | integer | `0` | Number of worker threads (0 = auto-detect CPU cores) |
| `request_timeout` | integer | `30` | Request timeout in seconds |
| `max_request_size` | integer | `16777216` | Maximum request size in bytes (16MB) |
| `idle_timeout` | integer | `300` | Connection idle timeout in seconds |

**Example:**
```yaml
server:
  port: "8080"
  host: "localhost"  # Only accept local connections
  max_connections: 50
  worker_threads: 4  # Use 4 worker threads
  request_timeout: 60
```

### Logging Configuration

The `logging` section controls server logging behavior:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `level` | string | `"INFO"` | Log level: `DEBUG`, `INFO`, `WARNING`, `ERROR` |
| `file` | string | `""` | Log file path (empty = console only) |
| `access_log` | boolean | `false` | Enable access logging for HTTP requests |
| `quiet_mode` | boolean | `false` | Suppress routine operational messages (connections, requests) |
| `show_request_details` | boolean | `true` | Show detailed request processing information |

**Log Levels:**
- `ERROR`: Only show errors and critical issues
- `WARNING`: Show errors and warnings  
- `INFO`: Show general operational information (default)
- `DEBUG`: Show detailed debugging information

**Example:**
```yaml
logging:
  level: "WARNING"      # Reduced verbosity
  file: "./logs/server.log"
  access_log: false
  quiet_mode: true      # Hide routine connection/request logs
  show_request_details: false  # Hide detailed request processing
```

**Quiet Mode:**
When `quiet_mode` is enabled, routine operational messages are suppressed:
- New client connections
- Request processing messages  
- Successful request completions
- Authentication middleware details

**Request Details:**
When `show_request_details` is disabled, detailed request processing information is hidden:
- Thread IDs and connection details
- Content-Length headers
- Authentication middleware results
- CORS preflight handling

### Authentication Configuration

The `auth` section configures authentication and security features:

#### Basic Authentication

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | boolean | `true` | Enable authentication system |
| `require_api_key` | boolean | `false` | Require API key for all requests |
| `api_key_header` | string | `"X-API-Key"` | HTTP header name for API keys |
| `api_keys` | array | `[]` | List of valid API keys |

#### Rate Limiting

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `rate_limit.enabled` | boolean | `true` | Enable rate limiting |
| `rate_limit.max_requests` | integer | `100` | Maximum requests per window |
| `rate_limit.window_size` | integer | `60` | Rate limit window size in seconds |

#### CORS Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `cors.enabled` | boolean | `true` | Enable CORS headers |
| `cors.allow_credentials` | boolean | `false` | Allow credentials in CORS requests |
| `cors.max_age` | integer | `86400` | CORS preflight cache duration (seconds) |
| `cors.allowed_origins` | array | `["*"]` | Allowed origins for CORS |
| `cors.allowed_methods` | array | Standard HTTP methods | Allowed HTTP methods |
| `cors.allowed_headers` | array | Standard headers | Allowed HTTP headers |

**Example with API Key Authentication:**
```yaml
auth:
  enabled: true
  require_api_key: true
  api_key_header: "Authorization"
  api_keys:
    - "sk-1234567890abcdef"
    - "sk-fedcba0987654321"
  rate_limit:
    enabled: true
    max_requests: 1000
    window_size: 3600  # 1 hour
  cors:
    enabled: true
    allowed_origins:
      - "https://mydomain.com"
      - "https://app.mydomain.com"
```

### Model Configuration

The `models` section defines models to be loaded at startup:

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `id` | string | ✅ | Unique identifier for the model |
| `path` | string | ✅ | Local file path or URL to model file |
| `load_at_startup` | boolean | ❌ | Load model immediately on server start |
| `main_gpu_id` | integer | ❌ | Primary GPU ID for model inference |
| `preload_context` | boolean | ❌ | Preload context for faster initial inference |
| `load_params` | object | ❌ | Advanced model loading parameters |

#### Model Load Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `n_ctx` | integer | `2048` | Context window size (max tokens) |
| `n_keep` | integer | `1024` | Number of tokens to keep in context |
| `use_mmap` | boolean | `true` | Use memory mapping for model loading |
| `use_mlock` | boolean | `false` | Lock model in memory (prevents swapping) |
| `n_parallel` | integer | `1` | Number of parallel sequences |
| `cont_batching` | boolean | `true` | Enable continuous batching |

#### Model Path Examples

**Local File:**
```yaml
models:
  - id: "my-local-model"
    path: "./models/my-model.gguf"
    load_at_startup: true
```

**Hugging Face URL:**
```yaml
models:
  - id: "remote-model"
    path: "https://huggingface.co/user/model/resolve/main/model.gguf"
    load_at_startup: false  # Download on first use
```

**GPU Configuration:**
```yaml
models:
  - id: "gpu-model"
    path: "./models/large-model.gguf"
    load_at_startup: true
    main_gpu_id: 0
    load_params:
      n_ctx: 8192
      n_gpu_layers: 50  # Offload layers to GPU
      use_mlock: true   # Lock in GPU memory
```

### Features Configuration

The `features` section controls optional server features:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `health_check` | boolean | `true` | Enable `/health` endpoint |
| `metrics` | boolean | `false` | Enable system metrics endpoints |

**Example:**
```yaml
features:
  health_check: true
  metrics: true  # Enable /metrics and /completion-metrics
```

## Configuration Loading

### Automatic Detection

The server automatically searches for configuration files in this order:

1. `config.json`
2. `config.yaml`
3. If no config file is found, uses default values

### Manual Configuration File

You can specify a custom configuration file:

```bash
# JSON configuration
./kolosal-server.exe --config=my-config.json

# YAML configuration  
./kolosal-server.exe --config=my-config.yaml
```

### Environment Variables

Some configuration options can be overridden with environment variables:

| Environment Variable | Configuration Path | Description |
|---------------------|-------------------|-------------|
| `KOLOSAL_PORT` | `server.port` | Server port |
| `KOLOSAL_HOST` | `server.host` | Server host |
| `KOLOSAL_LOG_LEVEL` | `logging.level` | Log level |

**Example:**
```bash
set KOLOSAL_PORT=9090
set KOLOSAL_LOG_LEVEL=DEBUG
./kolosal-server.exe
```

## Configuration Examples

### Minimal Configuration

```yaml
server:
  port: "8080"

models:
  - id: "default"
    path: "./models/model.gguf"
    load_at_startup: true
```

### Production Configuration

```yaml
server:
  port: "8080"
  host: "0.0.0.0"
  max_connections: 500
  worker_threads: 8
  request_timeout: 120

logging:
  level: "INFO"
  file: "./logs/kolosal-server.log"
  access_log: true

auth:
  enabled: true
  require_api_key: true
  api_key_header: "Authorization"
  api_keys:
    - "sk-production-key-1"
    - "sk-production-key-2"
  rate_limit:
    enabled: true
    max_requests: 10000
    window_size: 3600
  cors:
    enabled: true
    allowed_origins:
      - "https://yourdomain.com"

models:
  - id: "gpt-3.5-turbo"
    path: "./models/gpt-3.5-turbo.gguf"
    load_at_startup: true
    main_gpu_id: 0
    load_params:
      n_ctx: 4096
      n_keep: 2048
      use_mmap: true
      use_mlock: true
      n_parallel: 4
      cont_batching: true

features:
  health_check: true
  metrics: true
```

### Development Configuration

```yaml
server:
  port: "8080"
  host: "localhost"
  max_connections: 10

logging:
  level: "DEBUG"
  access_log: true

auth:
  enabled: false  # Disable auth for development

models:
  - id: "test-model"
    path: "./models/test-model.gguf"
    load_at_startup: false  # Lazy loading for faster startup
    load_params:
      n_ctx: 1024

features:
  health_check: true
  metrics: true
```

## Configuration Validation

The server validates configuration on startup and will report errors for:

- Invalid JSON/YAML syntax
- Missing required fields
- Invalid parameter values
- Conflicting settings

**Example Error Messages:**
```
[ERROR] Configuration validation failed:
- server.port: must be a valid port number (1-65535)
- models[0].id: field is required
- auth.api_keys: must be non-empty when require_api_key is true
```

## Best Practices

### Security

1. **Use API Keys**: Enable `require_api_key: true` for production
2. **Restrict CORS**: Don't use `"*"` for allowed origins in production
3. **Set Rate Limits**: Configure appropriate rate limiting for your use case
4. **Use HTTPS**: Configure reverse proxy (nginx, Apache) with SSL/TLS

### Performance

1. **Worker Threads**: Set `worker_threads` to match your CPU cores
2. **Connection Limits**: Adjust `max_connections` based on expected load
3. **GPU Configuration**: Use `main_gpu_id` and `n_gpu_layers` for GPU acceleration
4. **Memory Mapping**: Keep `use_mmap: true` for better memory efficiency

### Model Management

1. **Lazy Loading**: Use `load_at_startup: false` for large models to improve startup time
2. **Context Size**: Set `n_ctx` based on your typical input lengths
3. **GPU Layers**: Experiment with `n_gpu_layers` to find optimal GPU/CPU split
4. **Continuous Batching**: Enable `cont_batching: true` for better throughput

### Monitoring

1. **Enable Metrics**: Set `features.metrics: true` to monitor performance
2. **Access Logs**: Enable `logging.access_log: true` for request tracking
3. **Log Files**: Configure `logging.file` for persistent logging
4. **Health Checks**: Keep `features.health_check: true` enabled

## Troubleshooting

### Common Configuration Issues

1. **Port Already in Use**
   ```
   [ERROR] Failed to bind to port 8080: Address already in use
   ```
   *Solution*: Change the port number or stop the conflicting service

2. **Model File Not Found**
   ```
   [ERROR] Failed to load model: file not found at ./models/model.gguf
   ```
   *Solution*: Verify the model path or download the model file

3. **Invalid JSON/YAML**
   ```
   [ERROR] Failed to parse configuration: invalid JSON at line 15
   ```
   *Solution*: Use a JSON/YAML validator to check syntax

4. **GPU Not Available**
   ```
   [WARN] GPU 0 not available, falling back to CPU
   ```
   *Solution*: Check CUDA installation or set `main_gpu_id: -1` for CPU-only

### Configuration Testing

Test your configuration before deploying:

```bash
# Validate configuration without starting server
./kolosal-server.exe --config=config.yaml --validate-only

# Start server with verbose logging
./kolosal-server.exe --config=config.yaml --log-level=DEBUG
```
