#include "kolosal/download_manager.hpp"
#include "kolosal/download_utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "inference_interface.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cmath>

namespace kolosal
{

    DownloadManager &DownloadManager::getInstance()
    {
        static DownloadManager instance;
        return instance;
    }
    bool DownloadManager::startDownload(const std::string &model_id, const std::string &url, const std::string &local_path)
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);

        // Check if download is already in progress
        auto existing_it = downloads_.find(model_id);
        if (existing_it != downloads_.end())
        {
            // If the existing download is cancelled, failed, or completed, we can restart it
            auto &existing_progress = existing_it->second;
            if (existing_progress->status == "downloading" || existing_progress->status == "paused")
            {
                ServerLogger::logWarning("Download already in progress for model: %s", model_id.c_str());
                return false;
            }
            else
            {
                // Clean up the old entry to allow restart
                ServerLogger::logInfo("Cleaning up previous download entry for model: %s (status: %s)",
                                      model_id.c_str(), existing_progress->status.c_str());
                download_futures_.erase(model_id);
                downloads_.erase(existing_it);
            }
        } // Create new download progress entry
        auto progress = std::make_shared<DownloadProgress>(model_id, url, local_path);
        downloads_[model_id] = progress;

        // Report initial progress as 0%
        ServerLogger::logInfo("Initialized download for model %s: 0.0%% (0/0 bytes)", model_id.c_str());

        // Start download in background thread
        download_futures_[model_id] = std::async(std::launch::async, [this, progress]()
                                                 { performDownload(progress); });

        ServerLogger::logInfo("Started download for model %s from URL: %s", model_id.c_str(), url.c_str());
        return true;
    }

    bool DownloadManager::startDownloadWithEngine(const std::string &model_id, const std::string &url,
                                                  const std::string &local_path, const EngineCreationParams &engine_params)
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);

        // Check if download is already in progress
        auto existing_it = downloads_.find(model_id);
        if (existing_it != downloads_.end())
        {
            // If the existing download is cancelled, failed, or completed, we can restart it
            auto &existing_progress = existing_it->second;
            if (existing_progress->status == "downloading" || existing_progress->status == "paused")
            {
                ServerLogger::logWarning("Download already in progress for model: %s", model_id.c_str());
                return false;
            }
            else
            {
                // Clean up the old entry to allow restart
                ServerLogger::logInfo("Cleaning up previous download entry for model: %s (status: %s)",
                                      model_id.c_str(), existing_progress->status.c_str());
                download_futures_.erase(model_id);
                downloads_.erase(existing_it);
            }
        } // Create new download progress entry with engine parameters
        auto progress = std::make_shared<DownloadProgress>(model_id, url, local_path);
        progress->engine_params = std::make_unique<EngineCreationParams>(engine_params);
        downloads_[model_id] = progress;

        // Report initial progress as 0%
        ServerLogger::logInfo("Initialized download for model %s: 0.0%% (0/0 bytes)", model_id.c_str());

        // Start download in background thread
        download_futures_[model_id] = std::async(std::launch::async, [this, progress]()
                                                 { performDownload(progress); });

        ServerLogger::logInfo("Started download with engine creation for model %s from URL: %s", model_id.c_str(), url.c_str());
        return true;
    }

    std::shared_ptr<DownloadProgress> DownloadManager::getDownloadProgress(const std::string &model_id)
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);

        auto it = downloads_.find(model_id);
        if (it != downloads_.end())
        {
            return it->second;
        }

        return nullptr;
    }

    bool DownloadManager::isDownloadInProgress(const std::string &model_id)
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);

        auto it = downloads_.find(model_id);
        if (it != downloads_.end())
        {
            return it->second->status == "downloading";
        }

        return false;
    }
    bool DownloadManager::cancelDownload(const std::string &model_id)
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);
        auto it = downloads_.find(model_id);
        if (it != downloads_.end() && (it->second->status == "downloading" || it->second->status == "creating_engine" || it->second->status == "paused"))
        {
            it->second->status = "cancelled";
            it->second->end_time = std::chrono::system_clock::now();
            it->second->cancelled = true; // Set the cancellation flag
            it->second->paused = false;   // Clear pause flag when cancelling

            // Check if this is a startup download (has engine params)
            if (it->second->engine_params)
            {
                ServerLogger::logInfo("Cancelled startup download for model: %s", model_id.c_str());
            }
            else
            {
                ServerLogger::logInfo("Cancelled download for model: %s", model_id.c_str());
            }
            return true;
        }

        return false;
    }
    int DownloadManager::cancelAllDownloads()
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);
        int cancelled_count = 0;
        int startup_downloads = 0;
        int regular_downloads = 0;
        for (auto &pair : downloads_)
        {
            auto progress = pair.second;
            if (progress && (progress->status == "downloading" || progress->status == "creating_engine" || progress->status == "paused"))
            {
                progress->status = "cancelled";
                progress->end_time = std::chrono::system_clock::now();
                progress->cancelled = true; // Set the cancellation flag
                progress->paused = false;   // Clear pause flag when cancelling
                cancelled_count++;

                // Check if this is a startup download (has engine params)
                if (progress->engine_params)
                {
                    startup_downloads++;
                    ServerLogger::logInfo("Cancelled startup download for model: %s", pair.first.c_str());
                }
                else
                {
                    regular_downloads++;
                    ServerLogger::logInfo("Cancelled download for model: %s", pair.first.c_str());
                }
            }
        }

        if (cancelled_count > 0)
        {
            ServerLogger::logInfo("Cancelled %d downloads total (%d startup, %d regular)",
                                  cancelled_count, startup_downloads, regular_downloads);
        }

        return cancelled_count;
    }
    void DownloadManager::waitForAllDownloads()
    {
        std::map<std::string, std::future<void>> futures_to_wait;

        // First, cancel all downloads to ensure they exit quickly
        ServerLogger::logInfo("Cancelling all active downloads before shutdown...");
        int cancelled = cancelAllDownloads();

        // Give a moment for cancellation to take effect
        if (cancelled > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Copy futures under lock to avoid holding lock while waiting
        {
            std::lock_guard<std::mutex> lock(downloads_mutex_);
            futures_to_wait = std::move(download_futures_);
            download_futures_.clear();
        }

        if (futures_to_wait.empty())
        {
            ServerLogger::logInfo("No download threads to wait for");
            return;
        }

        ServerLogger::logInfo("Waiting for %zu download threads to complete...", futures_to_wait.size());

        // Wait for all download threads to complete with progressive timeout
        int completed = 0;
        int total = futures_to_wait.size();

        for (auto &pair : futures_to_wait)
        {
            try
            {
                if (pair.second.valid())
                {
                    // Use a longer timeout for the first few threads, shorter for others
                    int timeout_seconds = (completed < 2) ? 10 : 3;
                    auto status = pair.second.wait_for(std::chrono::seconds(timeout_seconds));

                    if (status == std::future_status::ready)
                    {
                        pair.second.get(); // Get any exceptions
                        completed++;
                        ServerLogger::logInfo("Download thread completed (%d/%d): %s", completed, total, pair.first.c_str());
                    }
                    else
                    {
                        ServerLogger::logWarning("Download thread for %s did not complete within %ds timeout, forcing shutdown",
                                                 pair.first.c_str(), timeout_seconds);
                        // Don't call get() on timeout to avoid blocking
                    }
                }
                else
                {
                    ServerLogger::logInfo("Download thread future invalid: %s", pair.first.c_str());
                }
            }
            catch (const std::exception &ex)
            {
                ServerLogger::logError("Error waiting for download thread %s: %s", pair.first.c_str(), ex.what());
            }
        }

        ServerLogger::logInfo("Finished waiting for download threads (%d/%d completed)", completed, total);
    }

    std::map<std::string, std::shared_ptr<DownloadProgress>> DownloadManager::getAllActiveDownloads()
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);

        std::map<std::string, std::shared_ptr<DownloadProgress>> active_downloads;
        for (const auto &pair : downloads_)
        {
            if (pair.second->status == "downloading" || pair.second->status == "paused")
            {
                active_downloads[pair.first] = pair.second;
            }
        }

        return active_downloads;
    }

    void DownloadManager::cleanupOldDownloads(int minutes)
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);

        auto now = std::chrono::system_clock::now();
        auto cutoff_time = now - std::chrono::minutes(minutes);

        auto it = downloads_.begin();
        while (it != downloads_.end())
        {
            auto &progress = it->second; // Only clean up completed, failed, or cancelled downloads (not downloading or paused)
            if (progress->status != "downloading" &&
                progress->status != "paused" &&
                progress->end_time < cutoff_time)
            {

                ServerLogger::logInfo("Cleaning up old download record for model: %s", it->first.c_str());

                // Clean up the future as well
                download_futures_.erase(it->first);

                it = downloads_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    void DownloadManager::performDownload(std::shared_ptr<DownloadProgress> progress)
    {
        try
        {
            // Check if file is already complete before starting download
            if (std::filesystem::exists(progress->local_path))
            {
                try
                {
                    size_t local_size = std::filesystem::file_size(progress->local_path);
                    if (local_size > 0)
                    {
                        // Get expected file size to check if file is complete
                        DownloadResult url_info = get_url_file_info(progress->url);
                        if (url_info.success && local_size == url_info.total_bytes)
                        {
                            std::lock_guard<std::mutex> lock(downloads_mutex_);
                            progress->status = "already_complete";
                            progress->total_bytes = local_size;
                            progress->downloaded_bytes = local_size;
                            progress->percentage = 100.0;
                            progress->end_time = std::chrono::system_clock::now();

                            ServerLogger::logInfo("File already fully downloaded for model %s: %zu bytes (skipping download)",
                                                  progress->model_id.c_str(), local_size);

                            // If engine parameters are provided, create the engine
                            if (progress->engine_params)
                            {
                                createEngineAfterDownload(progress);
                            }
                            return;
                        }
                    }
                }
                catch (const std::exception &ex)
                {
                    ServerLogger::logWarning("Error checking existing file for model %s: %s",
                                             progress->model_id.c_str(), ex.what());
                }
            }
            // Progress callback to update download progress
            bool progress_was_reported = false;
            auto progressCallback = [progress, &progress_was_reported](size_t downloaded, size_t total, double percentage)
            {
                std::lock_guard<std::mutex> lock(const_cast<DownloadManager &>(getInstance()).downloads_mutex_);
                
                // Handle pause by checking the pause flag and waiting
                while (progress->paused && progress->status == "paused" && !progress->cancelled)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                // If cancelled while paused, return early
                if (progress->cancelled)
                {
                    return;
                }
                
                progress_was_reported = true;
                
                // Validate percentage value before storing
                if (percentage < 0.0 || percentage > 100.0 || std::isnan(percentage) || std::isinf(percentage))
                {
                    ServerLogger::logWarning("Invalid percentage value %.2f for model %s, clamping to valid range", 
                                           percentage, progress->model_id.c_str());
                    percentage = std::max(0.0, std::min(100.0, percentage));
                    if (std::isnan(percentage) || std::isinf(percentage))
                    {
                        percentage = 0.0;
                    }
                }
                
                progress->downloaded_bytes = downloaded;
                progress->total_bytes = total;
                progress->percentage = percentage;

                ServerLogger::logInfo("Download progress for %s: %.1f%% (%zu/%zu bytes)",
                                      progress->model_id.c_str(), percentage, downloaded, total); }; // Perform the actual download with cancellation support
            ServerLogger::logInfo("Starting download for model %s, initial progress: %.1f%% (%zu/%zu bytes)",
                                  progress->model_id.c_str(), progress->percentage, progress->downloaded_bytes, progress->total_bytes);
            DownloadResult result = download_file_with_cancellation_and_resume(progress->url, progress->local_path, progressCallback, &(progress->cancelled), true);

            {
                std::lock_guard<std::mutex> lock(downloads_mutex_);

                ServerLogger::logInfo("Download result for model %s: success=%s, total_bytes=%zu, error='%s'",
                                      progress->model_id.c_str(), result.success ? "true" : "false",
                                      result.total_bytes, result.error_message.c_str());
                if (result.success && progress->status != "cancelled")
                {
                    progress->status = "completed";
                    progress->total_bytes = result.total_bytes;
                    progress->downloaded_bytes = result.total_bytes;
                    progress->percentage = 100.0;

                    // Check if this was a file that was already complete (no progress reported)
                    if (!progress_was_reported && result.total_bytes > 0)
                    {
                        ServerLogger::logInfo("File was already complete for model: %s (no download needed)", progress->model_id.c_str());
                        // Set status to indicate the file was already complete
                        progress->status = "already_complete";
                    }
                    else
                    {
                        ServerLogger::logInfo("Download completed successfully for model: %s", progress->model_id.c_str());
                    }
                }
                else if (progress->status != "cancelled")
                {
                    progress->status = "failed";
                    progress->error_message = result.error_message;
                    ServerLogger::logError("Download failed for model %s: %s", progress->model_id.c_str(), result.error_message.c_str());
                }
            }

            // If engine parameters are provided and download was successful, create the engine
            if (progress->engine_params && result.success && progress->status != "cancelled")
            {
                createEngineAfterDownload(progress);
            }
            else
            {
                std::lock_guard<std::mutex> lock(downloads_mutex_);
                progress->end_time = std::chrono::system_clock::now();
            }
        }
        catch (const std::exception &ex)
        {
            std::lock_guard<std::mutex> lock(downloads_mutex_);
            progress->status = "failed";
            progress->error_message = std::string("Exception during download: ") + ex.what();
            progress->end_time = std::chrono::system_clock::now();

            ServerLogger::logError("Exception during download for model %s: %s", progress->model_id.c_str(), ex.what());
        }
    }

    void DownloadManager::createEngineAfterDownload(std::shared_ptr<DownloadProgress> progress)
    {
        try
        {
            // Update status to indicate engine creation
            {
                std::lock_guard<std::mutex> lock(downloads_mutex_);
                progress->status = "creating_engine";
                ServerLogger::logInfo("Starting engine creation for model: %s", progress->model_id.c_str());
            }

            // Get the NodeManager and create the engine
            auto &nodeManager = ServerAPI::instance().getNodeManager();

            // Use the downloaded file path as the model path
            std::string actualModelPath = progress->local_path;
            
            bool success;
            if (progress->engine_params->load_immediately)
            {
                // Load immediately - use addEngine
                success = nodeManager.addEngine(
                    progress->engine_params->engine_id,
                    actualModelPath.c_str(),
                    progress->engine_params->loading_params,
                    progress->engine_params->main_gpu_id);
            }
            else
            {
                // Lazy loading - use registerEngine
                success = nodeManager.registerEngine(
                    progress->engine_params->engine_id,
                    actualModelPath.c_str(),
                    progress->engine_params->loading_params,
                    progress->engine_params->main_gpu_id);
            }

            std::lock_guard<std::mutex> lock(downloads_mutex_);

            if (success)
            {
                progress->status = "engine_created";
                ServerLogger::logInfo("Engine created successfully for model: %s", progress->model_id.c_str());
            }
            else
            {
                progress->status = "engine_creation_failed";
                progress->error_message = "Failed to create engine after successful download";
                ServerLogger::logError("Failed to create engine for model: %s", progress->model_id.c_str());
            }

            progress->end_time = std::chrono::system_clock::now();
        }
        catch (const std::exception &ex)
        {
            std::lock_guard<std::mutex> lock(downloads_mutex_);
            progress->status = "engine_creation_failed";
            progress->error_message = std::string("Exception during engine creation: ") + ex.what();
            progress->end_time = std::chrono::system_clock::now();
            ServerLogger::logError("Exception during engine creation for model %s: %s", progress->model_id.c_str(), ex.what());
        }
    }

    // Add a helper method for startup model loading
    bool DownloadManager::loadModelAtStartup(const std::string &model_id, const std::string &model_path,
                                             const LoadingParameters &load_params, int main_gpu_id, bool load_immediately)
    {
        // Check if the model path is a URL
        if (is_valid_url(model_path))
        {
            // Generate download path
            std::string download_path = generate_download_path(model_path, "./models");

            // Check if file already exists and is complete
            if (std::filesystem::exists(download_path))
            {
                // Check if we can resume this download (file might be incomplete)
                if (can_resume_download(model_path, download_path))
                {
                    ServerLogger::logInfo("Found incomplete download for startup model '%s', will resume: %s",
                                          model_id.c_str(), download_path.c_str());

                    // Create engine creation parameters for resume
                    EngineCreationParams engine_params;
                    engine_params.engine_id = model_id;
                    engine_params.load_immediately = load_immediately;
                    engine_params.main_gpu_id = main_gpu_id;
                    engine_params.loading_params = load_params;

                    // Start download with engine creation (will resume automatically)
                    return startDownloadWithEngine(model_id, model_path, download_path, engine_params);
                }
                else
                {
                    ServerLogger::logInfo("Model file already exists locally for startup model '%s': %s",
                                          model_id.c_str(), download_path.c_str());

                    // Load directly using NodeManager
                    auto &node_manager = ServerAPI::instance().getNodeManager();
                    if (load_immediately)
                    {
                        return node_manager.addEngine(model_id, download_path.c_str(), load_params, main_gpu_id);
                    }
                    else
                    {
                        return node_manager.registerEngine(model_id, download_path.c_str(), load_params, main_gpu_id);
                    }
                }
            }
            else
            {
                ServerLogger::logInfo("Starting startup download for model '%s' from URL: %s", model_id.c_str(), model_path.c_str());

                // Create engine creation parameters
                EngineCreationParams engine_params;
                engine_params.engine_id = model_id;
                engine_params.load_immediately = load_immediately;
                engine_params.main_gpu_id = main_gpu_id;
                engine_params.loading_params = load_params;

                // Start download with engine creation
                return startDownloadWithEngine(model_id, model_path, download_path, engine_params);
            }
        }
        else
        {
            // Not a URL, use regular NodeManager methods
            auto &node_manager = ServerAPI::instance().getNodeManager();
            if (load_immediately)
            {
                return node_manager.addEngine(model_id, model_path.c_str(), load_params, main_gpu_id);
            }
            else
            {
                return node_manager.registerEngine(model_id, model_path.c_str(), load_params, main_gpu_id);
            }
        }
    }

    bool DownloadManager::pauseDownload(const std::string &model_id)
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);

        auto it = downloads_.find(model_id);
        if (it != downloads_.end() && it->second->status == "downloading")
        {
            it->second->paused = true;
            it->second->status = "paused";
            ServerLogger::logInfo("Paused download for model: %s", model_id.c_str());
            return true;
        }

        return false;
    }

    bool DownloadManager::resumeDownload(const std::string &model_id)
    {
        std::lock_guard<std::mutex> lock(downloads_mutex_);

        auto it = downloads_.find(model_id);
        if (it != downloads_.end() && it->second->status == "paused")
        {
            it->second->paused = false;
            it->second->status = "downloading";
            ServerLogger::logInfo("Resumed download for model: %s", model_id.c_str());
            return true;
        }

        return false;
    }

} // namespace kolosal
