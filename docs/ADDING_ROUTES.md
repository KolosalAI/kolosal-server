# Adding New Routes

This guide explains how to add new API endpoints to the Kolosal Server by implementing custom route handlers.

## Route Architecture

Routes in Kolosal Server follow the `IRoute` interface pattern:

```cpp
class IRoute {
public:
    virtual bool match(const std::string& method, const std::string& path) = 0;
    virtual void handle(SocketType sock, const std::string& body) = 0;
    virtual ~IRoute() {}
};
```

## Step-by-Step Guide

### 1. Create Route Header File

Create a new header file in `include/kolosal/routes/`:

**File**: `include/kolosal/routes/example_route.hpp`

```cpp
#pragma once

#include "route_interface.hpp"
#include "../export.hpp"

namespace kolosal {
    
    /**
     * @brief Example route demonstrating custom endpoint implementation
     * 
     * This route handles GET/POST requests to /v1/example
     * and demonstrates common patterns for route implementation.
     */    class KOLOSAL_SERVER_API ExampleRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
        
    private:
        void handleGetRequest(SocketType sock);
        void handlePostRequest(SocketType sock, const std::string& body);
        json processStatusAction(const json& request);
        json processListEnginesAction(const json& request);
        json processCustomInferenceAction(const json& request);
    };
    
} // namespace kolosal
```

### 2. Implement Route Logic

Create the implementation file in `src/routes/`:

**File**: `src/routes/example_route.cpp`

```cpp
#include "kolosal/routes/example_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>

using json = nlohmann::json;

namespace kolosal {
    
    bool ExampleRoute::match(const std::string& method, const std::string& path) {
        // Define which requests this route should handle
        return ((method == "GET" || method == "POST") && 
                path == "/v1/example");
    }
    
    void ExampleRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Processing example route request", 
                                 std::this_thread::get_id());
              // Handle different HTTP methods
            // Note: HTTP method parsing would need to be implemented
            // For this example, we'll determine method from the route registration
            
            // Since this route handles both GET and POST, we need to determine which
            // For simplicity, assume POST if body is not empty, GET otherwise
            if (body.empty()) {
                handleGetRequest(sock);
            } else {
                handlePostRequest(sock, body);
            }
              } catch (const json::exception& ex) {
            ServerLogger::logError("JSON parsing error in example route: %s", ex.what());
            send_response(sock, 400, "{\"error\":{\"message\":\"Invalid JSON format\"}}");
        } catch (const std::invalid_argument& ex) {
            ServerLogger::logError("Validation error in example route: %s", ex.what());
            send_response(sock, 400, "{\"error\":{\"message\":\"" + std::string(ex.what()) + "\"}}");
        } catch (const std::runtime_error& ex) {
            ServerLogger::logError("Runtime error in example route: %s", ex.what());
            send_response(sock, 500, "{\"error\":{\"message\":\"" + std::string(ex.what()) + "\"}}");
        } catch (const std::exception& ex) {
            ServerLogger::logError("Unexpected error in example route: %s", ex.what());
            send_response(sock, 500, "{\"error\":{\"message\":\"Internal server error\"}}");
        }
    }    
    void ExampleRoute::handleGetRequest(SocketType sock) {
        // GET request - return status information
        json response = {
            {"endpoint", "example"},
            {"method", "GET"},
            {"timestamp", std::time(nullptr)},
            {"server_info", {
                {"version", "1.0.0"},
                {"status", "operational"}
            }}
        };
        
        send_response(sock, 200, response.dump());
        ServerLogger::logInfo("Sent GET response for example endpoint");
    }
    
    void ExampleRoute::handlePostRequest(SocketType sock, const std::string& body) {
        // Parse JSON body
        if (body.empty()) {
            throw std::invalid_argument("Request body cannot be empty for POST requests");
        }
        
        auto j = json::parse(body);
        
        // Validate required fields
        if (!j.contains("action")) {
            throw std::invalid_argument("Missing required field: action");
        }
        
        std::string action = j["action"].get<std::string>();
        
        // Process different actions
        json response;
        if (action == "status") {
            response = processStatusAction(j);
        } else if (action == "list_engines") {
            response = processListEnginesAction(j);
        } else if (action == "custom_inference") {
            response = processCustomInferenceAction(j);
        } else {
            throw std::invalid_argument("Unknown action: " + action);
        }
        
        send_response(sock, 200, response.dump());
        ServerLogger::logInfo("Processed POST action: %s", action.c_str());
    }
    
    json ExampleRoute::processStatusAction(const json& request) {
        auto& nodeManager = ServerAPI::instance().getNodeManager();
        auto engineIds = nodeManager.listEngineIds();
        
        return json{
            {"action", "status"},
            {"result", {
                {"total_engines", engineIds.size()},
                {"engine_list", engineIds},
                {"server_status", "healthy"}
            }}
        };
    }
    
    json ExampleRoute::processListEnginesAction(const json& request) {
        auto& nodeManager = ServerAPI::instance().getNodeManager();
        auto engineIds = nodeManager.listEngineIds();
        
        json engines = json::array();
        for (const auto& engineId : engineIds) {
            auto engine = nodeManager.getEngine(engineId);
            engines.push_back({
                {"engine_id", engineId},
                {"status", engine ? "loaded" : "unloaded"}
            });
        }
        
        return json{
            {"action", "list_engines"},
            {"result", {
                {"engines", engines},
                {"count", engines.size()}
            }}
        };
    }
    
    json ExampleRoute::processCustomInferenceAction(const json& request) {
        // Validate custom inference parameters
        if (!request.contains("model") || !request.contains("prompt")) {
            throw std::invalid_argument("Custom inference requires 'model' and 'prompt' fields");
        }
        
        std::string model = request["model"].get<std::string>();
        std::string prompt = request["prompt"].get<std::string>();
        
        // Get inference engine
        auto& nodeManager = ServerAPI::instance().getNodeManager();
        auto engine = nodeManager.getEngine(model);
        
        if (!engine) {
            throw std::runtime_error("Model '" + model + "' not found or not loaded");
        }
        
        // Set up inference parameters
        CompletionParameters params;
        params.prompt = prompt;
        params.maxNewTokens = request.value("max_tokens", 50);
        params.temperature = static_cast<float>(request.value("temperature", 0.7));
        params.streaming = false;
        
        // Submit inference job
        int jobId = engine->submitCompletionsJob(params);
        if (jobId < 0) {
            throw std::runtime_error("Failed to submit inference job");
        }
        
        // Wait for completion
        engine->waitForJob(jobId);
        
        if (engine->hasJobError(jobId)) {
            std::string error = engine->getJobError(jobId);
            throw std::runtime_error("Inference error: " + error);
        }
        
        // Get results
        CompletionResult result = engine->getJobResult(jobId);
        
        return json{
            {"action", "custom_inference"},
            {"result", {
                {"model", model},
                {"prompt", prompt},
                {"completion", result.text},
                {"tokens_per_second", result.tps}
            }}
        };
    }
    
} // namespace kolosal
```

### 3. Register the Route

Add your route to the server initialization in `src/server_api.cpp`:

```cpp
// Add include at the top
#include "kolosal/routes/example_route.hpp"

// In ServerAPI::init() method, add:
pImpl->server->addRoute(std::make_unique<ExampleRoute>());
```

### 4. Update Build Configuration

Add the new source file to `CMakeLists.txt`:

```cmake
set(SOURCES
    src/server.cpp
    src/server_api.cpp
    src/logger.cpp
    src/download_utils.cpp
    src/routes/chat_completion_route.cpp
    src/routes/completion_route.cpp
    src/routes/add_engine_route.cpp
    src/routes/list_engines_route.cpp
    src/routes/remove_engine_route.cpp
    src/routes/engine_status_route.cpp
    src/routes/health_status_route.cpp
    src/routes/example_route.cpp  # Add this line
    src/node_manager.cpp
    src/inference.cpp
)
```

## Advanced Route Patterns

### Streaming Response Route

For endpoints that need to stream data:

```cpp
class StreamingRoute : public IRoute {
public:
    void handle(SocketType sock, const std::string& body) override {
        try {
            // Start streaming response with Server-Sent Events headers
            std::map<std::string, std::string> headers = {
                {"Content-Type", "text/event-stream"},
                {"Cache-Control", "no-cache"},
                {"Access-Control-Allow-Origin", "*"}
            };
            
            begin_streaming_response(sock, 200, headers);
            
            // Process request and get streaming data source
            auto dataSource = setupDataSource(body);
            
            // Stream data chunks
            while (dataSource->hasMore()) {
                auto chunk = dataSource->getNext();
                
                // Format as Server-Sent Event
                std::string sseData = "data: " + chunk.to_json().dump() + "\n\n";
                StreamChunk streamChunk(sseData, false);
                
                send_stream_chunk(sock, streamChunk);
                
                // Small delay to prevent overwhelming the client
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // Send completion marker
            StreamChunk doneChunk("data: [DONE]\n\n", true);
            send_stream_chunk(sock, doneChunk);
            
        } catch (...) {
            // Handle errors gracefully
            StreamChunk errorChunk("data: {\"error\": \"Stream terminated\"}\n\n", true);
            send_stream_chunk(sock, errorChunk);
        }
    }
};
```

### Path Parameter Route

For routes with dynamic path segments:

```cpp
class ParameterizedRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        // Match pattern like /v1/models/{model_id}/status
        if (method != "GET") return false;
        
        // Simple pattern matching - check if path starts with prefix and ends with suffix
        return (path.find("/v1/models/") == 0 && 
                path.find("/status") != std::string::npos &&
                path.length() > 19); // "/v1/models/" + at least 1 char + "/status"
    }
    
    void handle(SocketType sock, const std::string& body) override {
        // Extract path parameter (this would need to be passed from the server)
        // For now, we'll simulate extraction
        std::string modelId = extractModelIdFromPath(getRequestPath());
        
        // Process with extracted parameter
        processModelStatus(sock, modelId);
    }

private:
    std::string extractModelIdFromPath(const std::string& path) {
        // Extract model ID from path like /v1/models/{model_id}/status
        size_t start = path.find("/v1/models/") + 11; // Length of "/v1/models/"
        size_t end = path.find("/status", start);
        if (end == std::string::npos) {
            throw std::invalid_argument("Invalid path format");
        }
        return path.substr(start, end - start);
    }
    
    std::string getRequestPath() {
        // This would need to be implemented to get the actual request path
        // For now, return a placeholder
        return "/v1/models/example-model/status";
    }
};
```

### Authenticated Route

For routes requiring authentication:

```cpp
class AuthenticatedRoute : public IRoute {
public:
    void handle(SocketType sock, const std::string& body) override {
        try {
            // Extract and validate authorization header
            // Note: Actual header extraction would need to be implemented
            std::string authHeader = getAuthorizationHeader();
            
            if (!validateAuth(authHeader)) {
                send_response(sock, 401, "{\"error\":{\"message\":\"Invalid or missing authentication\"}}");
                return;
            }
            
            // Process authenticated request
            processAuthenticatedRequest(sock, body);
            
        } catch (...) {
            send_response(sock, 500, "{\"error\":{\"message\":\"Authentication validation failed\"}}");
        }
    }

private:
    std::string getAuthorizationHeader() {
        // This would need to be implemented to extract the Authorization header
        // For now, return a placeholder
        return "Bearer sample-token";
    }
    
    bool validateAuth(const std::string& authHeader) {
        // Implement your authentication logic
        if (authHeader.empty()) return false;
        
        // Example: Bearer token validation
        if (authHeader.substr(0, 7) != "Bearer ") return false;
        
        std::string token = authHeader.substr(7);
        return validateToken(token);
    }
    
    bool validateToken(const std::string& token) {
        // Implement token validation logic
        // This could involve:
        // - Database lookup
        // - JWT verification
        // - API key validation
        return !token.empty(); // Simplified example
    }
};
```

## Utility Functions

The Kolosal Server provides utility functions in `utils.hpp` for handling HTTP responses:

### Available Utility Functions

#### `send_response()`
Sends a complete HTTP response with headers and body:

```cpp
inline void send_response(
    SocketType sock,
    int status_code,
    const std::string& body,
    const std::map<std::string, std::string>& headers = {{"Content-Type", "application/json"}}
);
```

**Usage Example:**
```cpp
// Send JSON response
json response = {{"status", "success"}};
send_response(sock, 200, response.dump());

// Send with custom headers
std::map<std::string, std::string> headers = {
    {"Content-Type", "text/plain"},
    {"Cache-Control", "no-cache"}
};
send_response(sock, 200, "Hello World", headers);
```

#### `begin_streaming_response()`
Starts a streaming HTTP response with chunked transfer encoding:

```cpp
inline void begin_streaming_response(
    SocketType sock,
    int status_code,
    const std::map<std::string, std::string>& headers = {}
);
```

#### `send_stream_chunk()`
Sends a chunk of data in a streaming response:

```cpp
inline void send_stream_chunk(SocketType sock, const StreamChunk& chunk);

// StreamChunk structure:
struct StreamChunk {
    std::string data;        // The content to stream
    bool isComplete = false; // Whether this is the final chunk
};
```

**Streaming Example:**
```cpp
// Start streaming response
std::map<std::string, std::string> headers = {
    {"Content-Type", "text/event-stream"},
    {"Cache-Control", "no-cache"}
};
begin_streaming_response(sock, 200, headers);

// Send data chunks
while (hasMoreData()) {
    std::string data = getNextChunk();
    StreamChunk chunk(data, false);
    send_stream_chunk(sock, chunk);
}

// Send final chunk to complete the stream
StreamChunk finalChunk("", true);
send_stream_chunk(sock, finalChunk);
```

### Common Response Patterns

#### JSON Error Response
```cpp
void sendJsonError(SocketType sock, int statusCode, const std::string& message) {
    json error = {
        {"error", {
            {"message", message},
            {"type", "error"},
            {"code", statusCode}
        }}
    };
    send_response(sock, statusCode, error.dump());
}
```

#### Server-Sent Events (SSE) Format
```cpp
void sendSSEEvent(SocketType sock, const std::string& eventData, bool isLast = false) {
    std::string sseData = "data: " + eventData + "\n\n";
    StreamChunk chunk(sseData, isLast);
    send_stream_chunk(sock, chunk);
}
```

## Testing Your Route

### Unit Testing

```cpp
#include <gtest/gtest.h>
#include "kolosal/routes/example_route.hpp"

class ExampleRouteTest : public ::testing::Test {
protected:
    void SetUp() override {
        route = std::make_unique<ExampleRoute>();
    }
    
    std::unique_ptr<ExampleRoute> route;
};

TEST_F(ExampleRouteTest, MatchesCorrectPaths) {
    EXPECT_TRUE(route->match("GET", "/v1/example"));
    EXPECT_TRUE(route->match("POST", "/v1/example"));
    EXPECT_FALSE(route->match("GET", "/v1/other"));
    EXPECT_FALSE(route->match("DELETE", "/v1/example"));
}

TEST_F(ExampleRouteTest, HandlesValidRequest) {
    // Mock socket and test request handling
    // Implementation depends on your testing framework
}
```

### Integration Testing

```bash
# Test GET request
curl -X GET http://localhost:8080/v1/example

# Test POST request
curl -X POST http://localhost:8080/v1/example \
  -H "Content-Type: application/json" \
  -d '{"action": "status"}'

# Test error handling
curl -X POST http://localhost:8080/v1/example \
  -H "Content-Type: application/json" \
  -d '{"invalid": "request"}'
```

## Best Practices

### 1. Error Handling
- Always wrap main logic in try-catch blocks
- Return appropriate HTTP status codes
- Log errors for debugging
- Provide meaningful error messages

### 2. Input Validation
- Validate all JSON input
- Check required fields
- Sanitize user input
- Validate parameter ranges

### 3. Performance
- Minimize processing time in route handlers
- Use async operations for long-running tasks
- Implement proper resource cleanup
- Consider caching for frequently accessed data

### 4. Security
- Validate all inputs
- Implement authentication where needed
- Avoid exposing sensitive information
- Log security-relevant events

### 5. Logging
- Log all significant events
- Include thread IDs for debugging
- Use appropriate log levels
- Avoid logging sensitive data

### 6. Documentation
- Document route purpose and behavior
- Provide usage examples
- Document all parameters
- Keep API documentation updated

## Common Patterns

### Route Registration Pattern

```cpp
// In server_api.cpp
void ServerAPI::registerAllRoutes() {
    // Core API routes
    pImpl->server->addRoute(std::make_unique<ChatCompletionsRoute>());
    pImpl->server->addRoute(std::make_unique<CompletionsRoute>());
    
    // Engine management routes
    pImpl->server->addRoute(std::make_unique<AddEngineRoute>());
    pImpl->server->addRoute(std::make_unique<ListEnginesRoute>());
    pImpl->server->addRoute(std::make_unique<RemoveEngineRoute>());
    pImpl->server->addRoute(std::make_unique<EngineStatusRoute>());
    
    // System routes
    pImpl->server->addRoute(std::make_unique<HealthStatusRoute>());
    
    // Custom routes
    pImpl->server->addRoute(std::make_unique<ExampleRoute>());
}
```

### Route Factory Pattern

For complex route hierarchies:

```cpp
class RouteFactory {
public:
    static std::vector<std::unique_ptr<IRoute>> createAllRoutes() {
        std::vector<std::unique_ptr<IRoute>> routes;
        
        // Add core routes
        routes.emplace_back(std::make_unique<ChatCompletionsRoute>());
        routes.emplace_back(std::make_unique<CompletionsRoute>());
        
        // Add management routes
        routes.emplace_back(std::make_unique<AddEngineRoute>());
        routes.emplace_back(std::make_unique<ListEnginesRoute>());
        
        return routes;
    }
};
```

This comprehensive guide should help you create robust, well-structured routes for the Kolosal Server. Remember to follow the established patterns and maintain consistency with existing routes.
