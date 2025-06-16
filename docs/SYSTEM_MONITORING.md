# System Monitoring API

The Kolosal Server includes comprehensive system monitoring capabilities that provide real-time information about CPU usage, RAM usage, and GPU/VRAM usage.

## Overview

The system monitoring feature provides:
- **CPU Usage**: Real-time CPU utilization percentage
- **RAM Usage**: Memory usage statistics including total, used, free, and utilization percentage
- **GPU Usage**: GPU utilization and VRAM usage for each GPU (when available)
- **VRAM Usage**: Video memory statistics for each GPU (when available)

## API Endpoints

The system metrics are available through multiple endpoint paths:

- `GET /metrics`
- `GET /v1/metrics` 
- `GET /system/metrics`

All endpoints return the same JSON response format.

## Response Format

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

## Field Descriptions

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

## GPU Support

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

## Error Handling

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
