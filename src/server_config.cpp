#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#include <limits.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace kolosal
{
    /**
     * @brief Convert a relative path to an absolute path with fallback to executable directory
     * @param path The path to convert (can be relative or already absolute)
     * @return Absolute path as string
     */
    /*static*/ std::string ServerConfig::makeAbsolutePath(const std::string& path) {
        if (path.empty()) {
            return path;
        }
        
        try {
            std::filesystem::path fsPath(path);
            
            // If already absolute, return as-is
            if (fsPath.is_absolute()) {
                return fsPath.string();
            }
            
            // First try: Convert to absolute path relative to current working directory
            std::filesystem::path absolutePath = std::filesystem::absolute(fsPath);
            if (std::filesystem::exists(absolutePath)) {
                return absolutePath.string();
            }
            
            // Second try: Check relative to the executable directory
            try {
                // Get the executable path
                std::filesystem::path executablePath;
                
#ifdef _WIN32
                char execPath[MAX_PATH];
                if (GetModuleFileNameA(NULL, execPath, MAX_PATH) != 0) {
                    executablePath = std::filesystem::path(execPath).parent_path();
                }
#elif defined(__APPLE__)
                // On macOS, use _NSGetExecutablePath
                uint32_t size = PATH_MAX;
                char execPath[PATH_MAX];
                if (_NSGetExecutablePath(execPath, &size) == 0) {
                    // Resolve any symlinks
                    char resolvedPath[PATH_MAX];
                    if (realpath(execPath, resolvedPath) != nullptr) {
                        executablePath = std::filesystem::path(resolvedPath).parent_path();
                    } else {
                        executablePath = std::filesystem::path(execPath).parent_path();
                    }
                }
#else
                // On Linux, use /proc/self/exe
                char execPath[PATH_MAX];
                ssize_t len = readlink("/proc/self/exe", execPath, sizeof(execPath) - 1);
                if (len != -1) {
                    execPath[len] = '\0';
                    executablePath = std::filesystem::path(execPath).parent_path();
                }
#endif
                
                if (!executablePath.empty()) {
                    std::filesystem::path execRelativePath = executablePath / fsPath;
                    if (std::filesystem::exists(execRelativePath)) {
                        ServerLogger::instance().info("Found path relative to executable: " + execRelativePath.string());
                        return execRelativePath.string();
                    }
                }
            } catch (const std::exception& e) {
                ServerLogger::instance().info("Failed to get executable path: " + std::string(e.what()));
            }
            
            // If file doesn't exist in either location, return the absolute path anyway
            // (it might be created later or be a URL)
            return absolutePath.string();
            
        } catch (const std::filesystem::filesystem_error& e) {
            // If filesystem operations fail, return the original path
            ServerLogger::instance().info("Warning: Failed to convert path to absolute: " + path + " - " + e.what());
            return path;
        }
    }

    bool ServerConfig::loadFromArgs(int argc, char *argv[])
    {
        // Automatically detect and load configuration files
        // Check for config files in this order: 
        // 1. System-wide installation (/etc/kolosal/config.yaml) - preferred for installed versions
        // 2. Local working directory (config.yaml, config.json) - for development
        // 3. User home directory (~/.kolosal/config.yaml)
        bool configLoaded = false;
        
        // First try system-wide config (installed version)
#ifdef __APPLE__
        // On macOS, check /usr/local/etc for Homebrew installations and /etc for system-wide
        std::vector<std::string> systemPaths = {
            "/usr/local/etc/kolosal/config.yaml",  // Homebrew/user installed
            "/etc/kolosal/config.yaml"             // System-wide
        };
        
        for (const auto& systemPath : systemPaths) {
            std::ifstream systemFile(systemPath);
            if (systemFile.good()) {
                systemFile.close();
                if (loadFromFile(systemPath)) {
                    ServerLogger::instance().info("Loaded configuration from " + systemPath);
                    currentConfigFilePath = systemPath;
                    ServerLogger::instance().info("Stored config file path: " + currentConfigFilePath);
                    ServerLogger::instance().info("ServerConfig instance address during load: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
                    configLoaded = true;
                    break;
                }
            }
        }
#else
        // On Linux, use standard system path
        std::ifstream systemFile("/etc/kolosal/config.yaml");
        if (systemFile.good()) {
            systemFile.close();
            if (loadFromFile("/etc/kolosal/config.yaml")) {
                ServerLogger::instance().info("Loaded configuration from /etc/kolosal/config.yaml");
                currentConfigFilePath = "/etc/kolosal/config.yaml"; // Store exact path used
                ServerLogger::instance().info("Stored config file path: " + currentConfigFilePath);
                ServerLogger::instance().info("ServerConfig instance address during load: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
                configLoaded = true;
            }
        }
#endif
        
        // If no system config found, try config.yaml in working directory (development)
        if (!configLoaded) {
            std::ifstream yamlFile("config.yaml");
            if (yamlFile.good()) {
                yamlFile.close();
                if (loadFromFile("config.yaml")) {
                    ServerLogger::instance().info("Loaded configuration from config.yaml");
                    // For relative paths, store the absolute path to ensure we can always find it later
                    currentConfigFilePath = std::filesystem::absolute("config.yaml").string();
                    ServerLogger::instance().info("Stored config file path: " + currentConfigFilePath);
                    ServerLogger::instance().info("ServerConfig instance address during load: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
                    configLoaded = true;
                }
            }
        }
        
        // If config.yaml not found, try config.json in working directory
        if (!configLoaded) {
            std::ifstream jsonFile("config.json");
            if (jsonFile.good()) {
                jsonFile.close();
                if (loadFromFile("config.json")) {
                    ServerLogger::instance().info("Loaded configuration from config.json");
                    // For relative paths, store the absolute path to ensure we can always find it later
                    currentConfigFilePath = std::filesystem::absolute("config.json").string();
                    ServerLogger::instance().info("Stored config file path: " + currentConfigFilePath);
                    ServerLogger::instance().info("ServerConfig instance address during load: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
                    configLoaded = true;
                }
            }
        }
        
        // If still no config found, try user home directory
        if (!configLoaded) {
            std::string userConfigPath;
            
#ifdef _WIN32
            // On Windows, try user's AppData\Roaming directory first
            char* userProfile = nullptr;
            size_t len = 0;
            if (_dupenv_s(&userProfile, &len, "USERPROFILE") == 0 && userProfile != nullptr) {
                userConfigPath = std::string(userProfile) + "\\AppData\\Roaming\\Kolosal\\config.yaml";
                free(userProfile);
                
                std::ifstream userFile(userConfigPath);
                if (userFile.good()) {
                    userFile.close();
                    if (loadFromFile(userConfigPath)) {
                        ServerLogger::instance().info("Loaded configuration from " + userConfigPath);
                        currentConfigFilePath = userConfigPath; // Store exact path used
                        ServerLogger::instance().info("Stored config file path: " + currentConfigFilePath);
                        ServerLogger::instance().info("ServerConfig instance address during load: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
                        configLoaded = true;
                    }
                }
            }
#elif defined(__APPLE__)
            // On macOS, use standard Application Support directory
            const char* homeDir = getenv("HOME");
            if (homeDir) {
                userConfigPath = std::string(homeDir) + "/Library/Application Support/Kolosal/config.yaml";
                std::ifstream userFile(userConfigPath);
                if (userFile.good()) {
                    userFile.close();
                    if (loadFromFile(userConfigPath)) {
                        ServerLogger::instance().info("Loaded configuration from " + userConfigPath);
                        currentConfigFilePath = userConfigPath; // Store exact path used
                        ServerLogger::instance().info("Stored config file path: " + currentConfigFilePath);
                        ServerLogger::instance().info("ServerConfig instance address during load: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
                        configLoaded = true;
                    }
                }
            }
#else
            // On Unix/Linux systems
            const char* homeDir = getenv("HOME");
            if (homeDir) {
                userConfigPath = std::string(homeDir) + "/.kolosal/config.yaml";
                std::ifstream userFile(userConfigPath);
                if (userFile.good()) {
                    userFile.close();
                    if (loadFromFile(userConfigPath)) {
                        ServerLogger::instance().info("Loaded configuration from " + userConfigPath);
                        currentConfigFilePath = userConfigPath; // Store exact path used
                        ServerLogger::instance().info("Stored config file path: " + currentConfigFilePath);
                        ServerLogger::instance().info("ServerConfig instance address during load: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
                        configLoaded = true;
                    }
                }
            }
#endif
        }
        
        if (!configLoaded) {
            ServerLogger::instance().info("No configuration file found, using default settings");
        }

        // Process command line arguments (they can override config file settings)
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
                std::string configFile = argv[++i];
                if (!loadFromFile(configFile))
                {
                    return false;
                }
                // Handle both relative and absolute paths from command line
                if (std::filesystem::path(configFile).is_absolute()) {
                    currentConfigFilePath = configFile; // Store exact absolute path
                } else {
                    currentConfigFilePath = std::filesystem::absolute(configFile).string(); // Convert relative to absolute
                }
                ServerLogger::instance().info("Loaded configuration from " + configFile);
                ServerLogger::instance().info("Stored config file path: " + currentConfigFilePath);
                ServerLogger::instance().info("ServerConfig instance address during load: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
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
                model.path = ServerConfig::makeAbsolutePath(argv[++i]);
                model.loadImmediately = true;
                models.push_back(model);
            }
            else if ((arg == "--model-lazy") && i + 2 < argc)
            {
                ModelConfig model;
                model.id = argv[++i];
                model.path = ServerConfig::makeAbsolutePath(argv[++i]);
                model.loadImmediately = false;
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
            else if ((arg == "--idle-timeout") && i + 1 < argc)
            {
                idleTimeout = std::chrono::seconds(std::stoi(argv[++i]));
            }
            // Feature flags
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
            
            // Search options
            else if (arg == "--enable-search")
            {
                search.enabled = true;
            }
            else if (arg == "--disable-search")
            {
                search.enabled = false;
            }
            else if ((arg == "--search-url" || arg == "--searxng-url") && i + 1 < argc)
            {
                search.searxng_url = argv[++i];
            }
            else if ((arg == "--search-timeout") && i + 1 < argc)
            {
                search.timeout = std::stoi(argv[++i]);
            }
            else if ((arg == "--search-max-results") && i + 1 < argc)
            {
                search.max_results = std::stoi(argv[++i]);
            }
            else if ((arg == "--search-engine") && i + 1 < argc)
            {
                search.default_engine = argv[++i];
            }
            else if ((arg == "--search-api-key") && i + 1 < argc)
            {
                search.api_key = argv[++i];
            }
            else if ((arg == "--search-language") && i + 1 < argc)
            {
                search.default_language = argv[++i];
            }
            else if ((arg == "--search-category") && i + 1 < argc)
            {
                search.default_category = argv[++i];
            }
            else if (arg == "--search-safe-search")
            {
                search.enable_safe_search = true;
            }
            else if (arg == "--no-search-safe-search")
            {
                search.enable_safe_search = false;
            }
            
            // Help and version
            else if (arg == "-h" || arg == "--help")
            {
                printHelp();
                helpOrVersionShown = true;
                return false;
            }
            else if (arg == "-v" || arg == "--version")
            {
                printVersion();
                helpOrVersionShown = true;
                return false;
            }
            else if (arg.front() == '-')
            {
                std::cerr << "Unknown option: " << arg << std::endl;
                return false;
            }
        }

        // Apply default inference engine to models that don't have one specified
        if (!defaultInferenceEngine.empty())
        {
            for (auto &model : models)
            {
                if (model.inferenceEngine.empty() || model.inferenceEngine == "llama-cpu")
                {
                    model.inferenceEngine = defaultInferenceEngine;
                }
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
            }            // Load logging settings
            if (config["logging"])
            {
                auto logging = config["logging"];
                if (logging["level"])
                    logLevel = logging["level"].as<std::string>();
                if (logging["file"])
                    logFile = logging["file"].as<std::string>();
                if (logging["access_log"])
                    enableAccessLog = logging["access_log"].as<bool>();
                if (logging["quiet_mode"])
                    quietMode = logging["quiet_mode"].as<bool>();
                if (logging["show_request_details"])
                    showRequestDetails = logging["show_request_details"].as<bool>();
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

            // Load search configuration
            if (config["search"])
            {
                auto searchConfig = config["search"];
                if (searchConfig["enabled"])
                    search.enabled = searchConfig["enabled"].as<bool>();
                if (searchConfig["searxng_url"])
                    search.searxng_url = searchConfig["searxng_url"].as<std::string>();
                if (searchConfig["timeout"])
                    search.timeout = searchConfig["timeout"].as<int>();
                if (searchConfig["max_results"])
                    search.max_results = searchConfig["max_results"].as<int>();
                if (searchConfig["default_engine"])
                    search.default_engine = searchConfig["default_engine"].as<std::string>();
                if (searchConfig["api_key"])
                    search.api_key = searchConfig["api_key"].as<std::string>();
                if (searchConfig["enable_safe_search"])
                    search.enable_safe_search = searchConfig["enable_safe_search"].as<bool>();
                if (searchConfig["default_format"])
                    search.default_format = searchConfig["default_format"].as<std::string>();
                if (searchConfig["default_language"])
                    search.default_language = searchConfig["default_language"].as<std::string>();
                if (searchConfig["default_category"])
                    search.default_category = searchConfig["default_category"].as<std::string>();
            }

            // Load database configuration
            if (config["database"])
            {
                auto databaseConfig = config["database"];
                if (databaseConfig["qdrant"])
                {
                    auto qdrantConfig = databaseConfig["qdrant"];
                    if (qdrantConfig["enabled"])
                        database.qdrant.enabled = qdrantConfig["enabled"].as<bool>();
                    if (qdrantConfig["host"])
                        database.qdrant.host = qdrantConfig["host"].as<std::string>();
                    if (qdrantConfig["port"])
                        database.qdrant.port = qdrantConfig["port"].as<int>();
                    if (qdrantConfig["collection_name"])
                        database.qdrant.collectionName = qdrantConfig["collection_name"].as<std::string>();
                    if (qdrantConfig["default_embedding_model"])
                        database.qdrant.defaultEmbeddingModel = qdrantConfig["default_embedding_model"].as<std::string>();
                    if (qdrantConfig["timeout"])
                        database.qdrant.timeout = qdrantConfig["timeout"].as<int>();
                    if (qdrantConfig["api_key"])
                        database.qdrant.apiKey = qdrantConfig["api_key"].as<std::string>();
                    if (qdrantConfig["max_connections"])
                        database.qdrant.maxConnections = qdrantConfig["max_connections"].as<int>();
                    if (qdrantConfig["connection_timeout"])
                        database.qdrant.connectionTimeout = qdrantConfig["connection_timeout"].as<int>();
                    if (qdrantConfig["embedding_batch_size"])
                        database.qdrant.embeddingBatchSize = qdrantConfig["embedding_batch_size"].as<int>();
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
                        model.path = ServerConfig::makeAbsolutePath(modelConfig["path"].as<std::string>());
                    if (modelConfig["type"])
                        model.type = modelConfig["type"].as<std::string>();
                    // Support both new and old field names for backward compatibility
                    if (modelConfig["load_immediately"])
                        model.loadImmediately = modelConfig["load_immediately"].as<bool>();
                    else if (modelConfig["load_at_startup"])
                        model.loadImmediately = modelConfig["load_at_startup"].as<bool>();
                    if (modelConfig["main_gpu_id"])
                        model.mainGpuId = modelConfig["main_gpu_id"].as<int>();
                    if (modelConfig["inference_engine"])
                        model.inferenceEngine = modelConfig["inference_engine"].as<std::string>();
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
                        if (params["warmup"])
                            model.loadParams.warmup = params["warmup"].as<bool>();
                        if (params["n_gpu_layers"])
                            model.loadParams.n_gpu_layers = params["n_gpu_layers"].as<int>();
                        if (params["n_batch"])
                            model.loadParams.n_batch = params["n_batch"].as<int>();
                        if (params["n_ubatch"])
                            model.loadParams.n_ubatch = params["n_ubatch"].as<int>();
                    }

                    models.push_back(model);
                }
            }

            // Load inference engines
            if (config["inference_engines"])
            {
                inferenceEngines.clear();
                for (const auto &engineConfig : config["inference_engines"])
                {
                    InferenceEngineConfig engine;
                    if (engineConfig["name"])
                        engine.name = engineConfig["name"].as<std::string>();
                    if (engineConfig["library_path"])
                        engine.library_path = ServerConfig::makeAbsolutePath(engineConfig["library_path"].as<std::string>());
                    if (engineConfig["version"])
                        engine.version = engineConfig["version"].as<std::string>();
                    if (engineConfig["description"])
                        engine.description = engineConfig["description"].as<std::string>();
                    if (engineConfig["load_on_startup"])
                        engine.load_on_startup = engineConfig["load_on_startup"].as<bool>();

                    // Only add engines with valid name and library path
                    if (!engine.name.empty() && !engine.library_path.empty())
                    {
                        inferenceEngines.push_back(engine);
                    }
                }
            }

            // Load default inference engine
            if (config["default_inference_engine"])
            {
                defaultInferenceEngine = config["default_inference_engine"].as<std::string>();
            }

            // Apply default inference engine to models that don't have one specified
            if (!defaultInferenceEngine.empty())
            {
                for (auto &model : models)
                {
                    if (model.inferenceEngine.empty() || model.inferenceEngine == "llama-cpu")
                    {
                        model.inferenceEngine = defaultInferenceEngine;
                    }
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

            // Load search configuration
            if (config["search"])
            {
                auto searchConfig = config["search"];
                if (searchConfig["enabled"])
                    search.enabled = searchConfig["enabled"].as<bool>();
                if (searchConfig["searxng_url"])
                    search.searxng_url = searchConfig["searxng_url"].as<std::string>();
                if (searchConfig["timeout"])
                    search.timeout = searchConfig["timeout"].as<int>();
                if (searchConfig["max_results"])
                    search.max_results = searchConfig["max_results"].as<int>();
                if (searchConfig["default_format"])
                    search.default_format = searchConfig["default_format"].as<std::string>();
                if (searchConfig["default_language"])
                    search.default_language = searchConfig["default_language"].as<std::string>();
                if (searchConfig["default_category"])
                    search.default_category = searchConfig["default_category"].as<std::string>();
                if (searchConfig["default_engine"])
                    search.default_engine = searchConfig["default_engine"].as<std::string>();
                if (searchConfig["api_key"])
                    search.api_key = searchConfig["api_key"].as<std::string>();
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
            config["server"]["idle_timeout"] = static_cast<int>(idleTimeout.count());
            config["server"]["allow_public_access"] = allowPublicAccess;
            config["server"]["allow_internet_access"] = allowInternetAccess;            // Logging settings
            config["logging"]["level"] = logLevel;
            config["logging"]["file"] = logFile;
            config["logging"]["access_log"] = enableAccessLog;
            config["logging"]["quiet_mode"] = quietMode;
            config["logging"]["show_request_details"] = showRequestDetails;

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
            config["auth"]["cors"]["max_age"] = auth.cors.maxAge;            config["auth"]["cors"]["allowed_origins"] = auth.cors.allowedOrigins;
            config["auth"]["cors"]["allowed_methods"] = auth.cors.allowedMethods;
            config["auth"]["cors"]["allowed_headers"] = auth.cors.allowedHeaders;
            
            // Search configuration
            config["search"]["enabled"] = search.enabled;
            config["search"]["searxng_url"] = search.searxng_url;
            config["search"]["timeout"] = search.timeout;
            config["search"]["max_results"] = search.max_results;
            config["search"]["default_engine"] = search.default_engine;
            config["search"]["api_key"] = search.api_key;
            config["search"]["enable_safe_search"] = search.enable_safe_search;
            config["search"]["default_format"] = search.default_format;
            config["search"]["default_language"] = search.default_language;
            config["search"]["default_category"] = search.default_category;

            // Database configuration
            config["database"]["qdrant"]["enabled"] = database.qdrant.enabled;
            config["database"]["qdrant"]["host"] = database.qdrant.host;
            config["database"]["qdrant"]["port"] = database.qdrant.port;
            config["database"]["qdrant"]["collection_name"] = database.qdrant.collectionName;
            config["database"]["qdrant"]["default_embedding_model"] = database.qdrant.defaultEmbeddingModel;
            config["database"]["qdrant"]["timeout"] = database.qdrant.timeout;
            config["database"]["qdrant"]["api_key"] = database.qdrant.apiKey;
            config["database"]["qdrant"]["max_connections"] = database.qdrant.maxConnections;
            config["database"]["qdrant"]["connection_timeout"] = database.qdrant.connectionTimeout;
            config["database"]["qdrant"]["embedding_batch_size"] = database.qdrant.embeddingBatchSize;

            // Models
            for (const auto &model : models)
            {
                YAML::Node modelNode;
                modelNode["id"] = model.id;
                modelNode["path"] = ServerConfig::makeAbsolutePath(model.path);  // Convert to absolute path
                modelNode["type"] = model.type;
                modelNode["load_immediately"] = model.loadImmediately;
                modelNode["main_gpu_id"] = model.mainGpuId;
                modelNode["inference_engine"] = model.inferenceEngine;
                modelNode["load_params"]["n_ctx"] = model.loadParams.n_ctx;
                modelNode["load_params"]["n_keep"] = model.loadParams.n_keep;
                modelNode["load_params"]["use_mmap"] = model.loadParams.use_mmap;
                modelNode["load_params"]["use_mlock"] = model.loadParams.use_mlock;
                modelNode["load_params"]["n_parallel"] = model.loadParams.n_parallel;
                modelNode["load_params"]["cont_batching"] = model.loadParams.cont_batching;
                modelNode["load_params"]["warmup"] = model.loadParams.warmup;
                modelNode["load_params"]["n_gpu_layers"] = model.loadParams.n_gpu_layers;
                modelNode["load_params"]["n_batch"] = model.loadParams.n_batch;
                modelNode["load_params"]["n_ubatch"] = model.loadParams.n_ubatch;
                config["models"].push_back(modelNode);
            }

            // Inference engines
            for (const auto &engine : inferenceEngines)
            {
                YAML::Node engineNode;
                engineNode["name"] = engine.name;
                engineNode["library_path"] = ServerConfig::makeAbsolutePath(engine.library_path);  // Convert to absolute path
                engineNode["version"] = engine.version;
                engineNode["description"] = engine.description;
                engineNode["load_on_startup"] = engine.load_on_startup;
                config["inference_engines"].push_back(engineNode);
            }

            // Default inference engine
            if (!defaultInferenceEngine.empty())
            {
                config["default_inference_engine"] = defaultInferenceEngine;
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
        catch (const std::exception &)
        {
            std::cerr << "Error: Invalid port number: " << port << std::endl;
            return false;
        }        // Validate log level
        if (logLevel != "DEBUG" && logLevel != "INFO" && logLevel != "WARN" && logLevel != "WARNING" && logLevel != "ERROR")
        {
            std::cerr << "Error: Invalid log level: " << logLevel << std::endl;
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
            
            // Validate loading parameters (consistent with API validation)
            if (model.loadParams.n_ctx <= 0 || model.loadParams.n_ctx > 1000000)
            {
                std::cerr << "Error: Invalid n_ctx for model " << model.id << ": must be between 1 and 1000000" << std::endl;
                return false;
            }
            
            if (model.loadParams.n_keep < 0 || model.loadParams.n_keep > model.loadParams.n_ctx)
            {
                std::cerr << "Error: Invalid n_keep for model " << model.id << ": must be between 0 and n_ctx (" << model.loadParams.n_ctx << ")" << std::endl;
                return false;
            }
            
            if (model.loadParams.n_batch <= 0 || model.loadParams.n_batch > 8192)
            {
                std::cerr << "Error: Invalid n_batch for model " << model.id << ": must be between 1 and 8192" << std::endl;
                return false;
            }
            
            if (model.loadParams.n_ubatch <= 0 || model.loadParams.n_ubatch > model.loadParams.n_batch)
            {
                std::cerr << "Error: Invalid n_ubatch for model " << model.id << ": must be between 1 and n_batch (" << model.loadParams.n_batch << ")" << std::endl;
                return false;
            }
            
            if (model.loadParams.n_parallel <= 0 || model.loadParams.n_parallel > 16)
            {
                std::cerr << "Error: Invalid n_parallel for model " << model.id << ": must be between 1 and 16" << std::endl;
                return false;
            }
            
            if (model.loadParams.n_gpu_layers < 0 || model.loadParams.n_gpu_layers > 1000)
            {
                std::cerr << "Error: Invalid n_gpu_layers for model " << model.id << ": must be between 0 and 1000" << std::endl;
                return false;
            }
            
            if (model.mainGpuId < -1 || model.mainGpuId > 15)
            {
                std::cerr << "Error: Invalid main_gpu_id for model " << model.id << ": must be between -1 and 15" << std::endl;
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

        std::cout << "\nInference Engines:" << std::endl;
        if (inferenceEngines.empty())
        {
            std::cout << "  No inference engines configured" << std::endl;
        }
        else
        {
            for (const auto &engine : inferenceEngines)
            {
                std::cout << "  " << engine.name << ":" << std::endl;
                std::cout << "    Library: " << engine.library_path << std::endl;
                std::cout << "    Version: " << engine.version << std::endl;
                std::cout << "    Description: " << engine.description << std::endl;
                std::cout << "    Load on startup: " << (engine.load_on_startup ? "Yes" : "No") << std::endl;
            }
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
                std::cout << "    Load immediately: " << (model.loadImmediately ? "Yes" : "No") << std::endl;
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
        std::cout << "    --idle-timeout SEC        Model idle timeout in seconds (default: 300)\n\n";

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

        std::cout << "  Internet Search:\n";
        std::cout << "    --enable-search           Enable internet search endpoint\n";
        std::cout << "    --disable-search          Disable internet search endpoint\n";
        std::cout << "    --search-url, --searxng-url URL  SearXNG instance URL (default: http://localhost:4000)\n";
        std::cout << "    --search-timeout SEC      Search request timeout in seconds (default: 30)\n";
        std::cout << "    --search-max-results N    Maximum number of search results (default: 20)\n";
        std::cout << "    --search-engine ENGINE    Default search engine\n";
        std::cout << "    --search-api-key KEY      API key for search service authentication\n";
        std::cout << "    --search-language LANG    Default search language (default: en)\n";
        std::cout << "    --search-category CAT     Default search category (default: general)\n";
        std::cout << "    --search-safe-search      Enable safe search (default: enabled)\n";
        std::cout << "    --no-search-safe-search   Disable safe search\n\n";
        std::cout << "    --search-engine NAME      Default search engine (e.g., google, bing)\n";
        std::cout << "    --search-api-key KEY      API key for search engine, if required\n";
        std::cout << "    --search-language LANG    Default language for search results\n";
        std::cout << "    --search-category CAT      Default category for search results\n";
        std::cout << "    --search-safe-search      Enable safe search (restrict explicit content)\n";
        std::cout << "    --no-search-safe-search   Disable safe search\n\n";

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

    ServerConfig& ServerConfig::getInstance()
    {
        static ServerConfig instance;
        return instance;
    }

    void ServerConfig::setInstance(const ServerConfig& config)
    {
        getInstance() = config;
    }

    bool ServerConfig::saveToCurrentFile() const
    {
        ServerLogger::instance().info("saveToCurrentFile called. currentConfigFilePath: '" + currentConfigFilePath + "'");
        ServerLogger::instance().info("ServerConfig instance address: " + std::to_string(reinterpret_cast<uintptr_t>(this)));
        
        if (currentConfigFilePath.empty())
        {
            ServerLogger::instance().info("currentConfigFilePath is empty, using fallback logic");
            
            // If no config file was loaded, try to determine a reasonable default location
            // based on the same priority order used during loading
            
            std::vector<std::string> candidatePaths;
            
#ifdef _WIN32
            // On Windows, try user's AppData\Roaming directory first (same as loading logic)
            char* userProfile = nullptr;
            size_t len = 0;
            if (_dupenv_s(&userProfile, &len, "USERPROFILE") == 0 && userProfile != nullptr) {
                std::string userConfigPath = std::string(userProfile) + "\\AppData\\Roaming\\Kolosal\\config.yaml";
                candidatePaths.push_back(userConfigPath);
                free(userProfile);
            }
#else
            // On Unix/Linux systems, try user home directory
            const char* homeDir = getenv("HOME");
            if (homeDir) {
                std::string userConfigPath = std::string(homeDir) + "/.kolosal/config.yaml";
                candidatePaths.push_back(userConfigPath);
            }
#endif
            
            // Then try current working directory
            candidatePaths.push_back("config.yaml");
            
            // Try each candidate path in order
            for (const auto& path : candidatePaths) {
                ServerLogger::instance().info("Trying fallback path: " + path);
                try {
                    // Create directory if it doesn't exist
                    std::filesystem::path filePath(path);
                    std::filesystem::path dirPath = filePath.parent_path();
                    
                    if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
                        std::filesystem::create_directories(dirPath);
                    }
                    
                    // Test if we can write to this location
                    std::ofstream testFile(path, std::ios::app);
                    if (testFile.is_open()) {
                        testFile.close();
                        ServerLogger::instance().info("No config file path available, using fallback: " + path);
                        return saveToFile(path);
                    }
                } catch (const std::exception& e) {
                    ServerLogger::instance().info("Cannot write to " + path + ": " + e.what());
                    continue;
                }
            }
            
            std::cerr << "Error: No config file path available for saving and cannot write to any fallback location. Use saveToFile() with explicit path instead." << std::endl;
            return false;
        }
        
        ServerLogger::instance().info("Saving to current config file: " + currentConfigFilePath);
        return saveToFile(currentConfigFilePath);
    }

    const std::string& ServerConfig::getCurrentConfigFilePath() const
    {
        return currentConfigFilePath;
    }

} // namespace kolosal
