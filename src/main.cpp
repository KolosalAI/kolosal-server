#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include "kolosal_server.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/auth/auth_middleware.hpp"

using namespace kolosal;

// Global flag for graceful shutdown
std::atomic<bool> keep_running{true};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    keep_running = false;
}

void print_usage(const char* program_name) {
    ServerConfig config;
    config.printHelp();
}

void print_version() {
    ServerConfig config;
    config.printVersion();
}

int main(int argc, char* argv[]) {
    // Load configuration from command line arguments
    ServerConfig config;
    if (!config.loadFromArgs(argc, argv)) {
        return config.validate() ? 0 : 1; // Return 0 for help/version, 1 for errors
    }
    
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifdef _WIN32
    std::signal(SIGBREAK, signal_handler);
#endif
    
    // Print startup banner
    std::cout << "Starting Kolosal Server v1.0.0..." << std::endl;
    config.printSummary();
    
    // Initialize the server
    ServerAPI& server = ServerAPI::instance();
    
    if (!server.init(config.port)) {
        std::cerr << "Failed to initialize server on port " << config.port << std::endl;
        return 1;
    }
    
    // Configure authentication if enabled
    if (config.auth.enableAuth) {
        try {
            auto& authMiddleware = server.getAuthMiddleware();
            
            // Update rate limiter configuration
            authMiddleware.updateRateLimiterConfig(config.auth.rateLimiter);
            
            // Update CORS configuration
            authMiddleware.updateCorsConfig(config.auth.cors);
            
            ServerLogger::logInfo("Authentication configured - Rate Limit: %s, CORS: %s",
                                 config.auth.rateLimiter.enabled ? "enabled" : "disabled",
                                 config.auth.cors.enabled ? "enabled" : "disabled");
        } catch (const std::exception& e) {
            std::cerr << "Failed to configure authentication: " << e.what() << std::endl;
            return 1;
        }
    }
    
    // Load models if specified
    if (!config.models.empty()) {
        auto& nodeManager = server.getNodeManager();
        
        for (const auto& modelConfig : config.models) {
            std::cout << "Configuring model '" << modelConfig.id << "'..." << std::endl;
            
            if (modelConfig.loadAtStartup) {
                std::cout << "Loading model '" << modelConfig.id << "' from " << modelConfig.path << std::endl;
                bool success = nodeManager.addEngine(modelConfig.id, 
                                                   modelConfig.path.c_str(), 
                                                   modelConfig.loadParams, 
                                                   modelConfig.mainGpuId);
                if (success) {
                    std::cout << "✓ Model '" << modelConfig.id << "' loaded successfully" << std::endl;
                    ServerLogger::logInfo("Model '%s' loaded successfully", modelConfig.id.c_str());
                } else {
                    std::cerr << "✗ Failed to load model '" << modelConfig.id << "'" << std::endl;
                    ServerLogger::logError("Failed to load model '%s' from %s", 
                                         modelConfig.id.c_str(), modelConfig.path.c_str());
                    return 1;
                }
            } else {
                std::cout << "Registering model '" << modelConfig.id << "' for lazy loading" << std::endl;
                // For lazy loading, we might need to store the configuration for later use
                // This would require extending the NodeManager to support lazy loading
                ServerLogger::logInfo("Model '%s' registered for lazy loading", modelConfig.id.c_str());
            }
        }
    }
    
    std::cout << "\nServer started successfully!" << std::endl;
    std::cout << "Server URL: http://" << config.host << ":" << config.port << std::endl;
    std::cout << "\nAvailable endpoints:" << std::endl;
    std::cout << "  GET  /health                 - Health status" << std::endl;
    std::cout << "  GET  /models                 - List available models" << std::endl;
    std::cout << "  POST /v1/chat/completions    - Chat completions (OpenAI compatible)" << std::endl;
    std::cout << "  POST /v1/completions         - Text completions (OpenAI compatible)" << std::endl;
    std::cout << "  GET  /engines                - List engines" << std::endl;
    std::cout << "  POST /engines                - Add new engine" << std::endl;
    std::cout << "  GET  /engines/{id}/status    - Engine status" << std::endl;
    std::cout << "  DELETE /engines/{id}         - Remove engine" << std::endl;
    
    if (config.auth.enableAuth) {
        std::cout << "\nAuthentication endpoints:" << std::endl;
        std::cout << "  GET  /v1/auth/config         - Get authentication configuration" << std::endl;
        std::cout << "  PUT  /v1/auth/config         - Update authentication configuration" << std::endl;
        std::cout << "  GET  /v1/auth/stats          - Get authentication statistics" << std::endl;
        std::cout << "  POST /v1/auth/clear          - Clear rate limit data" << std::endl;
    }
    
    if (config.enableMetrics) {
        std::cout << "\nMetrics endpoints:" << std::endl;
        std::cout << "  GET  /metrics                - Prometheus metrics" << std::endl;
    }
    
    if (config.enableSwagger) {
        std::cout << "\nDocumentation:" << std::endl;
        std::cout << "  GET  /swagger                - API documentation" << std::endl;
    }
    
    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;
    
    // Main server loop
    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Shutting down server..." << std::endl;
    server.shutdown();
    std::cout << "Server stopped." << std::endl;
    
    return 0;
}
