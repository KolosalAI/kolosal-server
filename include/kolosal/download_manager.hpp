#pragma once

#include "export.hpp"
#include "download_utils.hpp"
#include "inference_interface.h"
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <future>
#include <chrono>

namespace kolosal {

    // Structure to hold engine creation parameters
    struct EngineCreationParams {
        std::string engine_id;
        bool load_immediately;
        int main_gpu_id;
        LoadingParameters loading_params;
        
        EngineCreationParams() : load_immediately(false), main_gpu_id(-1) {}
    };

    // Structure to hold download progress information
    struct DownloadProgress {
        std::string model_id;
        std::string url;
        std::string local_path;
        size_t total_bytes;
        size_t downloaded_bytes;
        double percentage;
        std::string status; // "downloading", "completed", "failed", "cancelled", "creating_engine"
        std::string error_message;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        
        // Engine creation parameters (if this download is for engine creation)
        std::unique_ptr<EngineCreationParams> engine_params;

        DownloadProgress() : total_bytes(0), downloaded_bytes(0), percentage(0.0), status("downloading") {}
        
        DownloadProgress(const std::string& id, const std::string& download_url, const std::string& path)
            : model_id(id), url(download_url), local_path(path), total_bytes(0), 
              downloaded_bytes(0), percentage(0.0), status("downloading"),
              start_time(std::chrono::system_clock::now()) {}
    };    // Download manager class to handle concurrent downloads and track progress
    class KOLOSAL_SERVER_API DownloadManager {
    public:
        static DownloadManager& getInstance();

        // Start a new download (non-blocking)
        bool startDownload(const std::string& model_id, const std::string& url, const std::string& local_path);

        // Start a new download with engine creation parameters (non-blocking)
        bool startDownloadWithEngine(const std::string& model_id, const std::string& url, 
                                   const std::string& local_path, const EngineCreationParams& engine_params);

        // Get download progress for a specific model
        std::shared_ptr<DownloadProgress> getDownloadProgress(const std::string& model_id);

        // Check if a download is in progress
        bool isDownloadInProgress(const std::string& model_id);

        // Cancel a download
        bool cancelDownload(const std::string& model_id);

        // Get all active downloads
        std::map<std::string, std::shared_ptr<DownloadProgress>> getAllActiveDownloads();

        // Clean up completed/failed downloads older than specified minutes
        void cleanupOldDownloads(int minutes = 60);

        /**
         * @brief Load a model at startup, using DownloadManager for URLs
         * 
         * This method is specifically designed for startup model loading.
         * It will use DownloadManager for URL downloads to enable progress tracking,
         * and regular NodeManager methods for local files.
         * 
         * @param model_id The unique identifier for the model
         * @param model_path The path or URL to the model
         * @param load_params Loading parameters for the model
         * @param main_gpu_id The main GPU ID to use
         * @param load_immediately Whether to load immediately or register for lazy loading
         * @return True if the model was successfully processed, false otherwise
         */
        bool loadModelAtStartup(const std::string& model_id, const std::string& model_path, 
                               const LoadingParameters& load_params, int main_gpu_id, bool load_immediately);

    private:
        DownloadManager() = default;
        ~DownloadManager() = default;
        
        // Non-copyable
        DownloadManager(const DownloadManager&) = delete;
        DownloadManager& operator=(const DownloadManager&) = delete;

        std::map<std::string, std::shared_ptr<DownloadProgress>> downloads_;
        std::map<std::string, std::future<void>> download_futures_;
        mutable std::mutex downloads_mutex_;        // Internal method to perform the actual download
        void performDownload(std::shared_ptr<DownloadProgress> progress);
        
        // Internal method to create engine after successful download
        void createEngineAfterDownload(std::shared_ptr<DownloadProgress> progress);
    };

} // namespace kolosal
