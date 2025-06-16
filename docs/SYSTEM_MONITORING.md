# System and Completion Monitoring API

The Kolosal Server includes comprehensive monitoring capabilities that provide real-time information about both system resources and completion performance.

## Overview

The monitoring features provide:

### System Monitoring
- **CPU Usage**: Real-time CPU utilization percentage
- **RAM Usage**: Memory usage statistics including total, used, free, and utilization percentage
- **GPU Usage**: GPU utilization and VRAM usage for each GPU (when available)
- **VRAM Usage**: Video memory statistics for each GPU (when available)

### Completion Monitoring
- **Request Tracking**: Monitor completion requests across all models/engines
- **Performance Metrics**: Track TPS (tokens per second), TTFT (time to first token), and RPS (requests per second)
- **Success Rates**: Monitor completion success and failure rates
- **Token Statistics**: Track input/output token counts and processing times

## API Endpoints

### System Metrics

The system metrics are available through multiple endpoint paths:

- `GET /metrics`
- `GET /v1/metrics` 
- `GET /system/metrics`

### Completion Metrics

The completion metrics are available through multiple endpoint paths:

- `GET /completion-metrics`
- `GET /v1/completion-metrics`
- `GET /completion/metrics`

All endpoints return the same JSON response format for their respective metric types.

## Response Formats

### System Metrics Response

```json
{
  "timestamp": "2025-06-16T06:22:02.238Z",
  "cpu": {
    "usage_percent": 12.26
  },
  "memory": {
    "total_bytes": 8295342080,
    "used_bytes": 7390986240,
    "free_bytes": 904355840,
    "utilization_percent": 89.1,
    "total_formatted": "7.73 GB",
    "used_formatted": "6.88 GB",
    "free_formatted": "862.46 MB"
  },
  "gpus": [
    {
      "id": 0,
      "name": "NVIDIA GeForce RTX 4090",
      "utilization": {
        "gpu_percent": 85.5,
        "memory_percent": 67.8
      },
      "memory": {
        "total_bytes": 25769803776,
        "used_bytes": 17476714496,
        "free_bytes": 8293089280,
        "total_formatted": "24.00 GB",
        "used_formatted": "16.28 GB",
        "free_formatted": "7.72 GB"
      },
      "temperature_celsius": 72.0,
      "power_usage_watts": 350.5,
      "driver_version": "555.99"
    }
  ],
  "summary": {
    "cpu_usage_percent": 12.26,
    "ram_utilization_percent": 89.1,
    "gpu_count": 1,
    "average_gpu_utilization_percent": 85.5,
    "average_vram_utilization_percent": 67.8
  },
  "gpu_monitoring_available": true,
  "metadata": {
    "version": "1.0",
    "server": "kolosal-server",
    "monitoring_capabilities": {
      "cpu": true,
      "memory": true,
      "gpu": true
    }
  }
}
```

### Completion Metrics Response

```json
{
  "completion_metrics": {
    "summary": {
      "total_requests": 150,
      "completed_requests": 147,
      "failed_requests": 3,
      "success_rate_percent": 98.0,
      "total_input_tokens": 1250,
      "total_output_tokens": 3500,
      "avg_turnaround_time_ms": 1450.5,
      "avg_tps": 15.2,
      "avg_output_tps": 9.8,
      "avg_ttft_ms": 325.7,
      "avg_rps": 0.92
    },
    "per_engine": [
      {
        "model_name": "my-model-7b",
        "engine_id": "default",
        "total_requests": 100,
        "completed_requests": 98,
        "failed_requests": 2,
        "total_input_tokens": 800,
        "total_output_tokens": 2400,
        "tps": 16.5,
        "output_tps": 10.2,
        "avg_ttft": 280.3,
        "rps": 1.05,
        "last_updated": "2025-06-16T17:04:12.123Z"
      },
      {
        "model_name": "my-model-13b",
        "engine_id": "default", 
        "total_requests": 50,
        "completed_requests": 49,
        "failed_requests": 1,
        "total_input_tokens": 450,
        "total_output_tokens": 1100,
        "tps": 12.8,
        "output_tps": 8.9,
        "avg_ttft": 420.1,
        "rps": 0.75,
        "last_updated": "2025-06-16T17:04:10.890Z"
      }
    ],
    "timestamp": "2025-06-16T17:04:12.123Z"
  }
}
```

## Field Descriptions

### System Metrics

#### CPU Metrics

### CPU Metrics
- `cpu.usage_percent`: Current CPU utilization as a percentage (0-100)

### Memory Metrics
- `memory.total_bytes`: Total system RAM in bytes
- `memory.used_bytes`: Currently used RAM in bytes
- `memory.free_bytes`: Available RAM in bytes
- `memory.utilization_percent`: RAM usage percentage (0-100)
- `memory.*_formatted`: Human-readable formatted sizes

### GPU Metrics (when available)
- `gpus[].id`: GPU device ID
- `gpus[].name`: GPU device name
- `gpus[].utilization.gpu_percent`: GPU core utilization percentage (0-100)
- `gpus[].utilization.memory_percent`: VRAM utilization percentage (0-100)
- `gpus[].memory.*`: VRAM usage statistics in bytes and formatted
- `gpus[].temperature_celsius`: GPU temperature in Celsius
- `gpus[].power_usage_watts`: Current power consumption in watts
- `gpus[].driver_version`: GPU driver version (shown only for first GPU)

### Summary Statistics
- `summary.cpu_usage_percent`: Current CPU usage
- `summary.ram_utilization_percent`: Current RAM usage
- `summary.gpu_count`: Number of detected GPUs
- `summary.average_gpu_utilization_percent`: Average GPU utilization across all GPUs
- `summary.average_vram_utilization_percent`: Average VRAM utilization across all GPUs

### Metadata
- `gpu_monitoring_available`: Whether GPU monitoring is available
- `metadata.monitoring_capabilities`: Available monitoring features
- `metadata.version`: API version
- `metadata.server`: Server name
- `timestamp`: ISO 8601 timestamp of when metrics were collected

### Completion Metrics

#### Summary Statistics
- `summary.total_requests`: Total number of completion requests received
- `summary.completed_requests`: Number of successfully completed requests
- `summary.failed_requests`: Number of requests that failed with errors
- `summary.success_rate_percent`: Success rate as a percentage (0-100)
- `summary.total_input_tokens`: Total input tokens processed across all requests
- `summary.total_output_tokens`: Total output tokens generated across all requests
- `summary.avg_turnaround_time_ms`: Average time from request start to completion (milliseconds)
- `summary.avg_tps`: Average tokens per second (input + output tokens / turnaround time)
- `summary.avg_output_tps`: Average output tokens per second (output tokens / generation time)
- `summary.avg_ttft_ms`: Average time to first token in milliseconds
- `summary.avg_rps`: Average requests per second (completed requests / total time)

#### Per-Engine Metrics
- `per_engine[].model_name`: Name/path of the model being used
- `per_engine[].engine_id`: Identifier of the engine instance
- `per_engine[].total_requests`: Total requests for this specific engine
- `per_engine[].completed_requests`: Completed requests for this engine
- `per_engine[].failed_requests`: Failed requests for this engine
- `per_engine[].total_input_tokens`: Input tokens processed by this engine
- `per_engine[].total_output_tokens`: Output tokens generated by this engine
- `per_engine[].tps`: Tokens per second for this engine
- `per_engine[].output_tps`: Output tokens per second for this engine
- `per_engine[].avg_ttft`: Average time to first token for this engine (milliseconds)
- `per_engine[].rps`: Requests per second for this engine
- `per_engine[].last_updated`: ISO 8601 timestamp of last activity for this engine

#### Timing and Performance
- All timing metrics are in milliseconds
- TPS (Tokens Per Second) includes both input and output tokens
- Output TPS measures only the token generation speed
- TTFT (Time To First Token) measures inference startup latency
- RPS (Requests Per Second) measures overall throughput

## Monitoring Features

### System Monitoring

The server supports GPU monitoring through NVIDIA Management Library (NVML) for NVIDIA GPUs. When NVML is available:

- Multiple GPUs are supported
- Real-time utilization monitoring
- VRAM usage tracking
- Temperature monitoring
- Power consumption tracking
- Driver version information

When GPU monitoring is not available (no NVIDIA drivers or NVML not compiled):
- `gpu_monitoring_available` will be `false`
- `gpus` array will be empty
- GPU-related summary fields will be `null`

### Completion Monitoring

The completion monitoring system tracks all inference requests in real-time:

**Automatic Tracking**: All completion requests (both `/v1/completions` and `/v1/chat/completions`) are automatically monitored

**Singleton Architecture**: Uses a shared singleton monitor instance to ensure consistent metrics across all routes

**Performance Metrics**:
- Request lifecycle tracking from start to completion
- Token counting for both input and output
- Timing measurements including TTFT (Time to First Token)
- Success/failure rate monitoring
- Per-engine performance breakdown

**Real-time Updates**: Metrics are updated in real-time as requests are processed

**Thread-Safe**: All monitoring operations are thread-safe for concurrent request handling

## Error Handling

### System Monitoring Errors

If system monitoring fails, the API returns an error response:

```json
{
  "error": {
    "code": "internal_error",
    "message": "Failed to retrieve system metrics",
    "details": "Error details..."
  },
  "timestamp": "2025-06-16T06:22:02.238Z"
}
```

### Completion Monitoring Errors

If completion metrics retrieval fails, the API returns an error response:

```json
{
  "error": {
    "message": "Internal server error",
    "details": "Failed to aggregate completion metrics"
  }
}
}
```

## Usage Examples

### Basic CPU and RAM Monitoring
```bash
curl http://localhost:8080/metrics
```

### Monitoring with jq formatting
```bash
curl -s http://localhost:8080/metrics | jq '.summary'
```

### PowerShell Example
```powershell
$response = Invoke-RestMethod -Uri "http://localhost:8080/metrics"
Write-Host "CPU Usage: $($response.summary.cpu_usage_percent)%"
Write-Host "RAM Usage: $($response.summary.ram_utilization_percent)%"
```

## Implementation Details

The system monitoring is implemented through:
- **Windows**: Performance Data Helper (PDH) API for CPU, WMI for memory, NVML for GPU
- **Linux**: `/proc/stat` for CPU, `/proc/meminfo` for memory, NVML for GPU
- **Cross-platform**: NVML for NVIDIA GPU monitoring

The monitoring has minimal performance impact and provides real-time data suitable for dashboards and monitoring systems.
