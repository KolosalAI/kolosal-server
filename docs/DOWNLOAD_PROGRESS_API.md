# Download Progress API

This document describes the new download progress endpoints added to the Kolosal Server.

## Overview

The server now supports asynchronous model downloads with progress tracking. When you add an engine with a URL as the model path, the download happens in the background and you can track its progress.

## Endpoints

### 1. Download Progress for Specific Model

**GET** `/download-progress/{model-or-engine-id}` or `/v1/download-progress/{model-or-engine-id}`

Returns the download progress for a specific model/engine ID.

#### Response Format

```json
{
  "model_id": "my-model",
  "status": "downloading", // "downloading", "completed", "failed", "cancelled"
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
  }
}
```

#### Status Codes

- **200**: Success - returns download progress
- **404**: No download found for the specified model ID
- **400**: Invalid model ID format in URL
- **500**: Server error

### 2. All Active Downloads

**GET** `/downloads` or `/v1/downloads`

Returns the status of all currently active downloads.

#### Response Format

```json
{
  "active_downloads": [
    {
      "model_id": "model1",
      "status": "downloading",
      "url": "https://example.com/model1.gguf",
      "local_path": "./models/model1.gguf",
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
      }
    }
  ],
  "total_active": 1,
  "timestamp": 1703097610000
}
```

## How It Works

### 1. Adding an Engine with URL

When you make a POST request to `/engines` with a URL as the `model_path`:

```json
{
  "engine_id": "my-model",
  "model_path": "https://huggingface.co/microsoft/DialoGPT-medium/resolve/main/pytorch_model.bin",
  "load_immediately": true
}
```

If the URL is valid and the file doesn't exist locally:
- The server responds with HTTP 202 (Accepted)
- Download starts in the background
- Engine creation is deferred until download completes

#### Response for Started Download

```json
{
  "message": "Model download started successfully. Engine will be created once download completes.",
  "engine_id": "my-model",
  "download_status": "started",
  "download_url": "https://huggingface.co/model/file.gguf",
  "local_path": "./models/file.gguf",
  "progress_endpoint": "/download-progress/my-model",
  "note": "Check download progress using the progress_endpoint. Engine creation will be deferred until download completes."
}
```

### 2. Tracking Progress

Use the provided `progress_endpoint` to track download progress:

```bash
curl -X GET http://localhost:8080/download-progress/my-model
```

### 3. File Already Exists

If the model file already exists locally, the engine is created immediately without starting a download.

## Example Usage

### 1. Start a download

```bash
curl -X POST http://localhost:8080/engines \
  -H "Content-Type: application/json" \
  -d '{
    "engine_id": "qwen-model",
    "model_path": "https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/qwen2.5-0.5b-instruct-q4_k_m.gguf",
    "load_immediately": true
  }'
```

### 2. Check progress

```bash
curl -X GET http://localhost:8080/download-progress/qwen-model
```

### 3. List all downloads

```bash
curl -X GET http://localhost:8080/downloads
```

## Technical Details

- Downloads are handled by the `DownloadManager` singleton
- Each download runs in its own thread using `std::async`
- Progress is tracked in real-time with callbacks
- Download records are cleaned up automatically after completion
- The system supports concurrent downloads for multiple models
- Download speed and estimated completion time are calculated automatically

## Error Handling

- Invalid URLs return HTTP 422 with detailed error messages
- Network errors during download are captured and reported
- Partial downloads are cleaned up automatically on failure
- Download records are preserved for a short time after completion for status checking
