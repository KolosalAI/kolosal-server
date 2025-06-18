#include "kolosal/routes/cancel_all_downloads_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <thread>

using json = nlohmann::json;

namespace kolosal {

    bool CancelAllDownloadsRoute::match(const std::string& method, const std::string& path) {
        // Match DELETE or POST requests to /downloads or /v1/downloads
        return ((method == "DELETE" || method == "POST") && 
                (path == "/downloads" || path == "/v1/downloads" ||
                 path == "/downloads/cancel" || path == "/v1/downloads/cancel"));
    }

    void CancelAllDownloadsRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Received cancel all downloads request", std::this_thread::get_id());

            auto& download_manager = DownloadManager::getInstance();
            auto active_downloads = download_manager.getAllActiveDownloads();            json cancelled_downloads = json::array();
            json failed_cancellations = json::array();
            int successful_cancellations = 0;
            int startup_cancellations = 0;
            int regular_cancellations = 0;

            for (const auto& pair : active_downloads) {
                const std::string& model_id = pair.first;
                const auto& progress = pair.second;

                // Skip downloads that are already completed, failed, or cancelled
                if (progress->status == "completed" || progress->status == "failed" || progress->status == "cancelled") {
                    json skip_info = {
                        {"model_id", model_id},
                        {"status", progress->status},
                        {"reason", "Already completed, failed, or cancelled"}
                    };
                    failed_cancellations.push_back(skip_info);
                    continue;
                }

                // Attempt to cancel the download
                bool cancelled = download_manager.cancelDownload(model_id);
                
                if (cancelled) {
                    bool is_startup = progress->engine_params != nullptr;
                    json cancel_info = {
                        {"model_id", model_id},
                        {"previous_status", progress->status},
                        {"download_type", is_startup ? "startup" : "regular"},
                        {"cancelled_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    };
                    
                    // Add engine creation info if this was a startup download
                    if (is_startup) {
                        cancel_info["engine_creation"] = {
                            {"engine_id", progress->engine_params->engine_id},
                            {"load_immediately", progress->engine_params->load_immediately},
                            {"main_gpu_id", progress->engine_params->main_gpu_id}
                        };
                        startup_cancellations++;
                    } else {
                        regular_cancellations++;
                    }
                    
                    cancelled_downloads.push_back(cancel_info);
                    successful_cancellations++;
                    
                    ServerLogger::logInfo("[Thread %u] Successfully cancelled %s download for model: %s", 
                                        std::this_thread::get_id(), is_startup ? "startup" : "regular", model_id.c_str());
                } else {
                    json fail_info = {
                        {"model_id", model_id},
                        {"status", progress->status},
                        {"reason", "Failed to cancel"}
                    };
                    failed_cancellations.push_back(fail_info);
                    
                    ServerLogger::logWarning("[Thread %u] Failed to cancel download for model: %s", 
                                           std::this_thread::get_id(), model_id.c_str());
                }
            }            json response = {
                {"success", true},
                {"message", "Bulk download cancellation completed"},
                {"summary", {
                    {"total_downloads", active_downloads.size()},
                    {"successful_cancellations", successful_cancellations},
                    {"failed_cancellations", failed_cancellations.size()},
                    {"startup_cancellations", startup_cancellations},
                    {"regular_cancellations", regular_cancellations}
                }},
                {"cancelled_downloads", cancelled_downloads},
                {"failed_cancellations", failed_cancellations},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("[Thread %u] Bulk cancellation completed: %d successful (%d startup, %d regular), %d failed", 
                                std::this_thread::get_id(), successful_cancellations, startup_cancellations, 
                                regular_cancellations, static_cast<int>(failed_cancellations.size()));

        } catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error handling cancel all downloads request: %s", std::this_thread::get_id(), ex.what());

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
