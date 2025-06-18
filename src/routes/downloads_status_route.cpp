#include "kolosal/routes/downloads_status_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace kolosal {

    bool DownloadsStatusRoute::match(const std::string& method, const std::string& path) {
        // Match GET requests to /downloads or /v1/downloads
        return (method == "GET" && (path == "/downloads" || path == "/v1/downloads"));
    }

    void DownloadsStatusRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Received downloads status request", std::this_thread::get_id());

            // Get all active downloads from download manager
            auto& download_manager = DownloadManager::getInstance();
            auto active_downloads = download_manager.getAllActiveDownloads();

            json downloads_array = json::array();
            
            for (const auto& pair : active_downloads) {
                const auto& progress = pair.second;
                  // Calculate elapsed time
                auto now = std::chrono::system_clock::now();
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    (progress->status == "downloading" || progress->status == "creating_engine" ? now : progress->end_time) - progress->start_time
                ).count();

                // Calculate download speed (bytes per second)
                double download_speed = elapsed_seconds > 0 ? static_cast<double>(progress->downloaded_bytes) / elapsed_seconds : 0.0;

                // Estimate remaining time (only for active downloads)
                int estimated_remaining_seconds = -1;
                if (progress->status == "downloading" && progress->percentage > 0 && download_speed > 0) {
                    size_t remaining_bytes = progress->total_bytes - progress->downloaded_bytes;
                    estimated_remaining_seconds = static_cast<int>(remaining_bytes / download_speed);
                }                json download_info = {
                    {"model_id", progress->model_id},
                    {"status", progress->status},
                    {"download_type", progress->engine_params ? "startup" : "regular"},
                    {"url", progress->url},
                    {"local_path", progress->local_path},
                    {"progress", {
                        {"downloaded_bytes", progress->downloaded_bytes},
                        {"total_bytes", progress->total_bytes},
                        {"percentage", progress->percentage},
                        {"download_speed_bps", download_speed}
                    }},
                    {"timing", {
                        {"start_time", std::chrono::duration_cast<std::chrono::milliseconds>(progress->start_time.time_since_epoch()).count()},
                        {"elapsed_seconds", elapsed_seconds}
                    }}
                };// Add end time and error message if applicable
                if (progress->status != "downloading" && progress->status != "creating_engine") {
                    download_info["timing"]["end_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(progress->end_time.time_since_epoch()).count();
                }

                if (!progress->error_message.empty()) {
                    download_info["error_message"] = progress->error_message;
                }

                if (estimated_remaining_seconds >= 0) {
                    download_info["timing"]["estimated_remaining_seconds"] = estimated_remaining_seconds;
                }

                // Add engine creation info if applicable
                if (progress->engine_params) {
                    download_info["engine_creation"] = {
                        {"engine_id", progress->engine_params->engine_id},
                        {"load_immediately", progress->engine_params->load_immediately},
                        {"main_gpu_id", progress->engine_params->main_gpu_id}
                    };
                }                downloads_array.push_back(download_info);
            }

            // Count startup vs regular downloads
            int startup_count = 0;
            int regular_count = 0;
            for (const auto& pair : active_downloads) {
                if (pair.second->engine_params) {
                    startup_count++;
                } else {
                    regular_count++;
                }
            }

            json response = {
                {"active_downloads", downloads_array},
                {"summary", {
                    {"total_active", active_downloads.size()},
                    {"startup_downloads", startup_count},
                    {"regular_downloads", regular_count}
                }},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("[Thread %u] Successfully provided downloads status - %zu active downloads (%d startup, %d regular)", 
                                std::this_thread::get_id(), active_downloads.size(), startup_count, regular_count);

        } catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error handling downloads status request: %s", std::this_thread::get_id(), ex.what());

            json jError = {
                {"error", {
                    {"message", std::string("Server error: ") + ex.what()},
                    {"type", "server_error"},
                    {"param", nullptr},
                    {"code", nullptr}
                }}
            };

            send_response(sock, 500, jError.dump());
        }
    }

} // namespace kolosal
