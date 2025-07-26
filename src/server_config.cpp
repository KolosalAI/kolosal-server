#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include "../../inference/include/inference_interface.h"
#ifdef _WIN32
#include <cstdlib>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace kolosal
{    bool ServerConfig::loadFromArgs(int argc, char *argv[])
    {
        // Automatically detect and load configuration files
        // Check for config files in this order: 
        // 1. System-wide installation (/etc/kolosal/config.yaml) - preferred for installed versions
        // 2. Local config directory (config/config.yaml, config/config.json) - for development
        // 3. User home directory (~/.kolosal/config.yaml)
        bool configLoaded = false;
        
        // First try system-wide config (installed version)
        std::ifstream systemFile("/etc/kolosal/config.yaml");
        if (systemFile.good()) {
            systemFile.close();
            if (loadFromFile("/etc/kolosal/config.yaml")) {
                std::cout << "Loaded configuration from /etc/kolosal/config.yaml" << std::endl;
                configLoaded = true;
            }
        }
        
        // If no system config found, try config.yaml in config directory (development)
        if (!configLoaded) {
            std::ifstream yamlFile("config/config.yaml");
            if (yamlFile.good()) {
                yamlFile.close();
                if (loadFromFile("config/config.yaml")) {
                    std::cout << "Loaded configuration from config/config.yaml" << std::endl;
                    configLoaded = true;
                }
            }
        }
        
        // If config.yaml not found, try config.json in config directory
        if (!configLoaded) {
            std::ifstream jsonFile("config/config.json");
            if (jsonFile.good()) {
                jsonFile.close();
                if (loadFromFile("config/config.json")) {
                    std::cout << "Loaded configuration from config/config.json" << std::endl;
                    configLoaded = true;
                }
            }
        }
        
        // If still no config found, try user home directory
        if (!configLoaded) {
#ifdef _WIN32
            char* homeDir = nullptr;
            size_t len = 0;
            // On Windows, use USERPROFILE instead of HOME
            if (_dupenv_s(&homeDir, &len, "USERPROFILE") == 0 && homeDir != nullptr) {
                std::string userConfigPath = std::string(homeDir) + "/.kolosal/config.yaml";
                free(homeDir);
#else
            const char* homeDir = getenv("HOME");
            if (homeDir) {
                std::string userConfigPath = std::string(homeDir) + "/.kolosal/config.yaml";
#endif
                std::ifstream userFile(userConfigPath);
                if (userFile.good()) {
                    userFile.close();
                    if (loadFromFile(userConfigPath)) {
                        std::cout << "Loaded configuration from " << userConfigPath << std::endl;
                        configLoaded = true;
                    }
                }
            }
        }
        
        if (!configLoaded) {
            std::cout << "No configuration file found, using default settings" << std::endl;
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
                if (!loadFromFile(argv[++i]))
                {
                    return false;
                }
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
                model.loadImmediately = true;
                models.push_back(model);
            }
            else if ((arg == "--model-lazy") && i + 2 < argc)
            {
                ModelConfig model;
                model.id = argv[++i];
                model.path = argv[++i];
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
            else if (arg == "--auto-save")
            {
                autoSaveEnabled = true;
            }
            else if (arg == "--no-auto-save")
            {
                autoSaveEnabled = false;
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

        return validate();
    }

    bool ServerConfig::loadFromFile(const std::string &configFile)
    {
        try
        {
            // First, validate that the file exists and is readable
            std::ifstream testFile(configFile);
            if (!testFile.is_open()) {
                std::cerr << "[Error] Cannot open config file: " << configFile << std::endl;
                return false;
            }
            testFile.close();

            // Validate YAML syntax before parsing
            try {
                YAML::LoadFile(configFile);
            } catch (const YAML::Exception& e) {
                std::cerr << "[Error] Invalid YAML syntax in config file " << configFile << ": " << e.what() << std::endl;
                
                // Try to load backup if it exists
                std::string backupPath = configFile + ".backup";
                std::ifstream backupFile(backupPath);
                if (backupFile.is_open()) {
                    backupFile.close();
                    std::cerr << "[Info] Attempting to load from backup: " << backupPath << std::endl;
                    try {
                        YAML::LoadFile(backupPath);
                        // If backup is valid, suggest using it
                        std::cerr << "[Info] Backup file is valid. Consider restoring from backup." << std::endl;
                    } catch (const YAML::Exception& backupError) {
                        std::cerr << "[Error] Backup file is also invalid: " << backupError.what() << std::endl;
                    }
                }
                return false;
            }

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
                    auto qdrant = databaseConfig["qdrant"];
                    if (qdrant["enabled"])
                        database.qdrant.enabled = qdrant["enabled"].as<bool>();
                    if (qdrant["host"])
                        database.qdrant.host = qdrant["host"].as<std::string>();
                    if (qdrant["port"])
                        database.qdrant.port = qdrant["port"].as<int>();
                    if (qdrant["collection_name"])
                        database.qdrant.collectionName = qdrant["collection_name"].as<std::string>();
                    if (qdrant["default_embedding_model"])
                        database.qdrant.defaultEmbeddingModel = qdrant["default_embedding_model"].as<std::string>();
                    if (qdrant["timeout"])
                        database.qdrant.timeout = qdrant["timeout"].as<int>();
                    if (qdrant["api_key"])
                        database.qdrant.apiKey = qdrant["api_key"].as<std::string>();
                    if (qdrant["max_connections"])
                        database.qdrant.maxConnections = qdrant["max_connections"].as<int>();
                    if (qdrant["connection_timeout"])
                        database.qdrant.connectionTimeout = qdrant["connection_timeout"].as<int>();
                }
                
                // Load document management configuration
                if (databaseConfig["document_management"])
                {
                    auto docMgmt = databaseConfig["document_management"];
                    if (docMgmt["enabled"])
                        database.documentManagement.enabled = docMgmt["enabled"].as<bool>();
                    if (docMgmt["default_collection"])
                        database.documentManagement.defaultCollection = docMgmt["default_collection"].as<std::string>();
                    
                    // Processing configuration
                    if (docMgmt["processing"])
                    {
                        auto processing = docMgmt["processing"];
                        if (processing["max_file_size_mb"])
                            database.documentManagement.processing.maxFileSizeMb = processing["max_file_size_mb"].as<int>();
                        if (processing["max_batch_size"])
                            database.documentManagement.processing.maxBatchSize = processing["max_batch_size"].as<int>();
                        
                        // PDF parsing
                        if (processing["pdf_parsing"])
                        {
                            auto pdfParsing = processing["pdf_parsing"];
                            if (pdfParsing["enabled"])
                                database.documentManagement.processing.pdfParsing.enabled = pdfParsing["enabled"].as<bool>();
                            if (pdfParsing["max_pages"])
                                database.documentManagement.processing.pdfParsing.maxPages = pdfParsing["max_pages"].as<int>();
                            if (pdfParsing["extract_images"])
                                database.documentManagement.processing.pdfParsing.extractImages = pdfParsing["extract_images"].as<bool>();
                            if (pdfParsing["extract_tables"])
                                database.documentManagement.processing.pdfParsing.extractTables = pdfParsing["extract_tables"].as<bool>();
                        }
                        
                        // DOCX parsing
                        if (processing["docx_parsing"])
                        {
                            auto docxParsing = processing["docx_parsing"];
                            if (docxParsing["enabled"])
                                database.documentManagement.processing.docxParsing.enabled = docxParsing["enabled"].as<bool>();
                            if (docxParsing["preserve_formatting"])
                                database.documentManagement.processing.docxParsing.preserveFormatting = docxParsing["preserve_formatting"].as<bool>();
                            if (docxParsing["extract_images"])
                                database.documentManagement.processing.docxParsing.extractImages = docxParsing["extract_images"].as<bool>();
                            if (docxParsing["extract_tables"])
                                database.documentManagement.processing.docxParsing.extractTables = docxParsing["extract_tables"].as<bool>();
                        }
                        
                        // Text preprocessing
                        if (processing["text_preprocessing"])
                        {
                            auto textPreproc = processing["text_preprocessing"];
                            if (textPreproc["min_text_length"])
                                database.documentManagement.processing.textPreprocessing.minTextLength = textPreproc["min_text_length"].as<int>();
                            if (textPreproc["max_chunk_size"])
                                database.documentManagement.processing.textPreprocessing.maxChunkSize = textPreproc["max_chunk_size"].as<int>();
                            if (textPreproc["remove_extra_whitespace"])
                                database.documentManagement.processing.textPreprocessing.removeExtraWhitespace = textPreproc["remove_extra_whitespace"].as<bool>();
                            if (textPreproc["normalize_unicode"])
                                database.documentManagement.processing.textPreprocessing.normalizeUnicode = textPreproc["normalize_unicode"].as<bool>();
                        }
                    }
                    
                    // Embedding configuration
                    if (docMgmt["embedding"])
                    {
                        auto embedding = docMgmt["embedding"];
                        if (embedding["chunk_overlap"])
                            database.documentManagement.embedding.chunkOverlap = embedding["chunk_overlap"].as<int>();
                        if (embedding["embedding_batch_size"])
                            database.documentManagement.embedding.embeddingBatchSize = embedding["embedding_batch_size"].as<int>();
                        if (embedding["max_retries"])
                            database.documentManagement.embedding.maxRetries = embedding["max_retries"].as<int>();
                        if (embedding["retry_delay_ms"])
                            database.documentManagement.embedding.retryDelayMs = embedding["retry_delay_ms"].as<int>();
                    }
                    
                    // Storage configuration
                    if (docMgmt["storage"])
                    {
                        auto storage = docMgmt["storage"];
                        if (storage["auto_generate_ids"])
                            database.documentManagement.storage.autoGenerateIds = storage["auto_generate_ids"].as<bool>();
                        if (storage["id_strategy"])
                            database.documentManagement.storage.idStrategy = storage["id_strategy"].as<std::string>();
                        if (storage["store_metadata"])
                            database.documentManagement.storage.storeMetadata = storage["store_metadata"].as<bool>();
                        if (storage["max_metadata_size"])
                            database.documentManagement.storage.maxMetadataSize = storage["max_metadata_size"].as<int>();
                    }
                    
                    // Search configuration
                    if (docMgmt["search"])
                    {
                        auto search = docMgmt["search"];
                        if (search["default_k"])
                            database.documentManagement.search.defaultK = search["default_k"].as<int>();
                        if (search["default_score_threshold"])
                            database.documentManagement.search.defaultScoreThreshold = search["default_score_threshold"].as<float>();
                        if (search["max_k"])
                            database.documentManagement.search.maxK = search["max_k"].as<int>();
                        if (search["enable_reranking"])
                            database.documentManagement.search.enableReranking = search["enable_reranking"].as<bool>();
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

            // Load inference engines configuration
            if (config["inference_engines"])
            {
                auto enginesConfig = config["inference_engines"];
                if (enginesConfig.IsSequence())
                {
                    for (const auto& engineConfig : enginesConfig)
                    {
                        InferenceEngineConfig engine;
                        if (engineConfig["name"])
                            engine.name = engineConfig["name"].as<std::string>();
                        if (engineConfig["library_path"])
                            engine.library_path = makeAbsolutePath(engineConfig["library_path"].as<std::string>());
                        if (engineConfig["version"])
                            engine.version = engineConfig["version"].as<std::string>();
                        if (engineConfig["description"])
                            engine.description = engineConfig["description"].as<std::string>();
                        if (engineConfig["load_on_startup"])
                            engine.load_on_startup = engineConfig["load_on_startup"].as<bool>();

                        if (!engine.name.empty() && !engine.library_path.empty())
                        {
                            inferenceEngines.push_back(engine);
                        }
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

            // Load agent system configuration
            try {
                agentSystem = agents::SystemConfig::from_yaml(config);
            } catch (const std::exception& e) {
                std::cerr << "[Warning] Failed to load agent system configuration: " << e.what() << std::endl;
                std::cerr << "[Info] Agent system will be disabled" << std::endl;
                // Continue without agent system - this is not a fatal error
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

            // Store the config file path for future saves
            currentConfigFilePath = configFile;
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

            // Models
            for (const auto &model : models)
            {
                YAML::Node modelNode;
                modelNode["id"] = model.id;
                modelNode["path"] = model.path;
                modelNode["type"] = model.type;
                modelNode["load_immediately"] = model.loadImmediately;
                modelNode["main_gpu_id"] = model.mainGpuId;
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
                engineNode["library_path"] = engine.library_path;
                engineNode["version"] = engine.version;
                engineNode["description"] = engine.description;
                engineNode["load_on_startup"] = engine.load_on_startup;
                config["inference_engines"].push_back(engineNode);
            }

            // Feature flags
            config["features"]["health_check"] = enableHealthCheck;
            config["features"]["metrics"] = enableMetrics;
            
            // Agent system configuration
            try {
                YAML::Node agentSystemNode = agentSystem.to_yaml();
                if (agentSystemNode.size() > 0) {
                    // Only add agent system sections if they exist
                    if (agentSystemNode["system"]) config["system"] = agentSystemNode["system"];
                    if (agentSystemNode["functions"]) config["functions"] = agentSystemNode["functions"];
                    if (agentSystemNode["agents"]) config["agents"] = agentSystemNode["agents"];
                    // Note: inference_engines in agent config are different from server inference_engines
                    // We may want to merge them or keep them separate based on requirements
                }
            } catch (const std::exception& e) {
                std::cerr << "[Warning] Failed to save agent system configuration: " << e.what() << std::endl;
            }

            std::ofstream file(configFile);
            if (!file.is_open())
            {
                std::cerr << "Error: Cannot create config file: " << configFile << std::endl;
                return false;
            }

            // Validate the generated YAML before writing
            std::stringstream yamlStream;
            yamlStream << config;
            std::string yamlContent = yamlStream.str();
            
            // Try to parse it back to validate structure
            try {
                YAML::Load(yamlContent);
            } catch (const YAML::Exception& e) {
                std::cerr << "[Error] Generated YAML is invalid: " << e.what() << std::endl;
                file.close();
                return false;
            }

            file << yamlContent;
            file.close();
            
            // Verify the file was written successfully by trying to read it back
            std::ifstream verifyFile(configFile);
            if (!verifyFile.is_open()) {
                std::cerr << "[Error] Failed to verify saved config file: " << configFile << std::endl;
                return false;
            }
            verifyFile.close();

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

        std::cout << "  Configuration Management:\n";
        std::cout << "    --auto-save               Enable automatic saving of configuration changes\n";
        std::cout << "    --no-auto-save            Disable automatic saving (default)\n\n";

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

    bool ServerConfig::saveToCurrentFile() const
    {
        if (currentConfigFilePath.empty()) {
            std::cerr << "[Error] No current config file path set - cannot save configuration" << std::endl;
            return false;
        }
        
        // Create a backup of the current config before saving
        try {
            std::string backupPath = currentConfigFilePath + ".backup";
            std::ifstream src(currentConfigFilePath, std::ios::binary);
            std::ofstream dst(backupPath, std::ios::binary);
            if (src && dst) {
                dst << src.rdbuf();
                src.close();
                dst.close();
            }
        } catch (const std::exception& e) {
            std::cerr << "[Warning] Failed to create config backup: " << e.what() << std::endl;
        }
        
        return saveToFile(currentConfigFilePath);
    }

    const std::string& ServerConfig::getCurrentConfigFilePath() const
    {
        return currentConfigFilePath;
    }

    void ServerConfig::setAutoSaveEnabled(bool enabled)
    {
        autoSaveEnabled = enabled;
        if (enabled) {
            std::cout << "Auto-save enabled: Configuration changes will be automatically saved to file" << std::endl;
        } else {
            std::cout << "Auto-save disabled: Configuration changes will not be automatically saved to file" << std::endl;
        }
    }

    bool ServerConfig::isAutoSaveEnabled() const
    {
        return autoSaveEnabled;
    }

    std::string ServerConfig::makeAbsolutePath(const std::string& path)
    {
        if (path.empty()) {
            return path;
        }

        // Check if path is already absolute
#ifdef _WIN32
        if (path.length() >= 2 && path[1] == ':') {
            return path; // Already absolute on Windows (e.g., C:\path)
        }
        if (path.length() >= 2 && (path[0] == '\\' || path[0] == '/')) {
            return path; // UNC path or root path
        }
#else
        if (path[0] == '/') {
            return path; // Already absolute on Unix-like systems
        }
#endif

        // Convert relative path to absolute
        try {
#ifdef _WIN32
            char buffer[MAX_PATH];
            if (GetFullPathNameA(path.c_str(), MAX_PATH, buffer, nullptr) != 0) {
                return std::string(buffer);
            }
#endif
            
            // Fallback: get current working directory and append path
            char cwd[1024];
#ifdef _WIN32
            if (GetCurrentDirectoryA(sizeof(cwd), cwd) != 0) {
#else
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
#endif
                std::string result = std::string(cwd);
#ifdef _WIN32
                if (result.back() != '\\') result += '\\';
#else
                if (result.back() != '/') result += '/';
#endif
                result += path;
                return result;
            }
        } catch (...) {
            // If all else fails, return the original path
        }
        
        return path;
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

} // namespace kolosal
