#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <thread>

namespace kolosal
{

    bool ServerConfig::loadFromArgs(int argc, char *argv[])
    {
        for (int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];

            // Basic server options
            if ((arg == "-p" || arg == "--port") && i + 1 < argc)
            {
                port = argv[++i];
            }
            else if ((arg == "--host") && i + 1 < argc)
            {
                host = argv[++i];
            }
            else if ((arg == "-c" || arg == "--config") && i + 1 < argc)
            {
                if (!loadFromFile(argv[++i]))
                {
                    return false;
                }
            }
            else if ((arg == "--max-connections") && i + 1 < argc)
            {
                maxConnections = std::stoi(argv[++i]);
            }
            else if ((arg == "--worker-threads") && i + 1 < argc)
            {
                workerThreads = std::stoi(argv[++i]);
            }

            // Logging options
            else if ((arg == "--log-level") && i + 1 < argc)
            {
                logLevel = argv[++i];
            }
            else if ((arg == "--log-file") && i + 1 < argc)
            {
                logFile = argv[++i];
            }
            else if (arg == "--enable-access-log")
            {
                enableAccessLog = true;
            }

            // Authentication options
            else if (arg == "--disable-auth")
            {
                auth.enableAuth = false;
            }
            else if (arg == "--require-api-key")
            {
                auth.requireApiKey = true;
            }
            else if ((arg == "--api-key") && i + 1 < argc)
            {
                auth.allowedApiKeys.push_back(argv[++i]);
            }
            else if ((arg == "--api-key-header") && i + 1 < argc)
            {
                auth.apiKeyHeader = argv[++i];
            }

            // Rate limiting options
            else if ((arg == "--rate-limit") && i + 1 < argc)
            {
                auth.rateLimiter.maxRequests = std::stoul(argv[++i]);
            }
            else if ((arg == "--rate-window") && i + 1 < argc)
            {
                auth.rateLimiter.windowSize = std::chrono::seconds(std::stoi(argv[++i]));
            }
            else if (arg == "--disable-rate-limit")
            {
                auth.rateLimiter.enabled = false;
            }

            // CORS options
            else if ((arg == "--cors-origin") && i + 1 < argc)
            {
                auth.cors.allowedOrigins.push_back(argv[++i]);
            }
            else if ((arg == "--cors-methods") && i + 1 < argc)
            {
                std::string methods = argv[++i];
                auth.cors.allowedMethods.clear();
                // Parse comma-separated methods
                size_t start = 0, end = 0;
                while ((end = methods.find(',', start)) != std::string::npos)
                {
                    auth.cors.allowedMethods.push_back(methods.substr(start, end - start));
                    start = end + 1;
                }
                auth.cors.allowedMethods.push_back(methods.substr(start));
            }
            else if (arg == "--cors-credentials")
            {
                auth.cors.allowCredentials = true;
            }
            else if (arg == "--disable-cors")
            {
                auth.cors.enabled = false;
            }

            // Model loading options
            else if ((arg == "-m" || arg == "--model") && i + 2 < argc)
            {
                ModelConfig model;
                model.id = argv[++i];
                model.path = argv[++i];
                model.loadAtStartup = true;
                models.push_back(model);
            }
            else if ((arg == "--model-lazy") && i + 2 < argc)
            {
                ModelConfig model;
                model.id = argv[++i];
                model.path = argv[++i];
                model.loadAtStartup = false;
                models.push_back(model);
            }
            else if ((arg == "--model-gpu") && i + 1 < argc)
            {
                if (!models.empty())
                {
                    models.back().mainGpuId = std::stoi(argv[++i]);
                }
            }
            else if ((arg == "--model-ctx-size") && i + 1 < argc)
            {
                if (!models.empty())
                {
                    models.back().loadParams.n_ctx = std::stoi(argv[++i]);
                }
            }

            // Performance options
            else if ((arg == "--request-timeout") && i + 1 < argc)
            {
                requestTimeout = std::chrono::seconds(std::stoi(argv[++i]));
            }
            else if ((arg == "--idle-timeout") && i + 1 < argc)
            {
                idleTimeout = std::chrono::seconds(std::stoi(argv[++i]));
            }
            else if ((arg == "--max-request-size") && i + 1 < argc)
            {
                maxRequestSize = std::stoul(argv[++i]);
            } // Feature flags
            else if (arg == "--enable-metrics")
            {
                enableMetrics = true;
            }
            else if (arg == "--disable-health-check")
            {
                enableHealthCheck = false;
            }
            else if (arg == "--public" || arg == "--allow-public-access")
            {
                allowPublicAccess = true;
            }
            else if (arg == "--no-public" || arg == "--disable-public-access")
            {
                allowPublicAccess = false;
            }
            else if (arg == "--internet" || arg == "--allow-internet-access")
            {
                allowInternetAccess = true;
                allowPublicAccess = true; // Internet access implies public access
            }
            else if (arg == "--no-internet" || arg == "--disable-internet-access")
            {
                allowInternetAccess = false;
            }
            // Help and version
            else if (arg == "-h" || arg == "--help")
            {
                printHelp();
                return false;
            }
            else if (arg == "-v" || arg == "--version")
            {
                printVersion();
                return false;
            }
            else if (arg.front() == '-')
            {
                std::cerr << "Unknown option: " << arg << std::endl;
                return false;
            }
        }

        return validate();
    }

    bool ServerConfig::loadFromFile(const std::string &configFile)
    {
        try
        {
            YAML::Node config = YAML::LoadFile(configFile); // Load basic server settings
            if (config["server"])
            {
                auto server = config["server"];
                if (server["port"])
                    port = server["port"].as<std::string>();
                if (server["host"])
                    host = server["host"].as<std::string>();
                if (server["max_connections"])
                    maxConnections = server["max_connections"].as<int>();
                if (server["worker_threads"])
                    workerThreads = server["worker_threads"].as<int>();
                if (server["request_timeout"])
                    requestTimeout = std::chrono::seconds(server["request_timeout"].as<int>());
                if (server["max_request_size"])
                    maxRequestSize = server["max_request_size"].as<size_t>();
                if (server["idle_timeout"])
                    idleTimeout = std::chrono::seconds(server["idle_timeout"].as<int>());
                if (server["allow_public_access"])
                    allowPublicAccess = server["allow_public_access"].as<bool>();
                if (server["allow_internet_access"])
                {
                    allowInternetAccess = server["allow_internet_access"].as<bool>();
                    if (allowInternetAccess)
                    {
                        allowPublicAccess = true; // Internet access implies public access
                    }
                }
            }

            // Load logging settings
            if (config["logging"])
            {
                auto logging = config["logging"];
                if (logging["level"])
                    logLevel = logging["level"].as<std::string>();
                if (logging["file"])
                    logFile = logging["file"].as<std::string>();
                if (logging["access_log"])
                    enableAccessLog = logging["access_log"].as<bool>();
            }

            // Load authentication settings
            if (config["auth"])
            {
                auto authConfig = config["auth"];
                if (authConfig["enabled"])
                    auth.enableAuth = authConfig["enabled"].as<bool>();
                if (authConfig["require_api_key"])
                    auth.requireApiKey = authConfig["require_api_key"].as<bool>();
                if (authConfig["api_key_header"])
                    auth.apiKeyHeader = authConfig["api_key_header"].as<std::string>();
                if (authConfig["api_keys"])
                {
                    auth.allowedApiKeys.clear();
                    for (const auto &key : authConfig["api_keys"])
                    {
                        auth.allowedApiKeys.push_back(key.as<std::string>());
                    }
                }

                // Rate limiting
                if (authConfig["rate_limit"])
                {
                    auto rl = authConfig["rate_limit"];
                    if (rl["enabled"])
                        auth.rateLimiter.enabled = rl["enabled"].as<bool>();
                    if (rl["max_requests"])
                        auth.rateLimiter.maxRequests = rl["max_requests"].as<size_t>();
                    if (rl["window_size"])
                        auth.rateLimiter.windowSize = std::chrono::seconds(rl["window_size"].as<int>());
                }

                // CORS
                if (authConfig["cors"])
                {
                    auto cors = authConfig["cors"];
                    if (cors["enabled"])
                        auth.cors.enabled = cors["enabled"].as<bool>();
                    if (cors["allow_credentials"])
                        auth.cors.allowCredentials = cors["allow_credentials"].as<bool>();
                    if (cors["max_age"])
                        auth.cors.maxAge = cors["max_age"].as<int>();
                    if (cors["allowed_origins"])
                    {
                        auth.cors.allowedOrigins.clear();
                        for (const auto &origin : cors["allowed_origins"])
                        {
                            auth.cors.allowedOrigins.push_back(origin.as<std::string>());
                        }
                    }
                    if (cors["allowed_methods"])
                    {
                        auth.cors.allowedMethods.clear();
                        for (const auto &method : cors["allowed_methods"])
                        {
                            auth.cors.allowedMethods.push_back(method.as<std::string>());
                        }
                    }
                    if (cors["allowed_headers"])
                    {
                        auth.cors.allowedHeaders.clear();
                        for (const auto &header : cors["allowed_headers"])
                        {
                            auth.cors.allowedHeaders.push_back(header.as<std::string>());
                        }
                    }
                }
            }

            // Load models
            if (config["models"])
            {
                models.clear();                for (const auto &modelConfig : config["models"])
                {
                    ModelConfig model;
                    if (modelConfig["id"])
                        model.id = modelConfig["id"].as<std::string>();
                    if (modelConfig["path"])
                        model.path = modelConfig["path"].as<std::string>();
                    if (modelConfig["type"])
                        model.type = modelConfig["type"].as<std::string>();
                    if (modelConfig["load_at_startup"])
                        model.loadAtStartup = modelConfig["load_at_startup"].as<bool>();
                    if (modelConfig["main_gpu_id"])
                        model.mainGpuId = modelConfig["main_gpu_id"].as<int>();
                    if (modelConfig["preload_context"])
                        model.preloadContext = modelConfig["preload_context"].as<bool>();
                    if (modelConfig["load_params"])
                    {
                        auto params = modelConfig["load_params"];
                        if (params["n_ctx"])
                            model.loadParams.n_ctx = params["n_ctx"].as<int>();
                        if (params["n_keep"])
                            model.loadParams.n_keep = params["n_keep"].as<int>();
                        if (params["use_mmap"])
                            model.loadParams.use_mmap = params["use_mmap"].as<bool>();
                        if (params["use_mlock"])
                            model.loadParams.use_mlock = params["use_mlock"].as<bool>();
                        if (params["n_parallel"])
                            model.loadParams.n_parallel = params["n_parallel"].as<int>();
                        if (params["cont_batching"])
                            model.loadParams.cont_batching = params["cont_batching"].as<bool>();
                        if (params["n_batch"])
                            model.loadParams.n_batch = params["n_batch"].as<int>();
                        if (params["n_ubatch"])
                            model.loadParams.n_ubatch = params["n_ubatch"].as<int>();
                    }

                    models.push_back(model);
                }
            }

            // Load feature flags
            if (config["features"])
            {
                auto features = config["features"];
                if (features["health_check"])
                    enableHealthCheck = features["health_check"].as<bool>();
                if (features["metrics"])
                    enableMetrics = features["metrics"].as<bool>();
            }

            return validate();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing config file " << configFile << ": " << e.what() << std::endl;
            return false;
        }
    }

    bool ServerConfig::saveToFile(const std::string &configFile) const
    {
        try
        {
            YAML::Node config; // Server settings
            config["server"]["port"] = port;
            config["server"]["host"] = host;
            config["server"]["max_connections"] = maxConnections;
            config["server"]["worker_threads"] = workerThreads;
            config["server"]["request_timeout"] = static_cast<int>(requestTimeout.count());
            config["server"]["max_request_size"] = maxRequestSize;
            config["server"]["idle_timeout"] = static_cast<int>(idleTimeout.count());
            config["server"]["allow_public_access"] = allowPublicAccess;
            config["server"]["allow_internet_access"] = allowInternetAccess;

            // Logging settings
            config["logging"]["level"] = logLevel;
            config["logging"]["file"] = logFile;
            config["logging"]["access_log"] = enableAccessLog;

            // Authentication settings
            config["auth"]["enabled"] = auth.enableAuth;
            config["auth"]["require_api_key"] = auth.requireApiKey;
            config["auth"]["api_key_header"] = auth.apiKeyHeader;
            config["auth"]["api_keys"] = auth.allowedApiKeys;
            config["auth"]["rate_limit"]["enabled"] = auth.rateLimiter.enabled;
            config["auth"]["rate_limit"]["max_requests"] = auth.rateLimiter.maxRequests;
            config["auth"]["rate_limit"]["window_size"] = static_cast<int>(auth.rateLimiter.windowSize.count());
            config["auth"]["cors"]["enabled"] = auth.cors.enabled;
            config["auth"]["cors"]["allow_credentials"] = auth.cors.allowCredentials;
            config["auth"]["cors"]["max_age"] = auth.cors.maxAge;
            config["auth"]["cors"]["allowed_origins"] = auth.cors.allowedOrigins;
            config["auth"]["cors"]["allowed_methods"] = auth.cors.allowedMethods;
            config["auth"]["cors"]["allowed_headers"] = auth.cors.allowedHeaders;            // Models
            for (const auto &model : models)
            {
                YAML::Node modelNode;
                modelNode["id"] = model.id;
                modelNode["path"] = model.path;
                modelNode["type"] = model.type;
                modelNode["load_at_startup"] = model.loadAtStartup;
                modelNode["main_gpu_id"] = model.mainGpuId;
                modelNode["preload_context"] = model.preloadContext;
                modelNode["load_params"]["n_ctx"] = model.loadParams.n_ctx;
                modelNode["load_params"]["n_keep"] = model.loadParams.n_keep;
                modelNode["load_params"]["use_mmap"] = model.loadParams.use_mmap;
                modelNode["load_params"]["use_mlock"] = model.loadParams.use_mlock;
                modelNode["load_params"]["n_parallel"] = model.loadParams.n_parallel;
                modelNode["load_params"]["cont_batching"] = model.loadParams.cont_batching;
                modelNode["load_params"]["n_batch"] = model.loadParams.n_batch;
                modelNode["load_params"]["n_ubatch"] = model.loadParams.n_ubatch;
                config["models"].push_back(modelNode);
            }
            // Feature flags
            config["features"]["health_check"] = enableHealthCheck;
            config["features"]["metrics"] = enableMetrics;

            std::ofstream file(configFile);
            if (!file.is_open())
            {
                std::cerr << "Error: Cannot create config file: " << configFile << std::endl;
                return false;
            }

            file << config;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error saving config file " << configFile << ": " << e.what() << std::endl;
            return false;
        }
    }

    bool ServerConfig::validate() const
    {
        // Validate port
        try
        {
            int portNum = std::stoi(port);
            if (portNum < 1 || portNum > 65535)
            {
                std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
                return false;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: Invalid port number: " << port << std::endl;
            return false;
        }

        // Validate log level
        if (logLevel != "DEBUG" && logLevel != "INFO" && logLevel != "WARN" && logLevel != "ERROR")
        {
            std::cerr << "Error: Invalid log level: " << logLevel << std::endl;
            return false;
        }

        // Validate worker threads
        if (workerThreads < 0)
        {
            std::cerr << "Error: Worker threads must be non-negative" << std::endl;
            return false;
        }

        // Validate max connections
        if (maxConnections <= 0)
        {
            std::cerr << "Error: Max connections must be positive" << std::endl;
            return false;
        }

        // Validate models
        for (const auto &model : models)
        {
            if (model.id.empty())
            {
                std::cerr << "Error: Model ID cannot be empty" << std::endl;
                return false;
            }
            if (model.path.empty())
            {
                std::cerr << "Error: Model path cannot be empty for model: " << model.id << std::endl;
                return false;
            }
        }

        // Validate rate limiting
        if (auth.rateLimiter.enabled && auth.rateLimiter.maxRequests == 0)
        {
            std::cerr << "Error: Rate limit max requests must be positive when enabled" << std::endl;
            return false;
        }

        return true;
    }

    void ServerConfig::printSummary() const
    {
        std::cout << "=== Kolosal Server Configuration ===" << std::endl;
        std::cout << "Server:" << std::endl;
        std::cout << "  Port: " << port << std::endl;
        std::cout << "  Host: " << host << std::endl;
        std::cout << "  Public Access: " << (allowPublicAccess ? "Enabled" : "Disabled") << std::endl;
        std::cout << "  Internet Access: " << (allowInternetAccess ? "Enabled" : "Disabled") << std::endl;
        std::cout << "  Max Connections: " << maxConnections << std::endl;
        std::cout << "  Worker Threads: " << (workerThreads == 0 ? "Auto" : std::to_string(workerThreads)) << std::endl;
        std::cout << "  Request Timeout: " << requestTimeout.count() << "s" << std::endl;
        std::cout << "  Idle Timeout: " << idleTimeout.count() << "s" << std::endl;

        std::cout << "\nLogging:" << std::endl;
        std::cout << "  Level: " << logLevel << std::endl;
        std::cout << "  File: " << (logFile.empty() ? "Console" : logFile) << std::endl;
        std::cout << "  Access Log: " << (enableAccessLog ? "Enabled" : "Disabled") << std::endl;

        std::cout << "\nAuthentication:" << std::endl;
        std::cout << "  Auth: " << (auth.enableAuth ? "Enabled" : "Disabled") << std::endl;
        std::cout << "  API Key Required: " << (auth.requireApiKey ? "Yes" : "No") << std::endl;
        std::cout << "  Rate Limiting: " << (auth.rateLimiter.enabled ? "Enabled" : "Disabled") << std::endl;
        if (auth.rateLimiter.enabled)
        {
            std::cout << "    Max Requests: " << auth.rateLimiter.maxRequests << std::endl;
            std::cout << "    Window: " << auth.rateLimiter.windowSize.count() << "s" << std::endl;
        }
        std::cout << "  CORS: " << (auth.cors.enabled ? "Enabled" : "Disabled") << std::endl;
        if (auth.cors.enabled)
        {
            std::cout << "    Origins: " << auth.cors.allowedOrigins.size() << " configured" << std::endl;
        }

        std::cout << "\nModels:" << std::endl;
        if (models.empty())
        {
            std::cout << "  No models configured" << std::endl;
        }
        else
        {
            for (const auto &model : models)
            {
                std::cout << "  " << model.id << ":" << std::endl;
                std::cout << "    Path: " << model.path << std::endl;
                std::cout << "    Load at startup: " << (model.loadAtStartup ? "Yes" : "No") << std::endl;
                std::cout << "    GPU ID: " << model.mainGpuId << std::endl;
            }
        }
        std::cout << "\nFeatures:" << std::endl;
        std::cout << "  Health Check: " << (enableHealthCheck ? "Enabled" : "Disabled") << std::endl;
        std::cout << "  Metrics: " << (enableMetrics ? "Enabled" : "Disabled") << std::endl;
        std::cout << "====================================" << std::endl;
    }

    void ServerConfig::printHelp()
    {
        std::cout << "Kolosal Server v1.0.0 - High-performance AI inference server\n\n";
        std::cout << "USAGE:\n";
        std::cout << "    kolosal-server [OPTIONS]\n\n";
        std::cout << "OPTIONS:\n";
        std::cout << "  Basic Server:\n";
        std::cout << "    -p, --port PORT           Server port (default: 8080)\n";
        std::cout << "    --host HOST               Server host (default: 0.0.0.0)\n";
        std::cout << "    -c, --config FILE         Load configuration from YAML file\n";
        std::cout << "    --max-connections N       Maximum concurrent connections (default: 100)\n";
        std::cout << "    --worker-threads N        Number of worker threads (default: auto)\n";
        std::cout << "    --request-timeout SEC     Request timeout in seconds (default: 30)\n";
        std::cout << "    --idle-timeout SEC        Model idle timeout in seconds (default: 300)\n";
        std::cout << "    --max-request-size BYTES  Maximum request size in bytes (default: 16MB)\n\n";

        std::cout << "  Logging:\n";
        std::cout << "    --log-level LEVEL         Log level: DEBUG, INFO, WARN, ERROR (default: INFO)\n";
        std::cout << "    --log-file FILE           Log to file instead of console\n";
        std::cout << "    --enable-access-log       Enable HTTP access logging\n\n";

        std::cout << "  Authentication:\n";
        std::cout << "    --disable-auth            Disable all authentication\n";
        std::cout << "    --require-api-key         Require API key for all requests\n";
        std::cout << "    --api-key KEY             Add an allowed API key (can be used multiple times)\n";
        std::cout << "    --api-key-header HEADER   Header name for API key (default: X-API-Key)\n\n";

        std::cout << "  Rate Limiting:\n";
        std::cout << "    --rate-limit N            Maximum requests per window (default: 100)\n";
        std::cout << "    --rate-window SEC         Rate limit window in seconds (default: 60)\n";
        std::cout << "    --disable-rate-limit      Disable rate limiting\n\n";

        std::cout << "  CORS:\n";
        std::cout << "    --cors-origin ORIGIN      Add allowed CORS origin (can be used multiple times)\n";
        std::cout << "    --cors-methods METHODS    Comma-separated list of allowed methods\n";
        std::cout << "    --cors-credentials        Allow credentials in CORS requests\n";
        std::cout << "    --disable-cors            Disable CORS\n\n";

        std::cout << "  Models:\n";
        std::cout << "    -m, --model ID PATH       Load model at startup (ID and file path)\n";
        std::cout << "    --model-lazy ID PATH      Register model but don't load until first use\n";
        std::cout << "    --model-gpu ID            Set GPU ID for the last added model\n";
        std::cout << "    --model-ctx-size SIZE     Set context size for the last added model\n\n";
        std::cout << "  Features:\n";
        std::cout << "    --enable-metrics          Enable metrics collection\n";
        std::cout << "    --disable-health-check    Disable health check endpoint\n";
        std::cout << "    --public                  Allow external network access\n";
        std::cout << "    --allow-public-access     Allow external network access (same as --public)\n";
        std::cout << "    --no-public               Disable external network access (localhost only)\n";
        std::cout << "    --disable-public-access   Disable external network access (same as --no-public)\n";
        std::cout << "    --internet                Allow internet access (enables UPnP + public IP detection)\n";
        std::cout << "    --allow-internet-access   Allow internet access (same as --internet)\n";
        std::cout << "    --no-internet             Disable internet access\n";
        std::cout << "    --disable-internet-access Disable internet access (same as --no-internet)\n\n";

        std::cout << "  Help:\n";
        std::cout << "    -h, --help                Show this help message\n";
        std::cout << "    -v, --version             Show version information\n\n";

        std::cout << "EXAMPLES:\n";
        std::cout << "  # Basic server on port 3000\n";
        std::cout << "  kolosal-server --port 3000\n\n";

        std::cout << "  # Load two models at startup\n";
        std::cout << "  kolosal-server -m llama ./models/llama-7b.gguf -m gpt ./models/gpt-3.5.gguf\n\n";

        std::cout << "  # Server with authentication and rate limiting\n";
        std::cout << "  kolosal-server --require-api-key --api-key secret123 --rate-limit 50\n\n";
        std::cout << "  # Load from configuration file\n";
        std::cout << "  kolosal-server --config /path/to/config.yaml\n\n";
        std::cout << "  # Development mode with debug logging and metrics\n";
        std::cout << "  kolosal-server --log-level DEBUG --enable-access-log --enable-metrics\n\n";
    }

    void ServerConfig::printVersion()
    {
        std::cout << "Kolosal Server v1.0.0\n";
        std::cout << "A high-performance HTTP server for AI inference\n";
        std::cout << "Built with C++14, supports multiple models and authentication\n";
    }

} // namespace kolosal
