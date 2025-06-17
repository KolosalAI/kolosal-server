#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "export.hpp"
#include "auth/rate_limiter.hpp"
#include "auth/cors_handler.hpp"
#include "inference.h"

namespace kolosal {

/**
 * @brief Configuration for a model to be loaded at startup
 */
struct ModelConfig {
    std::string id;                    // Unique identifier for the model
    std::string path;                  // Path to the model file
    LoadingParameters loadParams;      // Model loading parameters
    int mainGpuId = 0;                // GPU ID to use for this model
    bool loadAtStartup = true;        // Whether to load the model immediately
    bool preloadContext = false;      // Whether to preload context for faster inference
    
    ModelConfig() = default;
    ModelConfig(const std::string& modelId, const std::string& modelPath, bool load = true)
        : id(modelId), path(modelPath), loadAtStartup(load) {}
};

/**
 * @brief Authentication configuration
 */
struct AuthConfig {
    // Rate limiting configuration
    auth::RateLimiter::Config rateLimiter;
    
    // CORS configuration
    auth::CorsHandler::Config cors;
    
    // General auth settings
    bool enableAuth = true;           // Whether authentication is enabled
    bool requireApiKey = false;       // Whether API key is required
    std::string apiKeyHeader = "X-API-Key"; // Header name for API key
    std::vector<std::string> allowedApiKeys; // List of valid API keys
    
    AuthConfig() = default;
};

/**
 * @brief Server startup configuration
 */
struct KOLOSAL_SERVER_API ServerConfig {
    // Basic server settings
    std::string port = "8080";
    std::string host = "0.0.0.0";
    int maxConnections = 100;
    std::chrono::seconds requestTimeout{30};
    bool allowPublicAccess = false;    // Enable/disable external network access
    
    // Logging configuration
    std::string logLevel = "INFO";    // DEBUG, INFO, WARN, ERROR
    std::string logFile = "";         // Empty means console only
    bool enableAccessLog = false;     // Whether to log all requests
    
    // Performance settings
    int workerThreads = 0;            // 0 = auto-detect based on CPU cores
    size_t maxRequestSize = 16 * 1024 * 1024; // 16MB max request size
    std::chrono::seconds idleTimeout{300}; // Model idle timeout
    
    // Models to load at startup
    std::vector<ModelConfig> models;
    
    // Authentication configuration
    AuthConfig auth;
      // Feature flags
    bool enableHealthCheck = true;
    bool enableMetrics = false;
    
    ServerConfig() = default;
    
    /**
     * @brief Load configuration from command line arguments
     * @param argc Argument count
     * @param argv Argument values
     * @return True if configuration was loaded successfully
     */
    bool loadFromArgs(int argc, char* argv[]);
      /**
     * @brief Load configuration from YAML file
     * @param configFile Path to configuration file
     * @return True if configuration was loaded successfully
     */
    bool loadFromFile(const std::string& configFile);
    
    /**
     * @brief Save current configuration to YAML file
     * @param configFile Path to save configuration
     * @return True if configuration was saved successfully
     */
    bool saveToFile(const std::string& configFile) const;
    
    /**
     * @brief Validate the configuration
     * @return True if configuration is valid
     */
    bool validate() const;
      /**
     * @brief Print configuration summary
     */
    void printSummary() const;
    
    /**
     * @brief Print help message
     */
    static void printHelp();
    
    /**
     * @brief Print version information
     */
    static void printVersion();
};

} // namespace kolosal
