#include "kolosal/node_manager.h"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp" // Assuming a logger is available
#include "kolosal/download_utils.hpp"
#include "kolosal/gpu_detection.hpp"
#include <filesystem>
#include <mutex>
#include <algorithm> // For std::max and std::min

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#define LIBRARY_EXTENSION ".dll"
#elif defined(__APPLE__)
#include <unistd.h>
#include <limits.h>
#include <mach-o/dyld.h>
#define LIBRARY_EXTENSION ".dylib"
#else
#include <unistd.h>
#include <limits.h>
#define LIBRARY_EXTENSION ".so"
#endif

namespace kolosal
{
    // Helper function to get the directory containing the current executable
    static std::string getExecutableDirectory()
    {
#ifdef _WIN32
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string execPath(path);
        return std::filesystem::path(execPath).parent_path().string();
#elif defined(__APPLE__)
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            char realPath[PATH_MAX];
            if (realpath(path, realPath) != NULL) {
                return std::filesystem::path(realPath).parent_path().string();
            }
        }
        // Fallback to current directory
        return std::filesystem::current_path().string();
#else
        char path[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
        if (count != -1) {
            path[count] = '\0';
            return std::filesystem::path(path).parent_path().string();
        }
        // Fallback to current directory
        return std::filesystem::current_path().string();
#endif
    }

#ifdef __APPLE__
    // Helper function to detect if we're running from a macOS app bundle
    static bool isRunningFromAppBundle()
    {
        try {
            std::string execDir = getExecutableDirectory();
            std::filesystem::path execPath(execDir);
            
            // Check if we're in a typical app bundle structure
            // App bundles have structure: App.app/Contents/MacOS/executable
            if (execPath.filename() == "MacOS") {
                auto contentsPath = execPath.parent_path();
                if (contentsPath.filename() == "Contents") {
                    auto appPath = contentsPath.parent_path();
                    if (appPath.extension() == ".app") {
                        return true;
                    }
                }
            }
            
            // Also check if we have the typical app bundle directories nearby
            auto frameworksPath = execPath / "../Frameworks";
            auto resourcesPath = execPath / "../Resources";
            if (std::filesystem::exists(frameworksPath) && 
                std::filesystem::exists(resourcesPath)) {
                return true;
            }
            
            return false;
        } catch (const std::exception& e) {
            ServerLogger::logWarning("Failed to detect app bundle structure: %s", e.what());
            return false;
        }
    }

    // Helper function to get app bundle-aware search paths for libraries
    static std::vector<std::string> getLibrarySearchPaths(const std::string& execDir, const std::string& libName)
    {
        std::vector<std::string> searchPaths;
        
        if (isRunningFromAppBundle()) {
            ServerLogger::logInfo("App bundle detected, prioritizing Frameworks directory");
            // Prioritize app bundle paths
            searchPaths.insert(searchPaths.end(), {
                execDir + "/../Frameworks/" + libName + std::string(LIBRARY_EXTENSION),
                execDir + "/../lib/" + libName + std::string(LIBRARY_EXTENSION)
            });
        }
        
        // Add standard app installation paths
        searchPaths.insert(searchPaths.end(), {
            "/Applications/Kolosal CLI.app/Contents/Frameworks/" + libName + std::string(LIBRARY_EXTENSION),
            "/Applications/Kolosal CLI.app/Contents/MacOS/lib/" + libName + std::string(LIBRARY_EXTENSION),
            // Standard macOS Homebrew paths
            "/opt/homebrew/lib/" + libName + std::string(LIBRARY_EXTENSION),
            "/usr/local/lib/" + libName + std::string(LIBRARY_EXTENSION),
            // Paths relative to executable directory (fallback)
            execDir + "/lib/" + libName + std::string(LIBRARY_EXTENSION),
            execDir + "/../lib/" + libName + std::string(LIBRARY_EXTENSION),
            // Current directory paths
            "./" + libName + std::string(LIBRARY_EXTENSION),
            "./lib/" + libName + std::string(LIBRARY_EXTENSION),
            "../lib/" + libName + std::string(LIBRARY_EXTENSION)
        });
        
        return searchPaths;
    }
#endif

    // Helper function to get platform-specific default inference engine
    std::string getPlatformDefaultInferenceEngine()
    {
#ifdef __APPLE__
        return "llama-metal";
#else
        return "llama-cpu";
#endif
    }

    NodeManager::NodeManager(std::chrono::seconds idleTimeout)
        : idleTimeout_(idleTimeout), stopAutoscaling_(false)
    {
        ServerLogger::logInfo("NodeManager initialized with idle timeout: %lld seconds.", idleTimeout_.count());

        // Initialize the inference loader
        inferenceLoader_ = std::make_unique<InferenceLoader>();

        // Configure inference engines from server config
        auto& config = ServerConfig::getInstance();
        if (!config.inferenceEngines.empty())
        {
            if (inferenceLoader_->configureEngines(config.inferenceEngines))
            {
                auto availableEngines = inferenceLoader_->getAvailableEngines();
                ServerLogger::logInfo("Configured %zu inference engines:", availableEngines.size());
                for (const auto &engine : availableEngines)
                {
                    ServerLogger::logInfo("  - %s: %s (%s)", engine.name.c_str(), engine.description.c_str(), 
                                        engine.is_loaded ? "loaded" : "available");
                }
                
                // Set default inference engine if none is configured
                if (config.defaultInferenceEngine.empty() && !availableEngines.empty())
                {
                    std::string preferredEngine;
                    
#ifdef __APPLE__
                    // On Apple systems, prioritize Metal acceleration
                    ServerLogger::logInfo("Apple system detected. Looking for Metal-accelerated engine...");
                    for (const auto &engine : availableEngines)
                    {
                        if (engine.name == "llama-metal")
                        {
                            preferredEngine = engine.name;
                            ServerLogger::logInfo("Metal acceleration available. Setting default inference engine to: %s", preferredEngine.c_str());
                            break;
                        }
                    }
                    
                    // If llama-metal not found, fall back to CPU
                    if (preferredEngine.empty())
                    {
                        for (const auto &engine : availableEngines)
                        {
                            if (engine.name == "llama-cpu")
                            {
                                preferredEngine = engine.name;
                                ServerLogger::logInfo("Metal acceleration not available. Using CPU-based engine: %s", preferredEngine.c_str());
                                break;
                            }
                        }
                    }
                    
                    // If still empty, use first available
                    if (preferredEngine.empty())
                    {
                        preferredEngine = availableEngines[0].name;
                        ServerLogger::logInfo("Using first available engine: %s", preferredEngine.c_str());
                    }
#else
                    // On non-Apple systems, check if system has a dedicated GPU for Vulkan acceleration
                    bool hasGPU = hasVulkanCapableGPU();
                    
                    if (hasGPU)
                    {
                        // Look for llama-vulkan engine first
                        for (const auto &engine : availableEngines)
                        {
                            if (engine.name == "llama-vulkan")
                            {
                                preferredEngine = engine.name;
                                ServerLogger::logInfo("Dedicated GPU detected. Setting default inference engine to Vulkan-accelerated engine: %s", preferredEngine.c_str());
                                break;
                            }
                        }
                        
                        // If llama-vulkan not found, fall back to first available
                        if (preferredEngine.empty())
                        {
                            preferredEngine = availableEngines[0].name;
                            ServerLogger::logInfo("Dedicated GPU detected, but llama-vulkan engine not available. Using first available engine: %s", preferredEngine.c_str());
                        }
                    }
                    else
                    {
                        // No dedicated GPU, use first available engine (likely CPU-based)
                        preferredEngine = availableEngines[0].name;
                        ServerLogger::logInfo("No dedicated GPU detected. Using CPU-based engine: %s", preferredEngine.c_str());
                    }
#endif
                    
                    config.defaultInferenceEngine = preferredEngine;
                    ServerLogger::logInfo("Set default inference engine to: %s", config.defaultInferenceEngine.c_str());
                    
                    // Persisting default engine changes to disk is disabled by default on macOS app bundle installs.
                    // Allow opt-in via environment variable KOLOSAL_ALLOW_CONFIG_SAVE=1
                    auto canWritePath = [&config]() -> bool {
                        std::string path = config.getCurrentConfigFilePath();
                        if (path.empty()) return false;
                        try {
                            std::filesystem::path p(path);
                            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
                            std::ofstream ofs(path, std::ios::app);
                            if (!ofs.is_open()) return false; ofs.close(); return true;
                        } catch (...) { return false; }
                    };
                    const char *allowSave = getenv("KOLOSAL_ALLOW_CONFIG_SAVE");
                    if ((allowSave && std::string(allowSave) == "1") || canWritePath())
                    {
                        ServerLogger::logInfo("Persisting default inference engine to config (KOLOSAL_ALLOW_CONFIG_SAVE=1)");
                        ServerLogger::logInfo("Current config file path during initialization: '%s'", config.getCurrentConfigFilePath().c_str());
                        if (config.saveToCurrentFile())
                        {
                            ServerLogger::logInfo("Saved default inference engine configuration to current config file");
                        }
                        else
                        {
                            ServerLogger::logWarning("Failed to save default inference engine configuration to current config file");
                        }
                    }
                    else
                    {
                        ServerLogger::logInfo("Skipping config file write (set KOLOSAL_ALLOW_CONFIG_SAVE=1 to force; path not writable)");
                    }
                }
            }
            else
            {
                ServerLogger::logError("Failed to configure inference engines: %s", inferenceLoader_->getLastError().c_str());
            }
        }
        else
        {
            ServerLogger::logWarning("No inference engines configured. Setting up default engines...");
            
            // Set up default inference engines based on platform
            std::vector<InferenceEngineConfig> defaultEngines;
            
#ifdef __APPLE__
            // On Apple systems, prioritize Metal acceleration
            ServerLogger::logInfo("Apple system detected. Adding Metal and CPU inference engines...");
            
            // Try to find libraries in the build directory
            std::filesystem::path buildDir = std::filesystem::current_path();
            auto metalPath = buildDir / "lib" / ("libllama-metal" + std::string(LIBRARY_EXTENSION));
            auto cpuPath = buildDir / "lib" / ("libllama-cpu" + std::string(LIBRARY_EXTENSION));
            
            if (std::filesystem::exists(metalPath))
            {
                defaultEngines.emplace_back("llama-metal", metalPath.string(), "Apple Metal GPU acceleration");
                ServerLogger::logInfo("Added Metal inference engine: %s", metalPath.string().c_str());
            }
            
            if (std::filesystem::exists(cpuPath))
            {
                defaultEngines.emplace_back("llama-cpu", cpuPath.string(), "CPU inference engine");
                ServerLogger::logInfo("Added CPU inference engine: %s", cpuPath.string().c_str());
            }
            
            // If no libraries found in build dir, try system paths
            if (defaultEngines.empty())
            {
                // Get executable directory for relative path searches
                std::string execDir = getExecutableDirectory();
                ServerLogger::logInfo("Searching for inference engines. Executable directory: %s", execDir.c_str());
                
                // Use helper function to get app bundle-aware search paths
                std::vector<std::string> metalPaths = getLibrarySearchPaths(execDir, "libllama-metal");
                std::vector<std::string> cpuPaths = getLibrarySearchPaths(execDir, "libllama-cpu");
                
                // Check for Metal engine first
                for (const auto& path : metalPaths)
                {
                    ServerLogger::logInfo("Checking for Metal inference engine at: %s", path.c_str());
                    if (std::filesystem::exists(path))
                    {
                        defaultEngines.emplace_back("llama-metal", path, "Apple Metal GPU acceleration");
                        ServerLogger::logInfo("Found Metal inference engine: %s", path.c_str());
                        break; // Found Metal, stop searching
                    }
                }
                
                // Check for CPU engine
                for (const auto& path : cpuPaths)
                {
                    ServerLogger::logInfo("Checking for CPU inference engine at: %s", path.c_str());
                    if (std::filesystem::exists(path))
                    {
                        defaultEngines.emplace_back("llama-cpu", path, "CPU inference engine");
                        ServerLogger::logInfo("Found CPU inference engine: %s", path.c_str());
                        break; // Found CPU, stop searching
                    }
                }
                
                // If still no engines found, provide detailed logging
                if (defaultEngines.empty())
                {
                    ServerLogger::logError("No inference engine libraries found in any of the searched paths.");
                    ServerLogger::logError("Please ensure inference engine libraries are properly installed in:");
                    ServerLogger::logError("  - App bundle Frameworks directory (../Frameworks/)");
                    ServerLogger::logError("  - Homebrew locations (/opt/homebrew/lib/ or /usr/local/lib/)");
                    ServerLogger::logError("  - Application bundle (/Applications/Kolosal CLI.app/Contents/Frameworks/)");
                    ServerLogger::logError("  - Relative to executable (./lib/ or ../lib/)");
                }
            }
#else
            // On non-Apple systems, use existing GPU detection logic
            ServerLogger::logInfo("Non-Apple system detected. Adding CPU and GPU inference engines...");
            
            std::filesystem::path buildDir = std::filesystem::current_path();
            auto cpuPath = buildDir / "lib" / ("libllama-cpu" + std::string(LIBRARY_EXTENSION));
            auto vulkanPath = buildDir / "lib" / ("libllama-vulkan" + std::string(LIBRARY_EXTENSION));
            
            if (std::filesystem::exists(cpuPath))
            {
                defaultEngines.emplace_back("llama-cpu", cpuPath.string(), "CPU inference engine");
                ServerLogger::logInfo("Added CPU inference engine: %s", cpuPath.string().c_str());
            }
            
            if (std::filesystem::exists(vulkanPath))
            {
                defaultEngines.emplace_back("llama-vulkan", vulkanPath.string(), "Vulkan GPU acceleration");
                ServerLogger::logInfo("Added Vulkan inference engine: %s", vulkanPath.string().c_str());
            }
#endif
            
            if (!defaultEngines.empty())
            {
                // Update the server config with default engines
                config.inferenceEngines = defaultEngines;
                
                // Try to configure the engines
                if (inferenceLoader_->configureEngines(config.inferenceEngines))
                {
                    auto availableEngines = inferenceLoader_->getAvailableEngines();
                    ServerLogger::logInfo("Configured %zu default inference engines:", availableEngines.size());
                    
                    // Set default engine based on platform
                    if (config.defaultInferenceEngine.empty() && !availableEngines.empty())
                    {
                        std::string preferredEngine;
                        
#ifdef __APPLE__
                        // Prefer Metal on Apple systems
                        for (const auto &engine : availableEngines)
                        {
                            if (engine.name == "llama-metal")
                            {
                                preferredEngine = engine.name;
                                break;
                            }
                        }
                        if (preferredEngine.empty())
                        {
                            preferredEngine = availableEngines[0].name;
                        }
#else
                        // Prefer GPU acceleration on other systems if available
                        bool hasGPU = hasVulkanCapableGPU();
                        if (hasGPU)
                        {
                            for (const auto &engine : availableEngines)
                            {
                                if (engine.name == "llama-vulkan")
                                {
                                    preferredEngine = engine.name;
                                    break;
                                }
                            }
                        }
                        if (preferredEngine.empty())
                        {
                            preferredEngine = availableEngines[0].name;
                        }
#endif
                        
                        config.defaultInferenceEngine = preferredEngine;
                        ServerLogger::logInfo("Set default inference engine to: %s", config.defaultInferenceEngine.c_str());
                        
                        // Persisting configuration is opt-in
                        auto canWritePath = [&config]() -> bool {
                            std::string path = config.getCurrentConfigFilePath();
                            if (path.empty()) return false;
                            try { std::filesystem::path p(path); if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path()); std::ofstream ofs(path, std::ios::app); if (!ofs.is_open()) return false; ofs.close(); return true;} catch (...) { return false; }
                        };
                        const char *allowSave = getenv("KOLOSAL_ALLOW_CONFIG_SAVE");
                        if ((allowSave && std::string(allowSave) == "1") || canWritePath())
                        {
                            if (config.saveToCurrentFile())
                            {
                                ServerLogger::logInfo("Saved default configuration to file");
                            }
                        }
                    }
                }
                else
                {
                    ServerLogger::logError("Failed to configure default inference engines: %s", inferenceLoader_->getLastError().c_str());
                }
            }
            else
            {
                ServerLogger::logError("No inference engine libraries found. Please build inference engines or check installation.");
                ServerLogger::logError("To resolve this issue:");
                ServerLogger::logError("1. Ensure that inference engines are built and installed properly");
                ServerLogger::logError("2. Check that libraries are in one of the expected locations:");
                ServerLogger::logError("   - /opt/homebrew/lib/ (Homebrew installation)");
                ServerLogger::logError("   - /usr/local/lib/ (standard installation)");
                ServerLogger::logError("   - Relative to executable: bin/../lib/");
                ServerLogger::logError("3. Verify that the Metal/CPU inference libraries exist (.dylib files)");
                ServerLogger::logError("4. Consider configuring engines manually in the configuration file");
            }
        }

        autoscalingThread_ = std::thread(&NodeManager::autoscalingLoop, this);
    }

    NodeManager::~NodeManager()
    {
        ServerLogger::logInfo("NodeManager shutting down.");
        stopAutoscaling_.store(true);

        // Wake up autoscaling thread
        {
            std::lock_guard<std::mutex> lock(autoscalingMutex_);
            autoscalingCv_.notify_one();
        }

        if (autoscalingThread_.joinable())
        {
            autoscalingThread_.join();
        }
        ServerLogger::logInfo("Autoscaling thread stopped.");

        // Get exclusive access to engines map
        std::unique_lock<std::shared_mutex> mapLock(engineMapMutex_);

        // Mark all engines for removal and unload them safely
        for (auto &[id, recordPtr] : engines_)
        {
            if (recordPtr)
            {
                recordPtr->markedForRemoval.store(true);
                std::lock_guard<std::mutex> engineLock(recordPtr->engineMutex);

                if (recordPtr->isLoaded.load() && recordPtr->engine)
                {
                    ServerLogger::logInfo("Unloading engine ID \'%s\' during shutdown.", id.c_str());
                    try
                    {
                        recordPtr->engine->unloadModel();
                        ServerLogger::logInfo("Successfully unloaded engine ID \'%s\'.", id.c_str());
                    }
                    catch (const std::exception &e)
                    {
                        ServerLogger::logError("Exception while unloading engine ID \'%s\': %s", id.c_str(), e.what());
                    }
                    catch (...)
                    {
                        ServerLogger::logError("Unknown exception while unloading engine ID \'%s\'", id.c_str());
                    }
                }

                // Wake up any threads waiting on this engine
                recordPtr->loadingCv.notify_all();
            }
        }
        engines_.clear();
        ServerLogger::logInfo("All engines unloaded and NodeManager shut down complete.");
    }

    bool NodeManager::addEngine(const std::string &engineId, const char *modelPath, const LoadingParameters &loadParams, int mainGpuId, const std::string &engineType)
    {
        // First check if engine already exists (read lock)
        {
            std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
            if (engines_.count(engineId))
            {
                ServerLogger::logWarning("Engine with ID \'%s\' already exists.", engineId.c_str());
                return false;
            }
        }

        // Validate model file outside of any locks
        ServerLogger::logInfo("Validating model file for engine \'%s\': %s", engineId.c_str(), modelPath);
        if (!validateModelFile(modelPath))
        {
            ServerLogger::logError("Model validation failed for engine \'%s\'. Skipping engine creation.", engineId.c_str());
            return false;
        }

        std::string actualModelPath = modelPath;
        // Handle URL downloads outside of locks to avoid blocking other engines
        if (is_valid_url(modelPath))
        {
            actualModelPath = handleUrlDownload(engineId, modelPath);
            if (actualModelPath.empty())
            {
                return false; // Download failed
            }
        }

        // Create engine instance using dynamic loader with safety handlers
        ServerLogger::logInfo("Creating %s inference engine for ID '%s'", engineType.c_str(), engineId.c_str());

        std::shared_ptr<IInferenceEngine> enginePtr;

        try
        {
            // Load the inference engine plugin if not already loaded
            if (!inferenceLoader_->isEngineLoaded(engineType))
            {
                ServerLogger::logInfo("Loading %s inference engine plugin...", engineType.c_str());
                if (!inferenceLoader_->loadEngine(engineType))
                {
                    ServerLogger::logError("Failed to load %s inference engine: %s",
                                           engineType.c_str(), inferenceLoader_->getLastError().c_str());
                    return false;
                }
                ServerLogger::logInfo("Successfully loaded %s inference engine plugin", engineType.c_str());
            }

            // Create engine instance from the loaded plugin
            ServerLogger::logInfo("Creating inference engine instance...");
            auto engineInstance = inferenceLoader_->createEngineInstance(engineType);
            if (!engineInstance)
            {
                ServerLogger::logError("Failed to create %s inference engine instance: %s",
                                       engineType.c_str(), inferenceLoader_->getLastError().c_str());
                return false;
            }

            // Load the model with safety handling
            ServerLogger::logInfo("Loading model for engine '%s' from path: %s", engineId.c_str(), actualModelPath.c_str());
            bool loadSuccess = false;
            try
            {
                loadSuccess = engineInstance->loadModel(actualModelPath.c_str(), loadParams, mainGpuId);
            }
            catch (const std::exception &e)
            {
                ServerLogger::logError("Exception during model loading for engine '%s': %s", engineId.c_str(), e.what());
                loadSuccess = false;
            }
            catch (...)
            {
                ServerLogger::logError("Unknown exception during model loading for engine '%s'", engineId.c_str());
                loadSuccess = false;
            }

            if (!loadSuccess)
            {
                ServerLogger::logError("Failed to load model for engine ID '%s' from path '%s'", engineId.c_str(), actualModelPath.c_str());
                // Ensure engine is properly cleaned up
                try
                {
                    if (engineInstance)
                    {
                        engineInstance->unloadModel();
                    }
                }
                catch (...)
                {
                    ServerLogger::logWarning("Exception during cleanup after failed model load for engine '%s'", engineId.c_str());
                }
                return false;
            }

            enginePtr = std::shared_ptr<IInferenceEngine>(engineInstance.release());
            ServerLogger::logInfo("Successfully loaded model for engine '%s'", engineId.c_str());
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Exception during engine creation for '%s': %s", engineId.c_str(), e.what());
            return false;
        }
        catch (...)
        {
            ServerLogger::logError("Unknown exception during engine creation for '%s'", engineId.c_str());
            return false;
        }

        // Create record and add to map (exclusive lock only for map modification)
        auto recordPtr = std::make_shared<EngineRecord>();
        recordPtr->engine = enginePtr;
        recordPtr->modelPath = actualModelPath;
        recordPtr->engineType = engineType;
        recordPtr->loadParams = loadParams;
        recordPtr->mainGpuId = mainGpuId;
        recordPtr->isLoaded.store(true);
        recordPtr->lastActivityTime = std::chrono::steady_clock::now();

        {
            std::unique_lock<std::shared_mutex> mapLock(engineMapMutex_);
            // Double-check pattern to ensure no race condition
            if (engines_.count(engineId))
            {
                ServerLogger::logWarning("Engine with ID \'%s\' was added by another thread.", engineId.c_str());
                return false;
            }
            engines_[engineId] = recordPtr;
        }

        ServerLogger::logInfo("Successfully added and loaded engine with ID \'%s\'. Model: %s", engineId.c_str(), actualModelPath.c_str());

        // Save model to configuration file
        saveModelToConfig(engineId, modelPath, loadParams, mainGpuId, engineType, true);

        // Notify autoscaling thread about new engine
        {
            std::lock_guard<std::mutex> lock(autoscalingMutex_);
            autoscalingCv_.notify_one();
        }

        return true;
    }

    bool NodeManager::addEngine(const std::string &engineId, const char *modelPath, const LoadingParameters &loadParams, int mainGpuId)
    {
        // Use platform-specific default inference engine
        auto& config = ServerConfig::getInstance();
        std::string engineType = !config.defaultInferenceEngine.empty() ? 
                                 config.defaultInferenceEngine : getPlatformDefaultInferenceEngine();
        
        ServerLogger::logInfo("Using inference engine '%s' for model '%s' (platform default)", 
                            engineType.c_str(), engineId.c_str());
        
        // Call the main addEngine method with the determined engine type
        return addEngine(engineId, modelPath, loadParams, mainGpuId, engineType);
    }

    bool NodeManager::addEmbeddingEngine(const std::string &engineId, const char *modelPath, const LoadingParameters &loadParams, int mainGpuId)
    {
        // First check if engine already exists (read lock)
        {
            std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
            if (engines_.count(engineId))
            {
                ServerLogger::logWarning("Embedding engine with ID \'%s\' already exists.", engineId.c_str());
                return false;
            }
        }

        // Validate model file outside of any locks
        ServerLogger::logInfo("Validating embedding model file for engine \'%s\': %s", engineId.c_str(), modelPath);
        if (!validateModelFile(modelPath))
        {
            ServerLogger::logError("Embedding model validation failed for engine \'%s\'. Skipping engine creation.", engineId.c_str());
            return false;
        }

        std::string actualModelPath = modelPath;
        // Handle URL downloads outside of locks to avoid blocking other engines
        if (is_valid_url(modelPath))
        {
            actualModelPath = handleUrlDownload(engineId, modelPath);
            if (actualModelPath.empty())
            {
                return false; // Download failed
            }
        }

        // Use the default inference engine for embedding models if available
        auto& config = ServerConfig::getInstance();
        std::string engineType = !config.defaultInferenceEngine.empty() ? 
                                 config.defaultInferenceEngine : getPlatformDefaultInferenceEngine();
        ServerLogger::logInfo("Using inference engine '%s' for embedding model '%s'", 
                            engineType.c_str(), engineId.c_str());
        std::shared_ptr<IInferenceEngine> enginePtr;

        try
        {
            // Load the inference engine plugin if not already loaded
            if (!inferenceLoader_->isEngineLoaded(engineType))
            {
                ServerLogger::logInfo("Loading %s inference engine plugin for embedding...", engineType.c_str());
                if (!inferenceLoader_->loadEngine(engineType))
                {
                    ServerLogger::logError("Failed to load %s inference engine for embedding: %s",
                                           engineType.c_str(), inferenceLoader_->getLastError().c_str());
                    return false;
                }
                ServerLogger::logInfo("Successfully loaded %s inference engine plugin for embedding", engineType.c_str());
            }

            // Create engine instance from the loaded plugin
            ServerLogger::logInfo("Creating inference engine instance for embedding...");
            auto engineInstance = inferenceLoader_->createEngineInstance(engineType);
            if (!engineInstance)
            {
                ServerLogger::logError("Failed to create %s inference engine instance for embedding: %s",
                                       engineType.c_str(), inferenceLoader_->getLastError().c_str());
                return false;
            }

            // Load the embedding model with safety handling
            ServerLogger::logInfo("Loading embedding model for engine '%s' from path: %s", engineId.c_str(), actualModelPath.c_str());
            bool loadSuccess = false;
            try
            {
                // For embedding models, use the specialized loadEmbeddingModel method
                loadSuccess = engineInstance->loadEmbeddingModel(actualModelPath.c_str(), loadParams, mainGpuId);
            }
            catch (const std::exception &e)
            {
                ServerLogger::logError("Exception during embedding model loading for engine '%s': %s", engineId.c_str(), e.what());
                loadSuccess = false;
            }
            catch (...)
            {
                ServerLogger::logError("Unknown exception during embedding model loading for engine '%s'", engineId.c_str());
                loadSuccess = false;
            }

            if (!loadSuccess)
            {
                ServerLogger::logError("Failed to load embedding model for engine ID '%s' from path '%s'", engineId.c_str(), actualModelPath.c_str());
                // Ensure engine is properly cleaned up
                try
                {
                    if (engineInstance)
                    {
                        engineInstance->unloadModel();
                    }
                }
                catch (...)
                {
                    ServerLogger::logWarning("Exception during cleanup after failed embedding model load for engine '%s'", engineId.c_str());
                }
                return false;
            }

            enginePtr = std::shared_ptr<IInferenceEngine>(engineInstance.release());
            ServerLogger::logInfo("Successfully loaded embedding model for engine '%s'", engineId.c_str());
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Exception during embedding engine creation for '%s': %s", engineId.c_str(), e.what());
            return false;
        }
        catch (...)
        {
            ServerLogger::logError("Unknown exception during embedding engine creation for '%s'", engineId.c_str());
            return false;
        }

        // Create record and add to map (exclusive lock only for map modification)
        auto recordPtr = std::make_shared<EngineRecord>();
        recordPtr->engine = enginePtr;
        recordPtr->modelPath = actualModelPath;
        recordPtr->engineType = engineType;
        recordPtr->loadParams = loadParams;
        recordPtr->mainGpuId = mainGpuId;
        recordPtr->isLoaded.store(true);
        recordPtr->isEmbeddingModel.store(true); // Mark as embedding model
        recordPtr->lastActivityTime = std::chrono::steady_clock::now();

        {
            std::unique_lock<std::shared_mutex> mapLock(engineMapMutex_);
            // Double-check pattern to ensure no race condition
            if (engines_.count(engineId))
            {
                ServerLogger::logWarning("Embedding engine with ID \'%s\' was added by another thread.", engineId.c_str());
                return false;
            }
            engines_[engineId] = recordPtr;
        }

        ServerLogger::logInfo("Successfully added and loaded embedding engine with ID \'%s\'. Model: %s", engineId.c_str(), actualModelPath.c_str());
        
        // Notify autoscaling thread about new engine
        {
            std::lock_guard<std::mutex> lock(autoscalingMutex_);
            autoscalingCv_.notify_one();
        }
        
        return true;
    }

    std::shared_ptr<IInferenceEngine> NodeManager::getEngine(const std::string &engineId)
    {
        // First, get shared access to find the engine record
        std::shared_ptr<EngineRecord> recordPtr;
        {
            std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
            auto it = engines_.find(engineId);
            if (it == engines_.end())
            {
                ServerLogger::logWarning("Engine with ID \'%s\' not found.", engineId.c_str());
                return nullptr;
            }

            recordPtr = it->second; // Get shared ownership of the record
            if (!recordPtr || recordPtr->markedForRemoval.load())
            {
                ServerLogger::logWarning("Engine with ID \'%s\' is marked for removal.", engineId.c_str());
                return nullptr;
            }
        }

        // Now work with the engine record without holding the map lock
        std::unique_lock<std::mutex> engineLock(recordPtr->engineMutex);

        // Update activity time first
        recordPtr->lastActivityTime = std::chrono::steady_clock::now();

        if (!recordPtr->isLoaded.load())
        {
            // Check if another thread is already loading
            if (recordPtr->isLoading.load())
            {
                ServerLogger::logDebug("Engine ID \'%s\' is being loaded by another thread. Waiting...", engineId.c_str());
                recordPtr->loadingCv.wait(engineLock, [recordPtr]
                                          { return !recordPtr->isLoading.load() || recordPtr->markedForRemoval.load(); });

                if (recordPtr->markedForRemoval.load())
                {
                    return nullptr;
                }

                if (recordPtr->isLoaded.load() && recordPtr->engine)
                {
                    ServerLogger::logDebug("Engine ID \'%s\' loaded by another thread.", engineId.c_str());
                    return recordPtr->engine;
                }
                else
                {
                    ServerLogger::logError("Engine ID \'%s\' failed to load by another thread.", engineId.c_str());
                    return nullptr;
                }
            }

            // This thread will handle the loading
            recordPtr->isLoading.store(true);
            engineLock.unlock(); // Release lock during potentially long loading operation

            ServerLogger::logInfo("Engine ID \'%s\' was unloaded due to inactivity. Attempting to reload.", engineId.c_str());

            // Create new engine instance using dynamic loader with safety handlers
            std::string engineType = recordPtr->engineType;
            ServerLogger::logInfo("Stored engine type for '%s': '%s'", engineId.c_str(), engineType.c_str());
            std::shared_ptr<IInferenceEngine> newEngine;

            try
            {
                if (!inferenceLoader_->isEngineLoaded(engineType))
                {
                    ServerLogger::logInfo("Reloading %s inference engine plugin...", engineType.c_str());
                    if (!inferenceLoader_->loadEngine(engineType))
                    {
                        ServerLogger::logError("Failed to reload %s inference engine: %s",
                                               engineType.c_str(), inferenceLoader_->getLastError().c_str());
                        // Re-acquire lock to update state
                        engineLock.lock();
                        recordPtr->isLoading.store(false);
                        recordPtr->loadingCv.notify_all();
                        return nullptr;
                    }
                }

                ServerLogger::logInfo("Creating new inference engine instance for reload...");
                auto newEngineInstance = inferenceLoader_->createEngineInstance(engineType);
                if (!newEngineInstance)
                {
                    ServerLogger::logError("Failed to create %s inference engine instance during reload: %s",
                                           engineType.c_str(), inferenceLoader_->getLastError().c_str());
                    // Re-acquire lock to update state
                    engineLock.lock();
                    recordPtr->isLoading.store(false);
                    recordPtr->loadingCv.notify_all();
                    return nullptr;
                }

                bool loadSuccess = false;
                try
                {
                    ServerLogger::logInfo("Reloading model from path: %s", recordPtr->modelPath.c_str());
                    // Check if this is an embedding model or regular model
                    if (recordPtr->isEmbeddingModel.load())
                    {
                        // For embedding models, use the specialized loadEmbeddingModel method
                        loadSuccess = newEngineInstance->loadEmbeddingModel(recordPtr->modelPath.c_str(), recordPtr->loadParams, recordPtr->mainGpuId);
                    }
                    else
                    {
                        loadSuccess = newEngineInstance->loadModel(recordPtr->modelPath.c_str(), recordPtr->loadParams, recordPtr->mainGpuId);
                    }
                }
                catch (const std::exception &e)
                {
                    ServerLogger::logError("Exception during model reload for engine '%s': %s", engineId.c_str(), e.what());
                    loadSuccess = false;
                }
                catch (...)
                {
                    ServerLogger::logError("Unknown exception during model reload for engine '%s'", engineId.c_str());
                    loadSuccess = false;
                }

                if (loadSuccess)
                {
                    newEngine = std::shared_ptr<IInferenceEngine>(newEngineInstance.release());
                    ServerLogger::logInfo("Successfully reloaded model for engine '%s'", engineId.c_str());
                }
                else
                {
                    ServerLogger::logError("Failed to reload model for engine '%s'", engineId.c_str());
                    // Ensure cleanup
                    try
                    {
                        if (newEngineInstance)
                        {
                            newEngineInstance->unloadModel();
                        }
                    }
                    catch (...)
                    {
                        ServerLogger::logWarning("Exception during cleanup after failed model reload for engine '%s'", engineId.c_str());
                    }
                }
            }
            catch (const std::exception &e)
            {
                ServerLogger::logError("Exception during engine reload for '%s': %s", engineId.c_str(), e.what());
            }
            catch (...)
            {
                ServerLogger::logError("Unknown exception during engine reload for '%s'", engineId.c_str());
            }
            // Re-acquire lock to update state
            engineLock.lock();
            recordPtr->isLoading.store(false);

            if (newEngine && !recordPtr->markedForRemoval.load())
            {
                recordPtr->engine = newEngine;
                recordPtr->isLoaded.store(true);
                ServerLogger::logInfo("Successfully reloaded %s engine ID \'%s\'.", 
                                      recordPtr->isEmbeddingModel.load() ? "embedding" : "LLM", 
                                      engineId.c_str());
            }
            else
            {
                if (recordPtr->markedForRemoval.load())
                {
                    ServerLogger::logInfo("Engine ID \'%s\' was marked for removal during loading.", engineId.c_str());
                }
                else
                {
                    ServerLogger::logError("Failed to reload %s model for engine ID \'%s\' from path \'%s\'.", 
                                          recordPtr->isEmbeddingModel.load() ? "embedding" : "LLM",
                                          engineId.c_str(), recordPtr->modelPath.c_str());
                }
                recordPtr->engine = nullptr;
            }

            // Notify all waiting threads
            recordPtr->loadingCv.notify_all();

            if (!newEngine || recordPtr->markedForRemoval.load())
            {
                return nullptr;
            }
        }

        // Notify autoscaling thread about activity
        {
            std::lock_guard<std::mutex> lock(autoscalingMutex_);
            autoscalingCv_.notify_one();
        }

        return recordPtr->engine;
    }

    bool NodeManager::removeEngine(const std::string &engineId)
    {
        std::shared_ptr<EngineRecord> recordPtr;

        // Get the engine record and mark it for removal
        {
            std::unique_lock<std::shared_mutex> mapLock(engineMapMutex_);
            auto it = engines_.find(engineId);
            if (it == engines_.end())
            {
                ServerLogger::logWarning("Attempted to remove non-existent engine with ID \'%s\'.", engineId.c_str());
                return false;
            }

            recordPtr = it->second;
            engines_.erase(it);
        }

        if (recordPtr)
        {
            recordPtr->markedForRemoval.store(true);
            std::lock_guard<std::mutex> engineLock(recordPtr->engineMutex);

            if (recordPtr->isLoaded.load() && recordPtr->engine)
            {
                ServerLogger::logInfo("Unloading engine with ID \'%s\'.", engineId.c_str());
                try
                {
                    recordPtr->engine->unloadModel();
                    ServerLogger::logInfo("Engine with ID \'%s\' unloaded successfully.", engineId.c_str());
                }
                catch (const std::exception &e)
                {
                    ServerLogger::logError("Exception while unloading engine ID \'%s\': %s", engineId.c_str(), e.what());
                }
                catch (...)
                {
                    ServerLogger::logError("Unknown exception while unloading engine ID \'%s\'", engineId.c_str());
                }
            }

            // Wake up any threads waiting on this engine
            recordPtr->loadingCv.notify_all();
        }

        ServerLogger::logInfo("Engine with ID \'%s\' removed from manager.", engineId.c_str());

        // Remove model from configuration file
        removeModelFromConfig(engineId);

        // Notify autoscaling thread
        {
            std::lock_guard<std::mutex> lock(autoscalingMutex_);
            autoscalingCv_.notify_one();
        }

        return true;
    }

    std::vector<std::string> NodeManager::listEngineIds() const
    {
        std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
        std::vector<std::string> ids;
        ids.reserve(engines_.size());
        for (auto const &[id, recordPtr] : engines_)
        {
            if (recordPtr && !recordPtr->markedForRemoval.load())
            {
                ids.push_back(id);
            }
        }
        return ids;
    }

    std::vector<InferenceEngineInfo> NodeManager::getAvailableInferenceEngines() const
    {
        if (inferenceLoader_)
        {
            return inferenceLoader_->getAvailableEngines();
        }
        return {};
    }

    // Helper function to validate model file existence
    bool NodeManager::validateModelFile(const std::string &modelPath)
    {
        try
        {
            if (is_valid_url(modelPath))
            {
                // For URLs, we can perform a HEAD request to check if the file exists
                ServerLogger::logInfo("Validating URL accessibility: %s", modelPath.c_str());

                try
                {
                    // Try to get file info without downloading
                    auto result = get_url_file_info(modelPath);
                    if (!result.success)
                    {
                        ServerLogger::logError("URL validation failed: %s - %s", modelPath.c_str(), result.error_message.c_str());
                        return false;
                    }
                    ServerLogger::logInfo("URL is accessible. File size: %.2f MB",
                                          static_cast<double>(result.total_bytes) / (1024.0 * 1024.0));
                    return true;
                }
                catch (const std::exception &e)
                {
                    ServerLogger::logError("Exception during URL validation for %s: %s", modelPath.c_str(), e.what());
                    return false;
                }
                catch (...)
                {
                    ServerLogger::logError("Unknown exception during URL validation for %s", modelPath.c_str());
                    return false;
                }
            }
            else
            {
                // For local paths, check if the file exists
                try
                {
                    if (!std::filesystem::exists(modelPath))
                    {
                        ServerLogger::logError("Local model file does not exist: %s", modelPath.c_str());
                        return false;
                    }

                    // Check if it's a regular file (not a directory)
                    if (!std::filesystem::is_regular_file(modelPath))
                    {
                        ServerLogger::logError("Model path is not a regular file: %s", modelPath.c_str());
                        return false;
                    }
                }
                catch (const std::filesystem::filesystem_error &e)
                {
                    ServerLogger::logError("Filesystem error during model validation for %s: %s", modelPath.c_str(), e.what());
                    return false;
                }
                catch (const std::exception &e)
                {
                    ServerLogger::logError("Exception during local file validation for %s: %s", modelPath.c_str(), e.what());
                    return false;
                }

                // Get file size for logging
                try
                {
                    std::error_code ec;
                    auto fileSize = std::filesystem::file_size(modelPath, ec);
                    if (!ec)
                    {
                        ServerLogger::logInfo("Local model file found. Size: %.2f MB",
                                              static_cast<double>(fileSize) / (1024.0 * 1024.0));
                    }
                    else
                    {
                        ServerLogger::logWarning("Could not determine file size for: %s", modelPath.c_str());
                    }
                }
                catch (...)
                {
                    ServerLogger::logWarning("Exception while getting file size for: %s", modelPath.c_str());
                }

                return true;
            }
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Exception during model file validation for %s: %s", modelPath.c_str(), e.what());
            return false;
        }
        catch (...)
        {
            ServerLogger::logError("Unknown exception during model file validation for %s", modelPath.c_str());
            return false;
        }
    }

    bool NodeManager::validateModelPath(const std::string &modelPath)
    {
        // This is a public wrapper for the private validateModelFile function
        return validateModelFile(modelPath);
    }

    void NodeManager::autoscalingLoop()
    {
        ServerLogger::logInfo("Autoscaling thread started.");

        // Initial check interval
        auto nextCheckInterval = std::chrono::seconds(10);

        while (!stopAutoscaling_.load())
        {
            {
                std::unique_lock<std::mutex> autoscalingLock(autoscalingMutex_);

                // Wait for a timeout or a notification
                if (autoscalingCv_.wait_for(autoscalingLock, nextCheckInterval, [this]
                                            { return stopAutoscaling_.load(); }))
                {
                    // stopAutoscaling_ is true, so exit
                    break;
                }

                if (stopAutoscaling_.load())
                    break; // Check again after wait
            }

            auto now = std::chrono::steady_clock::now();
            ServerLogger::logDebug("Autoscaling check at %lld (next check interval was: %lld seconds)",
                                   std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count(),
                                   std::chrono::duration_cast<std::chrono::seconds>(nextCheckInterval).count());

            // Get snapshot of engines to process
            std::vector<std::pair<std::string, std::shared_ptr<EngineRecord>>> engineSnapshot;
            {
                std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
                engineSnapshot.reserve(engines_.size());
                for (const auto &[id, recordPtr] : engines_)
                {
                    if (recordPtr && !recordPtr->markedForRemoval.load())
                    {
                        engineSnapshot.emplace_back(id, recordPtr);
                    }
                }
            }

            // Process engines without holding the map lock
            auto nextCheckTime = std::chrono::steady_clock::now() + std::chrono::seconds(60); // Default to 60 seconds
            bool hasLoadedEngines = false;

            for (const auto &[engineId, recordPtr] : engineSnapshot)
            {
                if (recordPtr->markedForRemoval.load())
                    continue;

                std::lock_guard<std::mutex> engineLock(recordPtr->engineMutex);

                if (recordPtr->isLoaded.load() && recordPtr->engine && !recordPtr->markedForRemoval.load())
                {
                    hasLoadedEngines = true;
                    auto idleDuration = std::chrono::duration_cast<std::chrono::seconds>(now - recordPtr->lastActivityTime);

                    if (idleDuration >= idleTimeout_)
                    {
                        // Check if the engine has any active jobs before unloading
                        if (recordPtr->engine->hasActiveJobs())
                        {
                            ServerLogger::logDebug("Engine ID \'%s\' has been idle for %lld seconds but has active jobs. Skipping unload.",
                                                   engineId.c_str(), idleDuration.count());
                            continue;
                        }

                        ServerLogger::logInfo("Engine ID \'%s\' has been idle for %lld seconds (threshold: %llds). Unloading.",
                                              engineId.c_str(), idleDuration.count(), idleTimeout_.count());
                        recordPtr->engine->unloadModel();
                        recordPtr->isLoaded.store(false);
                        recordPtr->engine = nullptr;
                        ServerLogger::logInfo("Engine ID \'%s\' unloaded due to inactivity.", engineId.c_str());
                    }
                    else
                    {
                        // Calculate when this engine will become idle
                        auto timeWhenIdle = recordPtr->lastActivityTime + idleTimeout_;
                        if (timeWhenIdle < nextCheckTime)
                        {
                            nextCheckTime = timeWhenIdle;
                        }
                    }
                }
            }

            // If no loaded engines, use longer interval
            if (!hasLoadedEngines)
            {
                nextCheckTime = now + std::chrono::seconds(60);
            }

            // Calculate time until next check (but cap it)
            auto timeUntilNextCheck = std::chrono::duration_cast<std::chrono::seconds>(nextCheckTime - now);
            timeUntilNextCheck = (std::max)(timeUntilNextCheck, std::chrono::seconds(1)); // At least 1 second

            // Cap the maximum check interval based on idle timeout to ensure timely unloading
            auto maxCheckInterval = (std::max)(idleTimeout_ / 2, std::chrono::seconds(5)); // At most half the idle timeout, minimum 5 seconds
            timeUntilNextCheck = (std::min)(timeUntilNextCheck, maxCheckInterval);

            // Set the next check interval for the next iteration
            nextCheckInterval = timeUntilNextCheck;

            ServerLogger::logDebug("Next autoscaling check in %lld seconds", timeUntilNextCheck.count());
        }
        ServerLogger::logInfo("Autoscaling thread finished.");
    }

    bool NodeManager::registerEngine(const std::string &engineId, const char *modelPath, const LoadingParameters &loadParams, int mainGpuId, const std::string &engineType)
    {
        // First check if engine already exists (read lock)
        {
            std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
            if (engines_.count(engineId))
            {
                ServerLogger::logWarning("Engine with ID \'%s\' already exists.", engineId.c_str());
                return false;
            }
        }

        // Validate model file outside of any locks
        ServerLogger::logInfo("Validating model file for engine registration \'%s\': %s", engineId.c_str(), modelPath);
        if (!validateModelFile(modelPath))
        {
            ServerLogger::logError("Model validation failed for engine \'%s\'. Skipping engine registration.", engineId.c_str());
            return false;
        }

        std::string actualModelPath = modelPath;
        // Handle URL downloads outside of locks to avoid blocking other engines
        if (is_valid_url(modelPath))
        {
            actualModelPath = handleUrlDownload(engineId, modelPath);
            if (actualModelPath.empty())
            {
                return false; // Download failed
            }
        }

        // Create a record for lazy loading (engine is not loaded yet)
        auto recordPtr = std::make_shared<EngineRecord>();
        recordPtr->engine = nullptr;            // No engine instance yet
        recordPtr->modelPath = actualModelPath; // Store the actual local path
        recordPtr->engineType = engineType;     // Store the engine type
        recordPtr->loadParams = loadParams;
        recordPtr->mainGpuId = mainGpuId;
        recordPtr->isLoaded.store(false); // Mark as not loaded for lazy loading
        recordPtr->lastActivityTime = std::chrono::steady_clock::now();

        ServerLogger::logInfo("Registering engine '%s' with engine type '%s' (passed: '%s')", 
                            engineId.c_str(), recordPtr->engineType.c_str(), engineType.c_str());

        {
            std::unique_lock<std::shared_mutex> mapLock(engineMapMutex_);
            // Double-check pattern to ensure no race condition
            if (engines_.count(engineId))
            {
                ServerLogger::logWarning("Engine with ID \'%s\' was registered by another thread.", engineId.c_str());
                return false;
            }
            engines_[engineId] = recordPtr;
        }

        ServerLogger::logInfo("Successfully registered engine with ID \'%s\' for lazy loading. Model: %s", engineId.c_str(), actualModelPath.c_str());
        
        // Save model to configuration file
        saveModelToConfig(engineId, modelPath, loadParams, mainGpuId, engineType, false);
        
        return true;
    }

    bool NodeManager::registerEngine(const std::string &engineId, const char *modelPath, const LoadingParameters &loadParams, int mainGpuId)
    {
        // Use platform-specific default inference engine
        auto& config = ServerConfig::getInstance();
        std::string engineType = !config.defaultInferenceEngine.empty() ? 
                                 config.defaultInferenceEngine : getPlatformDefaultInferenceEngine();
        
        ServerLogger::logInfo("Using inference engine '%s' for model registration '%s' (platform default)", 
                            engineType.c_str(), engineId.c_str());
        
        // Call the main registerEngine method with the determined engine type
        return registerEngine(engineId, modelPath, loadParams, mainGpuId, engineType);
    }

    std::pair<bool, bool> NodeManager::getEngineStatus(const std::string &engineId) const
    {
        std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
        auto it = engines_.find(engineId);
        if (it == engines_.end())
        {
            return std::make_pair(false, false); // Engine not found
        }

        const auto &recordPtr = it->second;
        if (!recordPtr || recordPtr->markedForRemoval.load())
        {
            return std::make_pair(false, false); // Engine marked for removal
        }

        return std::make_pair(true, recordPtr->isLoaded.load()); // Engine exists, return load status
    }

    std::string NodeManager::handleUrlDownload(const std::string &engineId, const std::string &modelPath)
    {
        ServerLogger::logInfo("Model path for engine \'%s\' is a URL. Starting download: %s", engineId.c_str(), modelPath.c_str());

    // Generate local path for the downloaded model - use user-writable models directory
    std::string downloadsDir = get_executable_models_directory();
        std::string localPath = generate_download_path(modelPath, downloadsDir);

        // Check if the file already exists locally
        if (std::filesystem::exists(localPath))
        {
            // Check if we can resume this download (file might be incomplete)
            if (can_resume_download(modelPath, localPath))
            {
                ServerLogger::logInfo("Found incomplete download for engine '%s', resuming: %s", engineId.c_str(), localPath.c_str());

                // Download the model with progress callback (will resume automatically)
                auto progressCallback = [&engineId](size_t downloaded, size_t total, double percentage)
                {
                    if (total > 0)
                    {
                        ServerLogger::logInfo("Resuming download for engine '%s': %.1f%% (%zu/%zu bytes)",
                                              engineId.c_str(), percentage, downloaded, total);
                    }
                };

                DownloadResult result = download_file(modelPath, localPath, progressCallback);

                if (!result.success)
                {
                    ServerLogger::logError("Failed to resume download for engine '%s' from URL '%s': %s",
                                           engineId.c_str(), modelPath.c_str(), result.error_message.c_str());
                    return "";
                }

                ServerLogger::logInfo("Successfully completed download for engine '%s' to: %s (%.2f MB)",
                                      engineId.c_str(), localPath.c_str(),
                                      static_cast<double>(result.total_bytes) / (1024.0 * 1024.0));
                return localPath;
            }
            else
            {
                ServerLogger::logInfo("Model file already exists locally for engine \'%s\': %s", engineId.c_str(), localPath.c_str());
                return localPath;
            }
        }
        else
        {
            // Download the model with progress callback
            auto progressCallback = [&engineId](size_t downloaded, size_t total, double percentage)
            {
                if (total > 0)
                {
                    ServerLogger::logInfo("Downloading model for engine \'%s\': %.1f%% (%zu/%zu bytes)",
                                          engineId.c_str(), percentage, downloaded, total);
                }
            };

            DownloadResult result = download_file(modelPath, localPath, progressCallback);

            if (!result.success)
            {
                ServerLogger::logError("Failed to download model for engine \'%s\' from URL \'%s\': %s",
                                       engineId.c_str(), modelPath.c_str(), result.error_message.c_str());
                return "";
            }

            ServerLogger::logInfo("Successfully downloaded model for engine \'%s\' to: %s (%.2f MB)",
                                  engineId.c_str(), localPath.c_str(),
                                  static_cast<double>(result.total_bytes) / (1024.0 * 1024.0));
            return localPath;
        }
    }

    bool NodeManager::registerEmbeddingEngine(const std::string &engineId, const char *modelPath, const LoadingParameters &loadParams, int mainGpuId)
    {
        // First check if engine already exists (read lock)
        {
            std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
            if (engines_.count(engineId))
            {
                ServerLogger::logWarning("Embedding engine with ID \'%s\' already exists.", engineId.c_str());
                return false;
            }
        }

        // Validate model file outside of any locks
        ServerLogger::logInfo("Validating embedding model file for engine registration \'%s\': %s", engineId.c_str(), modelPath);
        if (!validateModelFile(modelPath))
        {
            ServerLogger::logError("Embedding model validation failed for engine \'%s\'. Skipping engine registration.", engineId.c_str());
            return false;
        }

        std::string actualModelPath = modelPath;
        // Handle URL downloads outside of locks to avoid blocking other engines
        if (is_valid_url(modelPath))
        {
            actualModelPath = handleUrlDownload(engineId, modelPath);
            if (actualModelPath.empty())
            {
                return false; // Download failed
            }
        }

        // Create a record for lazy loading (engine is not loaded yet)
        auto recordPtr = std::make_shared<EngineRecord>();
        recordPtr->engine = nullptr;            // No engine instance yet
        recordPtr->modelPath = actualModelPath; // Store the actual local path
        
        // Use the default inference engine for embedding models if available
        auto& config = ServerConfig::getInstance();
        std::string engineType = !config.defaultInferenceEngine.empty() ? 
                                 config.defaultInferenceEngine : getPlatformDefaultInferenceEngine();
        recordPtr->engineType = engineType;    // Use appropriate engine type
        ServerLogger::logInfo("Registering embedding model '%s' with inference engine '%s'", 
                            engineId.c_str(), engineType.c_str());
        
        recordPtr->loadParams = loadParams;
        recordPtr->mainGpuId = mainGpuId;
        recordPtr->isLoaded.store(false); // Mark as not loaded for lazy loading
        recordPtr->isEmbeddingModel.store(true); // Mark as embedding model
        recordPtr->lastActivityTime = std::chrono::steady_clock::now();

        {
            std::unique_lock<std::shared_mutex> mapLock(engineMapMutex_);
            // Double-check pattern to ensure no race condition
            if (engines_.count(engineId))
            {
                ServerLogger::logWarning("Embedding engine with ID \'%s\' was registered by another thread.", engineId.c_str());
                return false;
            }
            engines_[engineId] = recordPtr;
        }

        ServerLogger::logInfo("Successfully registered embedding engine with ID \'%s\' for lazy loading. Model: %s", engineId.c_str(), actualModelPath.c_str());
        return true;
    }

    bool NodeManager::reconfigureEngines(const std::vector<InferenceEngineConfig>& engines)
    {
        if (!inferenceLoader_)
        {
            ServerLogger::logError("InferenceLoader not initialized");
            return false;
        }

        ServerLogger::logInfo("Reconfiguring inference engines with %zu engine(s)", engines.size());
        
        if (!inferenceLoader_->configureEngines(engines))
        {
            ServerLogger::logError("Failed to reconfigure inference engines");
            return false;
        }

        auto availableEngines = inferenceLoader_->getAvailableEngines();
        ServerLogger::logInfo("Successfully reconfigured %zu inference engines:", availableEngines.size());
        for (const auto &engine : availableEngines)
        {
            ServerLogger::logInfo("  - %s: %s", engine.name.c_str(), engine.description.c_str());
        }

        return true;
    }

    // Singleton implementation
    namespace {
        std::unique_ptr<NodeManager> instance_;
        std::mutex instanceMutex_;
        bool isInitialized_ = false;
    }

    NodeManager* NodeManager::getInstance()
    {
        std::lock_guard<std::mutex> lock(instanceMutex_);
        if (!instance_)
        {
            instance_ = std::make_unique<NodeManager>();
        }
        return instance_.get();
    }

    void NodeManager::initialize(std::chrono::seconds idleTimeout)
    {
        std::lock_guard<std::mutex> lock(instanceMutex_);
        if (!instance_)
        {
            instance_ = std::make_unique<NodeManager>(idleTimeout);
            isInitialized_ = true;
        }
    }

    std::vector<std::string> NodeManager::getAvailableModels() const
    {
        return listEngineIds();
    }

    bool NodeManager::saveModelToConfig(const std::string& engineId, const std::string& modelPath, 
                                      const LoadingParameters& loadParams, int mainGpuId, 
                                      const std::string& inferenceEngine, bool loadImmediately)
    {
        try
        {
            auto &config = ServerConfig::getInstance();
            
            // Apply default inference engine logic: if the passed engine is empty or platform default,
            // and we have a configured default, use that instead
            std::string actualInferenceEngine = inferenceEngine;
            const std::string platformDefault = getPlatformDefaultInferenceEngine();
            if (!config.defaultInferenceEngine.empty() && 
                (actualInferenceEngine.empty() || actualInferenceEngine == platformDefault))
            {
                actualInferenceEngine = config.defaultInferenceEngine;
                ServerLogger::logInfo("Using default inference engine '%s' for model '%s' instead of '%s'", 
                                    actualInferenceEngine.c_str(), engineId.c_str(), inferenceEngine.c_str());
            }
            
            // Check if model already exists in config to avoid duplicates
            bool modelExistsInConfig = false;
            for (const auto &existingModel : config.models)
            {
                if (existingModel.id == engineId)
                {
                    modelExistsInConfig = true;
                    ServerLogger::logInfo("Model '%s' already exists in configuration, updating it", engineId.c_str());
                    break;
                }
            }
            
            if (!modelExistsInConfig)
            {
                // Create new model config
                ModelConfig modelConfig;
                modelConfig.id = engineId;
                // Don't convert URLs to absolute paths
                if (is_valid_url(modelPath)) {
                    modelConfig.path = modelPath;
                } else {
                    modelConfig.path = ServerConfig::makeAbsolutePath(modelPath);  // Convert to absolute path
                }
                modelConfig.loadImmediately = loadImmediately;
                modelConfig.mainGpuId = mainGpuId;
                modelConfig.inferenceEngine = actualInferenceEngine;
                modelConfig.loadParams = loadParams;
                
                config.models.push_back(modelConfig);
                ServerLogger::logInfo("Added model '%s' to configuration", engineId.c_str());
            }
            else
            {
                // Update existing model config
                for (auto &existingModel : config.models)
                {
                    if (existingModel.id == engineId)
                    {
                        // Don't convert URLs to absolute paths
                        if (is_valid_url(modelPath)) {
                            existingModel.path = modelPath;
                        } else {
                            existingModel.path = ServerConfig::makeAbsolutePath(modelPath);  // Convert to absolute path
                        }
                        existingModel.loadImmediately = loadImmediately;
                        existingModel.mainGpuId = mainGpuId;
                        existingModel.inferenceEngine = actualInferenceEngine;
                        existingModel.loadParams = loadParams;
                        ServerLogger::logInfo("Updated model '%s' in configuration", engineId.c_str());
                        break;
                    }
                }
            }
            
            // Save the updated configuration
            ServerLogger::logInfo("About to save configuration for model '%s'", engineId.c_str());
            ServerLogger::logInfo("Current config file path in NodeManager: '%s'", config.getCurrentConfigFilePath().c_str());
            ServerLogger::logInfo("ServerConfig instance address during model save: %lu", reinterpret_cast<uintptr_t>(&config));
            
            auto canWritePathModel = [&config]() -> bool {
                std::string path = config.getCurrentConfigFilePath();
                if (path.empty()) return false;
                try { std::filesystem::path p(path); if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path()); std::ofstream ofs(path, std::ios::app); if (!ofs.is_open()) return false; ofs.close(); return true;} catch (...) { return false; }
            };
            const char *allowSaveModel = getenv("KOLOSAL_ALLOW_CONFIG_SAVE");
            if ((allowSaveModel && std::string(allowSaveModel) == "1") || canWritePathModel())
            {
                if (!config.saveToCurrentFile())
                {
                    ServerLogger::logWarning("Failed to save configuration to file for model '%s'. Configuration changes are in memory only.", engineId.c_str());
                    return false;
                }
                ServerLogger::logInfo("Successfully saved model '%s' to configuration file", engineId.c_str());
            }
            else
            {
                ServerLogger::logInfo("Skipping saving model '%s' to config (not writable and KOLOSAL_ALLOW_CONFIG_SAVE not set)", engineId.c_str());
            }
            return true;
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("Exception while saving model '%s' to config: %s", engineId.c_str(), ex.what());
            return false;
        }
    }

    bool NodeManager::removeModelFromConfig(const std::string& engineId)
    {
        try
        {
            auto &config = ServerConfig::getInstance();
            
            // Find and remove the model from config
            auto it = std::find_if(config.models.begin(), config.models.end(),
                                   [&engineId](const ModelConfig &model) {
                                       return model.id == engineId;
                                   });
            
            if (it != config.models.end())
            {
                config.models.erase(it);
                ServerLogger::logInfo("Removed model '%s' from configuration", engineId.c_str());
                
                // Save the updated configuration
                ServerLogger::logInfo("About to save configuration after removing model '%s'", engineId.c_str());
                ServerLogger::logInfo("Current config file path in NodeManager: '%s'", config.getCurrentConfigFilePath().c_str());
                ServerLogger::logInfo("ServerConfig instance address during model removal: %lu", reinterpret_cast<uintptr_t>(&config));
                
                auto canWritePathRemove = [&config]() -> bool {
                    std::string path = config.getCurrentConfigFilePath();
                    if (path.empty()) return false;
                    try { std::filesystem::path p(path); if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path()); std::ofstream ofs(path, std::ios::app); if (!ofs.is_open()) return false; ofs.close(); return true;} catch (...) { return false; }
                };
                const char *allowSaveRemove = getenv("KOLOSAL_ALLOW_CONFIG_SAVE");
                if ((allowSaveRemove && std::string(allowSaveRemove) == "1") || canWritePathRemove())
                {
                    if (!config.saveToCurrentFile())
                    {
                        ServerLogger::logWarning("Failed to save configuration to file after removing model '%s'. Configuration changes are in memory only.", engineId.c_str());
                        return false;
                    }
                    ServerLogger::logInfo("Successfully updated configuration file after removing model '%s'", engineId.c_str());
                }
                else
                {
                    ServerLogger::logInfo("Skipping config update after removing model '%s' (not writable and KOLOSAL_ALLOW_CONFIG_SAVE not set)", engineId.c_str());
                }
                return true;
            }
            else
            {
                ServerLogger::logInfo("Model '%s' was not found in configuration", engineId.c_str());
                return true; // Not an error if model wasn't in config
            }
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("Exception while removing model '%s' from config: %s", engineId.c_str(), ex.what());
            return false;
        }
    }

} // namespace kolosal
