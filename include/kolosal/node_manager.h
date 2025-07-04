#ifndef KOLOSAL_NODE_MANAGER_H
#define KOLOSAL_NODE_MANAGER_H

#include "export.hpp"
#include "inference.h" // Assuming InferenceEngine is defined here
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <chrono> // Added for time tracking
#include <thread> // Added for autoscaling thread
#include <atomic> // Added for thread control
#include <condition_variable> // Added for autoscaling thread synchronization

namespace kolosal {

/**
 * @brief Manages a collection of InferenceEngine instances.
 * 
 * The NodeManager is responsible for loading, unloading, and providing access
 * to multiple inference engines. This allows for managing different models
 * or configurations simultaneously.
 */
class KOLOSAL_SERVER_API NodeManager {
public:
    NodeManager(std::chrono::seconds idleTimeout = std::chrono::seconds(300)); // Added idleTimeout parameter
    ~NodeManager();

    NodeManager(const NodeManager&) = delete;
    NodeManager& operator=(const NodeManager&) = delete;
    NodeManager(NodeManager&&) = delete;
    NodeManager& operator=(NodeManager&&) = delete;

    /**
     * @brief Loads a new inference engine with the given model and parameters.
     * 
     * @param engineId A unique identifier for this engine.
     * @param modelPath Path to the model file.
     * @param loadParams Parameters for loading the model.
     * @param mainGpuId The main GPU ID to use for this engine.
     * @return True if the engine was loaded successfully, false otherwise.
     */
    bool addEngine(const std::string& engineId, const char* modelPath, const LoadingParameters& loadParams, int mainGpuId = 0);

    /**
     * @brief Registers a model for lazy loading without immediately loading it.
     * The model will be validated but not loaded until first access.
     * 
     * @param engineId A unique identifier for this engine.
     * @param modelPath Path to the model file.
     * @param loadParams Parameters for loading the model.
     * @param mainGpuId The main GPU ID to use for this engine.
     * @return True if the model was validated and registered successfully, false otherwise.
     */
    bool registerEngine(const std::string& engineId, const char* modelPath, const LoadingParameters& loadParams, int mainGpuId = 0);

    /**
     * @brief Retrieves a pointer to an inference engine by its ID.
     * If the engine was unloaded due to inactivity, it will attempt to reload it.
     * Accessing an engine resets its idle timer.
     * 
     * @param engineId The ID of the engine to retrieve.
     * @return A shared_ptr to the InferenceEngine, or nullptr if not found or reload fails.
     */
    std::shared_ptr<InferenceEngine> getEngine(const std::string& engineId);

    /**
     * @brief Checks if an engine exists and its load status without loading it.
     * This method does not trigger loading of lazy models and does not update activity time.
     * 
     * @param engineId The ID of the engine to check.
     * @return A pair of (exists, isLoaded) where exists indicates if the engine is registered
     *         and isLoaded indicates if it's currently loaded in memory.
     */
    std::pair<bool, bool> getEngineStatus(const std::string& engineId) const;

    /**
     * @brief Removes and unloads an inference engine by its ID.
     * 
     * @param engineId The ID of the engine to remove.
     * @return True if the engine was removed successfully, false otherwise.
     */
    bool removeEngine(const std::string& engineId);

    /**
     * @brief Lists the IDs of all currently managed engines.
     * 
     * @return A vector of strings containing the engine IDs.
     */
    std::vector<std::string> listEngineIds() const;

    /**
     * @brief Validates if a model file exists without loading it.
     * 
     * @param modelPath Path to the model file (local or URL).
     * @return True if the model file exists and is accessible, false otherwise.
     */
    bool validateModelPath(const std::string& modelPath);

    /**
     * @brief Handles URL download for models
     * @param engineId Engine identifier for logging
     * @param modelPath URL to download
     * @return Local path to downloaded file, or empty string on failure
     */
    std::string handleUrlDownload(const std::string& engineId, const std::string& modelPath);

private:
    struct EngineRecord {
        std::shared_ptr<InferenceEngine> engine;
        std::string modelPath;                 // Metadata: Path to the model file
        LoadingParameters loadParams;          // Metadata: Parameters used to load the model
        int mainGpuId;                         // Metadata: Main GPU ID for the engine
        std::chrono::steady_clock::time_point lastActivityTime; // For autoscaling
        std::atomic<bool> isLoaded{false};     // Tracks if the model is currently loaded
        std::atomic<bool> isLoading{false};    // Tracks if the model is currently being loaded
        std::atomic<bool> markedForRemoval{false}; // Tracks if engine is being removed
        mutable std::mutex engineMutex;        // Per-engine mutex for thread safety
        std::condition_variable loadingCv;     // For waiting on model loading
        
        EngineRecord() : mainGpuId(0), lastActivityTime(std::chrono::steady_clock::now()) {}
        
        // Disable copy operations to avoid mutex copying issues
        EngineRecord(const EngineRecord&) = delete;
        EngineRecord& operator=(const EngineRecord&) = delete;
        
        // Enable move operations
        EngineRecord(EngineRecord&& other) noexcept 
            : engine(std::move(other.engine))
            , modelPath(std::move(other.modelPath))
            , loadParams(other.loadParams)
            , mainGpuId(other.mainGpuId)
            , lastActivityTime(other.lastActivityTime)
            , isLoaded(other.isLoaded.load())
            , isLoading(other.isLoading.load())
            , markedForRemoval(other.markedForRemoval.load())
        {}
        
        EngineRecord& operator=(EngineRecord&& other) noexcept {
            if (this != &other) {
                engine = std::move(other.engine);
                modelPath = std::move(other.modelPath);
                loadParams = other.loadParams;
                mainGpuId = other.mainGpuId;
                lastActivityTime = other.lastActivityTime;
                isLoaded.store(other.isLoaded.load());
                isLoading.store(other.isLoading.load());
                markedForRemoval.store(other.markedForRemoval.load());
            }
            return *this;
        }
    };

    std::unordered_map<std::string, std::shared_ptr<EngineRecord>> engines_;
    mutable std::shared_mutex engineMapMutex_; // Shared mutex for engines map (allows concurrent reads)

    // Autoscaling members
    std::thread autoscalingThread_;
    std::atomic<bool> stopAutoscaling_{false};
    std::condition_variable autoscalingCv_;
    mutable std::mutex autoscalingMutex_; // Separate mutex for autoscaling
    std::chrono::seconds idleTimeout_;

    /**
     * @brief The main loop for the autoscaling thread.
     * Periodically checks for idle engines and unloads them.
     */
    void autoscalingLoop();

    /**
     * @brief Validates if a model file exists (either local path or URL).
     * 
     * @param modelPath Path to the model file (local or URL).
     * @return True if the model file exists and is accessible, false otherwise.
     */
    bool validateModelFile(const std::string& modelPath);
};

} // namespace kolosal

#endif // KOLOSAL_NODE_MANAGER_H
