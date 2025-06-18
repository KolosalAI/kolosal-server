#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp" // Assuming a logger is available
#include "kolosal/download_utils.hpp"
#include <filesystem>

namespace kolosal {

NodeManager::NodeManager(std::chrono::seconds idleTimeout) 
    : idleTimeout_(idleTimeout), stopAutoscaling_(false) {
    ServerLogger::logInfo("NodeManager initialized with idle timeout: %lld seconds.", idleTimeout_.count());
    autoscalingThread_ = std::thread(&NodeManager::autoscalingLoop, this);
}

NodeManager::~NodeManager() {
    ServerLogger::logInfo("NodeManager shutting down.");
    stopAutoscaling_.store(true);
    autoscalingCv_.notify_one();
    if (autoscalingThread_.joinable()) {
        autoscalingThread_.join();
    }
    ServerLogger::logInfo("Autoscaling thread stopped.");

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto const& [id, record] : engines_) {
        if (record.isLoaded && record.engine) {
            ServerLogger::logInfo("Unloading engine ID \'%s\' during shutdown.", id.c_str());
            record.engine->unloadModel();
        }
    }
    engines_.clear();
    ServerLogger::logInfo("All engines unloaded and NodeManager shut down complete.");
}

bool NodeManager::addEngine(const std::string& engineId, const char* modelPath, const LoadingParameters& loadParams, int mainGpuId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (engines_.count(engineId)) {
        ServerLogger::logWarning("Engine with ID \'%s\' already exists.", engineId.c_str());
        return false;
    }

    // First, validate if the model file exists
    ServerLogger::logInfo("Validating model file for engine \'%s\': %s", engineId.c_str(), modelPath);
    if (!validateModelFile(modelPath)) {
        ServerLogger::logError("Model validation failed for engine \'%s\'. Skipping engine creation.", engineId.c_str());
        return false;
    }

    std::string actualModelPath = modelPath;
      // Check if the model path is a URL and download if necessary
    if (is_valid_url(modelPath)) {
        ServerLogger::logInfo("Model path for engine \'%s\' is a URL. Starting download: %s", engineId.c_str(), modelPath);
        
        // Generate local path for the downloaded model
        std::string downloadsDir = "./models";
        std::string localPath = generate_download_path(modelPath, downloadsDir);
        
        // Check if the file already exists locally
        if (std::filesystem::exists(localPath)) {
            // Check if we can resume this download (file might be incomplete)
            if (can_resume_download(modelPath, localPath)) {
                ServerLogger::logInfo("Found incomplete download for engine '%s', resuming: %s", engineId.c_str(), localPath.c_str());
                
                // Download the model with progress callback (will resume automatically)
                auto progressCallback = [&engineId](size_t downloaded, size_t total, double percentage) {
                    if (total > 0) {
                        ServerLogger::logInfo("Resuming download for engine '%s': %.1f%% (%zu/%zu bytes)", 
                                             engineId.c_str(), percentage, downloaded, total);
                    }
                };
                
                DownloadResult result = download_file(modelPath, localPath, progressCallback);
                
                if (!result.success) {
                    ServerLogger::logError("Failed to resume download for engine '%s' from URL '%s': %s", 
                                         engineId.c_str(), modelPath, result.error_message.c_str());
                    return false;
                }
                
                ServerLogger::logInfo("Successfully completed download for engine '%s' to: %s (%.2f MB)", 
                                     engineId.c_str(), localPath.c_str(), 
                                     static_cast<double>(result.total_bytes) / (1024.0 * 1024.0));
                actualModelPath = localPath;
            } else {
                ServerLogger::logInfo("Model file already exists locally for engine \'%s\': %s", engineId.c_str(), localPath.c_str());
                actualModelPath = localPath;
            }
        } else {
            // Download the model with progress callback
            auto progressCallback = [&engineId](size_t downloaded, size_t total, double percentage) {
                if (total > 0) {
                    ServerLogger::logInfo("Downloading model for engine \'%s\': %.1f%% (%zu/%zu bytes)", 
                                         engineId.c_str(), percentage, downloaded, total);
                }
            };
            
            DownloadResult result = download_file(modelPath, localPath, progressCallback);
            
            if (!result.success) {
                ServerLogger::logError("Failed to download model for engine \'%s\' from URL \'%s\': %s", 
                                     engineId.c_str(), modelPath, result.error_message.c_str());
                return false;
            }
            
            ServerLogger::logInfo("Successfully downloaded model for engine \'%s\' to: %s (%.2f MB)", 
                                 engineId.c_str(), localPath.c_str(), 
                                 static_cast<double>(result.total_bytes) / (1024.0 * 1024.0));
            actualModelPath = localPath;
        }
    }

    auto engine = std::make_shared<InferenceEngine>();
    if (!engine->loadModel(actualModelPath.c_str(), loadParams, mainGpuId)) {
        ServerLogger::logError("Failed to load model for engine ID \'%s\' from path \'%s\'.", engineId.c_str(), actualModelPath.c_str());
        return false;
    }

    EngineRecord record;
    record.engine = engine;
    record.modelPath = actualModelPath;  // Store the actual local path
    record.loadParams = loadParams;
    record.mainGpuId = mainGpuId;
    record.isLoaded = true;
    record.lastActivityTime = std::chrono::steady_clock::now();
    
    engines_[engineId] = record;
    ServerLogger::logInfo("Successfully added and loaded engine with ID \'%s\'. Model: %s", engineId.c_str(), actualModelPath.c_str());
    autoscalingCv_.notify_one(); // Notify autoscaling thread about new engine
    return true;
}

std::shared_ptr<InferenceEngine> NodeManager::getEngine(const std::string& engineId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = engines_.find(engineId);
    if (it == engines_.end()) {
        ServerLogger::logWarning("Engine with ID \'%s\' not found.", engineId.c_str());
        return nullptr;
    }

    EngineRecord& record = it->second;
    if (!record.isLoaded) {
        ServerLogger::logInfo("Engine ID \'%s\' was unloaded due to inactivity. Attempting to reload.", engineId.c_str());
        record.engine = std::make_shared<InferenceEngine>(); // Create new engine instance
        
        // Note: During reload, we use the stored model path which should already be local
        // because the download would have happened during the initial addEngine call
        if (!record.engine->loadModel(record.modelPath.c_str(), record.loadParams, record.mainGpuId)) {
            ServerLogger::logError("Failed to reload model for engine ID \'%s\' from path \'%s\'.", engineId.c_str(), record.modelPath.c_str());
            record.engine = nullptr; // Ensure engine is null if reload fails
            return nullptr;
        }
        record.isLoaded = true;
        ServerLogger::logInfo("Successfully reloaded engine ID \'%s\'.", engineId.c_str());
    }
    
    record.lastActivityTime = std::chrono::steady_clock::now();
    autoscalingCv_.notify_one(); // Notify autoscaling thread about activity
    return record.engine;
}

bool NodeManager::removeEngine(const std::string& engineId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = engines_.find(engineId);
    if (it != engines_.end()) {
        if (it->second.isLoaded && it->second.engine) {
            it->second.engine->unloadModel();
            ServerLogger::logInfo("Engine with ID \'%s\' unloaded.", engineId.c_str());
        }
        engines_.erase(it);
        ServerLogger::logInfo("Engine with ID \'%s\' removed from manager.", engineId.c_str());
        autoscalingCv_.notify_one(); // Notify autoscaling thread
        return true;
    }
    ServerLogger::logWarning("Attempted to remove non-existent engine with ID \'%s\'.", engineId.c_str());
    return false;
}

std::vector<std::string> NodeManager::listEngineIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(engines_.size());
    for (auto const& [id, val] : engines_) {
        ids.push_back(id);
    }
    return ids;
}

// Helper function to validate model file existence
bool NodeManager::validateModelFile(const std::string& modelPath) {
    if (is_valid_url(modelPath)) {
        // For URLs, we can perform a HEAD request to check if the file exists
        ServerLogger::logInfo("Validating URL accessibility: %s", modelPath.c_str());
        
        // Try to get file info without downloading
        auto result = get_url_file_info(modelPath);
        if (!result.success) {
            ServerLogger::logError("URL validation failed: %s - %s", modelPath.c_str(), result.error_message.c_str());
            return false;
        }
          ServerLogger::logInfo("URL is accessible. File size: %.2f MB", 
                             static_cast<double>(result.total_bytes) / (1024.0 * 1024.0));
        return true;
    } else {
        // For local paths, check if the file exists
        if (!std::filesystem::exists(modelPath)) {
            ServerLogger::logError("Local model file does not exist: %s", modelPath.c_str());
            return false;
        }
        
        // Check if it's a regular file (not a directory)
        if (!std::filesystem::is_regular_file(modelPath)) {
            ServerLogger::logError("Model path is not a regular file: %s", modelPath.c_str());
            return false;
        }
        
        // Get file size for logging
        std::error_code ec;
        auto fileSize = std::filesystem::file_size(modelPath, ec);
        if (!ec) {
            ServerLogger::logInfo("Local model file found. Size: %.2f MB", 
                                 static_cast<double>(fileSize) / (1024.0 * 1024.0));
        } else {
            ServerLogger::logWarning("Could not determine file size for: %s", modelPath.c_str());
        }
        
        return true;
    }
}

bool NodeManager::validateModelPath(const std::string& modelPath) {
    // This is a public wrapper for the private validateModelFile function
    return validateModelFile(modelPath);
}

void NodeManager::autoscalingLoop() {
    ServerLogger::logInfo("Autoscaling thread started.");
    while (!stopAutoscaling_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait for a timeout or a notification
        // The condition variable waits for idleTimeout_ unless notified earlier
        // We use a shorter wait time in the loop to check stopAutoscaling_ more frequently
        // and to re-evaluate idle engines.
        if (autoscalingCv_.wait_for(lock, std::chrono::seconds(60), [this]{ return stopAutoscaling_.load(); })) {
            // stopAutoscaling_ is true, so exit
            break;
        }

        if (stopAutoscaling_.load()) break; // Check again after wait

        auto now = std::chrono::steady_clock::now();
        ServerLogger::logDebug("Autoscaling check at %lld", std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());        for (auto& pair : engines_) {
            EngineRecord& record = pair.second;
            if (record.isLoaded && record.engine) {
                auto idleDuration = std::chrono::duration_cast<std::chrono::seconds>(now - record.lastActivityTime);
                if (idleDuration >= idleTimeout_) {
                    // Check if the engine has any active jobs before unloading
                    if (record.engine->hasActiveJobs()) {
                        ServerLogger::logDebug("Engine ID \'%s\' has been idle for %lld seconds but has active jobs. Skipping unload.", 
                                             pair.first.c_str(), idleDuration.count());
                        continue;
                    }
                    
                    ServerLogger::logInfo("Engine ID \'%s\' has been idle for %lld seconds (threshold: %llds). Unloading.", 
                                       pair.first.c_str(), idleDuration.count(), idleTimeout_.count());
                    record.engine->unloadModel();
                    record.isLoaded = false;
                    // record.engine can be reset here if desired, or kept for faster reload
                    // For now, we keep the shared_ptr but mark as unloaded.
                    // A new one will be created in getEngine if needed.
                    ServerLogger::logInfo("Engine ID \'%s\' unloaded due to inactivity.", pair.first.c_str());
                }
            }
        }
    }
    ServerLogger::logInfo("Autoscaling thread finished.");
}

bool NodeManager::registerEngine(const std::string& engineId, const char* modelPath, const LoadingParameters& loadParams, int mainGpuId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (engines_.count(engineId)) {
        ServerLogger::logWarning("Engine with ID \'%s\' already exists.", engineId.c_str());
        return false;
    }

    // First, validate if the model file exists
    ServerLogger::logInfo("Validating model file for engine registration \'%s\': %s", engineId.c_str(), modelPath);
    if (!validateModelFile(modelPath)) {
        ServerLogger::logError("Model validation failed for engine \'%s\'. Skipping engine registration.", engineId.c_str());
        return false;
    }

    std::string actualModelPath = modelPath;
      // Check if the model path is a URL and pre-download if necessary
    if (is_valid_url(modelPath)) {
        ServerLogger::logInfo("Model path for engine \'%s\' is a URL. Pre-downloading: %s", engineId.c_str(), modelPath);
        
        // Generate local path for the downloaded model
        std::string downloadsDir = "./models";
        std::string localPath = generate_download_path(modelPath, downloadsDir);
        
        // Check if the file already exists locally
        if (std::filesystem::exists(localPath)) {
            // Check if we can resume this download (file might be incomplete)
            if (can_resume_download(modelPath, localPath)) {
                ServerLogger::logInfo("Found incomplete download for engine '%s', resuming: %s", engineId.c_str(), localPath.c_str());
                
                // Download the model with progress callback (will resume automatically)
                auto progressCallback = [&engineId](size_t downloaded, size_t total, double percentage) {
                    if (total > 0) {
                        ServerLogger::logInfo("Resuming pre-download for engine '%s': %.1f%% (%zu/%zu bytes)", 
                                             engineId.c_str(), percentage, downloaded, total);
                    }
                };
                
                DownloadResult result = download_file(modelPath, localPath, progressCallback);
                
                if (!result.success) {
                    ServerLogger::logError("Failed to resume pre-download for engine '%s' from URL '%s': %s", 
                                         engineId.c_str(), modelPath, result.error_message.c_str());
                    return false;
                }
                
                ServerLogger::logInfo("Successfully completed pre-download for engine '%s' to: %s (%.2f MB)", 
                                     engineId.c_str(), localPath.c_str(), 
                                     static_cast<double>(result.total_bytes) / (1024.0 * 1024.0));
                actualModelPath = localPath;
            } else {
                ServerLogger::logInfo("Model file already exists locally for engine \'%s\': %s", engineId.c_str(), localPath.c_str());
                actualModelPath = localPath;
            }
        } else {
            // Download the model with progress callback
            auto progressCallback = [&engineId](size_t downloaded, size_t total, double percentage) {
                if (total > 0) {
                    ServerLogger::logInfo("Pre-downloading model for engine \'%s\': %.1f%% (%zu/%zu bytes)", 
                                         engineId.c_str(), percentage, downloaded, total);
                }
            };
            
            DownloadResult result = download_file(modelPath, localPath, progressCallback);
            
            if (!result.success) {
                ServerLogger::logError("Failed to pre-download model for engine \'%s\' from URL \'%s\': %s", 
                                     engineId.c_str(), modelPath, result.error_message.c_str());
                return false;
            }
            
            ServerLogger::logInfo("Successfully pre-downloaded model for engine \'%s\' to: %s (%.2f MB)", 
                                 engineId.c_str(), localPath.c_str(), 
                                 static_cast<double>(result.total_bytes) / (1024.0 * 1024.0));
            actualModelPath = localPath;
        }
    }

    // Create a record for lazy loading (engine is not loaded yet)
    EngineRecord record;
    record.engine = nullptr;  // No engine instance yet
    record.modelPath = actualModelPath;  // Store the actual local path
    record.loadParams = loadParams;
    record.mainGpuId = mainGpuId;
    record.isLoaded = false;  // Mark as not loaded for lazy loading
    record.lastActivityTime = std::chrono::steady_clock::now();
    
    engines_[engineId] = record;
    ServerLogger::logInfo("Successfully registered engine with ID \'%s\' for lazy loading. Model: %s", engineId.c_str(), actualModelPath.c_str());
    return true;
}

} // namespace kolosal
