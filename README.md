# Kolosal Server

Kolosal Server is a high-performance HTTP server designed for AI inference. It provides a flexible and extensible platform for serving machine learning models, with a focus on performance and ease of use.

## Features

*   **High-performance HTTP server:** Built for speed and efficiency, Kolosal Server can handle a large number of concurrent requests.
*   **AI Inference:** Optimized for AI inference tasks, supporting various models and frameworks.
*   **OpenAI Compatible API:** Offers an OpenAI compatible API for chat completions and text completions, making it easy to integrate with existing tools and workflows.
*   **Extensible:** Designed to be easily extensible, allowing you to add support for new models, frameworks, and features.
*   **Cross-platform:** Runs on Windows, Linux, and macOS.
*   **GPU Acceleration:** Supports CUDA and Vulkan for GPU acceleration, significantly speeding up inference tasks.
*   **MPI Support:** Enables distributed inference across multiple nodes using MPI.

## Endpoints

*   `GET  /health`: Health status of the server.
*   `GET  /models`: List available models.
*   `POST /v1/chat/completions`: Chat completions (OpenAI compatible).
*   `POST /v1/completions`: Text completions (OpenAI compatible).
*   `GET  /engines`: List engines.
*   `POST /engines`: Add new engine.
*   `GET  /engines/{id}/status`: Engine status.
*   `DELETE /engines/{id}`: Remove engine.

## Endpoint Request Examples

Below are some examples of how to interact with the server endpoints using `curl`.

### Health Check

To check the health status of the server:

```bash
curl http://localhost:8080/health
```

**Example Response:**

```json
{
  "status": "OK"
}
```

### List Available Models

To get a list of available models:

```bash
curl http://localhost:8080/models
```

**Example Response:**

```json
{
  "models": [
    "model1_name",
    "model2_name"
  ]
}
```

### Chat Completions (OpenAI Compatible)

To get chat completions:

```bash
curl -X POST http://localhost:8080/v1/chat/completions \\
  -H "Content-Type: application/json" \\
  -d '{
    "model": "your_model_name",
    "messages": [
      {"role": "user", "content": "Hello!"}
    ]
  }'
```

### Text Completions (OpenAI Compatible)

To get text completions:

```bash
curl -X POST http://localhost:8080/v1/completions \\
  -H "Content-Type: application/json" \\
  -d '{
    "model": "your_model_name",
    "prompt": "Once upon a time",
    "max_tokens": 50
  }'
```

### List Engines

To get a list of currently running engines:

```bash
curl http://localhost:8080/engines
```

**Example Response:**

```json
{
  "engines": [
    {"id": "engine_id_1", "model": "model_name_1", "status": "running"},
    {"id": "engine_id_2", "model": "model_name_2", "status": "loading"}
  ]
}
```

### Add New Engine

To add/load a new inference engine:

```bash
curl -X POST http://localhost:8080/engines \\
  -H "Content-Type: application/json" \\
  -d '{
    "model_name": "your_new_model_name",
    "path_to_model_files": "/path/to/your/model/files"
  }'
```

**Example Response:**

```json
{
  "id": "new_engine_id",
  "status": "loading"
}
```

### Get Engine Status

To get the status of a specific engine:

```bash
curl http://localhost:8080/engines/your_engine_id/status
```

**Example Response:**

```json
{
  "id": "your_engine_id",
  "model": "model_name",
  "status": "running"
}
```

### Remove Engine

To remove/unload an engine:

```bash
curl -X DELETE http://localhost:8080/engines/your_engine_id
```

**Example Response:**

```json
{
  "id": "your_engine_id",
  "status": "removed"
}
```

## Building from Source

### Prerequisites

*   CMake (version 3.14 or higher)
*   C++17 compiler
*   cURL
*   [llama.cpp](https://github.com/ggerganov/llama.cpp) (cloned into `external/llama.cpp`)

### Optional Dependencies

*   **CUDA:** For NVIDIA GPU acceleration.
*   **Vulkan:** For AMD/Intel GPU acceleration.
*   **MPI:** For distributed inference (OpenMPI, MPICH, or MS-MPI).

### Build Steps

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/your-username/kolosal-server.git
    cd kolosal-server
    ```

2.  **Initialize submodules (if any):**

    ```bash
    git submodule update --init --recursive
    ```

3.  **Create a build directory:**

    ```bash
    mkdir build
    cd build
    ```

4.  **Configure CMake:**

    *   **Basic build:**

        ```bash
        cmake ..
        ```

    *   **With CUDA support:**

        ```bash
        cmake .. -DUSE_CUDA=ON
        ```

    *   **With Vulkan support:**

        ```bash
        cmake .. -DUSE_VULKAN=ON
        ```

    *   **With MPI support:**

        ```bash
        cmake .. -DUSE_MPI=ON
        ```

5.  **Build the project:**

    ```bash
    cmake --build .
    ```

    On Windows, you might need to specify the configuration (e.g., Release or Debug):

    ```bash
    cmake --build . --config Release
    ```

### Installation

After building, you can install the server using:

```bash
cmake --install .
```

This will install the executables, libraries, and header files to the default installation directory (e.g., `/usr/local` on Linux, `C:\Program Files` on Windows).

## Usage

```bash
./kolosal-server [OPTIONS]
```

### Options

*   `-p, --port PORT`: Server port (default: 8080)
*   `-h, --help`: Show help message
*   `-v, --version`: Show version information

## Contributing

Contributions are welcome! Please feel free to submit pull requests, report issues, or suggest new features.

## License

This project is licensed under the [MIT License](LICENSE).