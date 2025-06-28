#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp" // Assuming a logger is available
#include "kolosal/download_utils.hpp"
#include <filesystem>

namespace kolosal
{

    NodeManager::NodeManager(std::chrono::seconds idleTimeout)
        : idleTimeout_(idleTimeout), stopAutoscaling_(false)
    {
        ServerLogger::logInfo("NodeManager initialized with idle timeout: %lld seconds.", idleTimeout_.count());
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
        
        // Mark all engines for removal and unload them
        for (auto& [id, recordPtr] : engines_)
        {
            if (recordPtr)        {
            recordPtr->markedForRemoval.store(true);
            std::lock_guard<std::mutex> engineLock(recordPtr->engineMutex);
            
            if (recordPtr->isLoaded.load() && recordPtr->engine)
            {
                ServerLogger::logInfo("Unloading engine ID \'%s\' during shutdown.", id.c_str());
                recordPtr->engine->unloadModel();
            }
        }
        }
        engines_.clear();
        ServerLogger::logInfo("All engines unloaded and NodeManager shut down complete.");
    }

    bool NodeManager::addEngine(const std::string &engineId, const char *modelPath, const LoadingParameters &loadParams, int mainGpuId)
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

        // Create engine instance and load model (outside of map lock)
        auto engine = std::make_shared<InferenceEngine>();
        if (!engine->loadModel(actualModelPath.c_str(), loadParams, mainGpuId))
        {
            ServerLogger::logError("Failed to load model for engine ID \'%s\' from path \'%s\'.", engineId.c_str(), actualModelPath.c_str());
            return false;
        }

        // Create record and add to map (exclusive lock only for map modification)
        auto recordPtr = std::make_shared<EngineRecord>();
        recordPtr->engine = engine;
        recordPtr->modelPath = actualModelPath;
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
        
        // Notify autoscaling thread about new engine
        {
            std::lock_guard<std::mutex> lock(autoscalingMutex_);
            autoscalingCv_.notify_one();
        }
        
        return true;
    }

    std::shared_ptr<InferenceEngine> NodeManager::getEngine(const std::string &engineId)
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
                recordPtr->loadingCv.wait(engineLock, [recordPtr] { 
                    return !recordPtr->isLoading.load() || recordPtr->markedForRemoval.load(); 
                });
                
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
            auto newEngine = std::make_shared<InferenceEngine>();
            
            bool loadSuccess = newEngine->loadModel(recordPtr->modelPath.c_str(), recordPtr->loadParams, recordPtr->mainGpuId);
            
            // Re-acquire lock to update state
            engineLock.lock();
            recordPtr->isLoading.store(false);
            
            if (loadSuccess && !recordPtr->markedForRemoval.load())
            {
                recordPtr->engine = newEngine;
                recordPtr->isLoaded.store(true);
                ServerLogger::logInfo("Successfully reloaded engine ID \'%s\'.", engineId.c_str());
            }
            else
            {
                if (recordPtr->markedForRemoval.load())
                {
                    ServerLogger::logInfo("Engine ID \'%s\' was marked for removal during loading.", engineId.c_str());
                }
                else
                {
                    ServerLogger::logError("Failed to reload model for engine ID \'%s\' from path \'%s\'.", engineId.c_str(), recordPtr->modelPath.c_str());
                }
                recordPtr->engine = nullptr;
            }
            
            // Notify all waiting threads
            recordPtr->loadingCv.notify_all();
            
            if (!loadSuccess || recordPtr->markedForRemoval.load())
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
                recordPtr->engine->unloadModel();
                ServerLogger::logInfo("Engine with ID \'%s\' unloaded.", engineId.c_str());
            }
            
            // Wake up any threads waiting on this engine
            recordPtr->loadingCv.notify_all();
        }
        
        ServerLogger::logInfo("Engine with ID \'%s\' removed from manager.", engineId.c_str());
        
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

    // Helper function to validate model file existence
    bool NodeManager::validateModelFile(const std::string &modelPath)
    {
        if (is_valid_url(modelPath))
        {
            // For URLs, we can perform a HEAD request to check if the file exists
            ServerLogger::logInfo("Validating URL accessibility: %s", modelPath.c_str());

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
        else
        {
            // For local paths, check if the file exists
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

            // Get file size for logging
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

            return true;
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
        while (!stopAutoscaling_.load())
        {
            std::unique_lock<std::mutex> autoscalingLock(autoscalingMutex_);
            
            // Wait for a timeout or a notification
            if (autoscalingCv_.wait_for(autoscalingLock, std::chrono::seconds(60), [this]
                                        { return stopAutoscaling_.load(); }))
            {
                // stopAutoscaling_ is true, so exit
                break;
            }

            if (stopAutoscaling_.load())
                break; // Check again after wait

            autoscalingLock.unlock(); // Release autoscaling lock before processing engines

            auto now = std::chrono::steady_clock::now();
            ServerLogger::logDebug("Autoscaling check at %lld", std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
            
            // Get snapshot of engines to process
            std::vector<std::pair<std::string, std::shared_ptr<EngineRecord>>> engineSnapshot;
            {
                std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
                engineSnapshot.reserve(engines_.size());
                for (const auto& [id, recordPtr] : engines_)
                {
                    if (recordPtr && !recordPtr->markedForRemoval.load())
                    {
                        engineSnapshot.emplace_back(id, recordPtr);
                    }
                }
            }
            
            // Process engines without holding the map lock
            for (const auto& [engineId, recordPtr] : engineSnapshot)
            {
                if (recordPtr->markedForRemoval.load())
                    continue;
                    
                std::lock_guard<std::mutex> engineLock(recordPtr->engineMutex);
                
                if (recordPtr->isLoaded.load() && recordPtr->engine && !recordPtr->markedForRemoval.load())
                {
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
                        recordPtr->engine = nullptr; // Release engine instance to free memory
                        ServerLogger::logInfo("Engine ID \'%s\' unloaded due to inactivity.", engineId.c_str());
                    }
                }
            }
        }
        ServerLogger::logInfo("Autoscaling thread finished.");
    }

    bool NodeManager::registerEngine(const std::string &engineId, const char *modelPath, const LoadingParameters &loadParams, int mainGpuId)
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
        recordPtr->loadParams = loadParams;
        recordPtr->mainGpuId = mainGpuId;
        recordPtr->isLoaded.store(false); // Mark as not loaded for lazy loading
        recordPtr->lastActivityTime = std::chrono::steady_clock::now();

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
        return true;
    }

    std::pair<bool, bool> NodeManager::getEngineStatus(const std::string& engineId) const
    {
        std::shared_lock<std::shared_mutex> mapLock(engineMapMutex_);
        auto it = engines_.find(engineId);
        if (it == engines_.end())
        {
            return std::make_pair(false, false); // Engine not found
        }
        
        const auto& recordPtr = it->second;
        if (!recordPtr || recordPtr->markedForRemoval.load())
        {
            return std::make_pair(false, false); // Engine marked for removal
        }
        
        return std::make_pair(true, recordPtr->isLoaded.load()); // Engine exists, return load status
    }

    std::string NodeManager::handleUrlDownload(const std::string& engineId, const std::string& modelPath)
    {
        ServerLogger::logInfo("Model path for engine \'%s\' is a URL. Starting download: %s", engineId.c_str(), modelPath.c_str());

        // Generate local path for the downloaded model
        std::string downloadsDir = "./models";
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

} // namespace kolosal
