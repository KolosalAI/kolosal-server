#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp" // Assuming a logger is available

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

    auto engine = std::make_shared<InferenceEngine>();
    if (!engine->loadModel(modelPath, loadParams, mainGpuId)) {
        ServerLogger::logError("Failed to load model for engine ID \'%s\' from path \'%s\'.", engineId.c_str(), modelPath);
        return false;
    }

    EngineRecord record;
    record.engine = engine;
    record.modelPath = modelPath;
    record.loadParams = loadParams;
    record.mainGpuId = mainGpuId;
    record.isLoaded = true;
    record.lastActivityTime = std::chrono::steady_clock::now();
    
    engines_[engineId] = record;
    ServerLogger::logInfo("Successfully added and loaded engine with ID \'%s\'. Model: %s", engineId.c_str(), modelPath);
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
        ServerLogger::logDebug("Autoscaling check at %lld", std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());

        for (auto& pair : engines_) {
            EngineRecord& record = pair.second;
            if (record.isLoaded && record.engine) {
                auto idleDuration = std::chrono::duration_cast<std::chrono::seconds>(now - record.lastActivityTime);
                if (idleDuration >= idleTimeout_) {
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

} // namespace kolosal
