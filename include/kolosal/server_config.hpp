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
    std::string type = "llm";          // Model type: "llm" or "embedding"
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
 * @brief Database configuration
 */
struct DatabaseConfig {
    struct QdrantConfig {
        bool enabled = false;
        std::string host = "localhost";
        int port = 6333;
        std::string collectionName = "documents";
        std::string defaultEmbeddingModel = "text-embedding-3-small";
        int timeout = 30;
        std::string apiKey = "";
        int maxConnections = 10;
        int connectionTimeout = 5;
    } qdrant;
    
    DatabaseConfig() = default;
};

/**
 * @brief Internet search configuration
 */
struct SearchConfig {
    bool enabled = false;                      // Whether internet search is enabled
    std::string searxng_url = "http://localhost:4000"; // SearXNG instance URL
    int timeout = 30;                         // Request timeout in seconds
    int max_results = 20;                     // Maximum number of search results to return
    std::string default_engine = "";          // Default search engine (empty = use SearXNG default)
    std::string api_key = "";                 // Optional API key for authentication
    bool enable_safe_search = true;           // Enable safe search by default
    std::string default_format = "json";      // Default output format (json, xml, csv)
    std::string default_language = "en";      // Default search language
    std::string default_category = "general"; // Default search category
    
    SearchConfig() = default;
};

/**
 * @brief Server startup configuration
 */
struct KOLOSAL_SERVER_API ServerConfig {    // Basic server settings
    std::string port = "8080";
    std::string host = "0.0.0.0";
    int maxConnections = 100;
    std::chrono::seconds requestTimeout{30};
    bool allowPublicAccess = false;    // Enable/disable external network access
    bool allowInternetAccess = false;  // Enable/disable internet access (UPnP + public IP detection)
    
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
    
    // Database configuration
    DatabaseConfig database;
    
    // Internet search configuration
    SearchConfig search;
    
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
