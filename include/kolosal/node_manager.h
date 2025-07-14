#ifndef KOLOSAL_NODE_MANAGER_H
#define KOLOSAL_NODE_MANAGER_H

#include "export.hpp"
#include "inference_interface.h"
#include "inference_loader.hpp"
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>

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
    NodeManager(std::chrono::seconds idleTimeout = std::chrono::seconds(300));
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
     * @param engineType Type of engine to load ("cpu", "cuda", "vulkan")
     * @return True if the engine was loaded successfully, false otherwise.
     */
    bool addEngine(const std::string& engineId, const char* modelPath, const LoadingParameters& loadParams, int mainGpuId = 0, const std::string& engineType = "cpu");

    /**
     * @brief Registers a model for lazy loading without immediately loading it.
     * The model will be validated but not loaded until first access.
     * 
     * @param engineId A unique identifier for this engine.
     * @param modelPath Path to the model file.
     * @param loadParams Parameters for loading the model.
     * @param mainGpuId The main GPU ID to use for this engine.
     * @param engineType Type of engine to load ("cpu", "cuda", "vulkan")
     * @return True if the model was validated and registered successfully, false otherwise.
     */
    bool registerEngine(const std::string& engineId, const char* modelPath, const LoadingParameters& loadParams, int mainGpuId = 0, const std::string& engineType = "cpu");

    /**
     * @brief Retrieves a pointer to an inference engine by its ID.
     * If the engine was unloaded due to inactivity, it will attempt to reload it.
     * Accessing an engine resets its idle timer.
     * 
     * @param engineId The ID of the engine to retrieve.
     * @return A shared_ptr to the IInferenceEngine, or nullptr if not found or reload fails.
     */
    std::shared_ptr<IInferenceEngine> getEngine(const std::string& engineId);

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
     * @brief Get list of all available inference engine libraries.
     * 
     * @return A vector of InferenceEngineInfo structures containing details about available engines.
     */
    std::vector<InferenceEngineInfo> getAvailableInferenceEngines() const;

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
        std::shared_ptr<IInferenceEngine> engine;
        std::string modelPath;
        std::string engineType;  // "cpu", "cuda", "vulkan"
        LoadingParameters loadParams;
        int mainGpuId;
        std::chrono::steady_clock::time_point lastActivityTime;
        std::atomic<bool> isLoaded{false};
        std::atomic<bool> isLoading{false};
        std::atomic<bool> markedForRemoval{false};
        mutable std::mutex engineMutex;
        std::condition_variable loadingCv;
        
        EngineRecord() : engineType("cpu"), mainGpuId(0), lastActivityTime(std::chrono::steady_clock::now()) {}
        
        EngineRecord(const EngineRecord&) = delete;
        EngineRecord& operator=(const EngineRecord&) = delete;
        
        EngineRecord(EngineRecord&& other) noexcept 
            : engine(std::move(other.engine))
            , modelPath(std::move(other.modelPath))
            , engineType(std::move(other.engineType))
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
                engineType = std::move(other.engineType);
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

#pragma warning(push)
#pragma warning(disable: 4251)
    std::unordered_map<std::string, std::shared_ptr<EngineRecord>> engines_;
    mutable std::shared_mutex engineMapMutex_;

    // Dynamic inference loader for plugin management
    std::unique_ptr<InferenceLoader> inferenceLoader_;

    std::thread autoscalingThread_;
    std::atomic<bool> stopAutoscaling_{false};
    std::condition_variable autoscalingCv_;
#pragma warning(pop)
    mutable std::mutex autoscalingMutex_;
#pragma warning(push)
#pragma warning(disable: 4251)
    std::chrono::seconds idleTimeout_;
#pragma warning(pop)

    /**
     * @brief The main loop for the autoscaling thread.
     * Periodically checks for idle engines and unloads them.
     */
    void autoscalingLoop();

    /**
     * @brief Finds the best directory to search for inference engine plugins.
     * 
     * @return Path to the plugins directory
     */
    std::string findPluginsDirectory();

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
