#include "kolosal/routes/downloads_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cmath>

using json = nlohmann::json;

namespace kolosal
{

    bool DownloadsRoute::match(const std::string &method, const std::string &path)
    {
        // Match various download endpoints:
        // GET /downloads or /v1/downloads (all downloads status)
        // GET /downloads/{model_id} or /v1/downloads/{model_id} (specific download progress)
        // DELETE /downloads/{model_id} or /v1/downloads/{model_id} (cancel download)
        // POST /downloads/{model_id}/cancel or /v1/downloads/{model_id}/cancel (cancel download)
        // POST /downloads/{model_id}/pause or /v1/downloads/{model_id}/pause (pause download)
        // POST /downloads/{model_id}/resume or /v1/downloads/{model_id}/resume (resume download)
        // DELETE /downloads or /v1/downloads (cancel all downloads)
        // POST /downloads/cancel or /v1/downloads/cancel (cancel all downloads)
        
        std::regex all_pattern(R"(^(?:/v1)?/downloads$)");
        std::regex single_pattern(R"(^(?:/v1)?/downloads/([^/]+)$)");
        std::regex cancel_pattern(R"(^(?:/v1)?/downloads/([^/]+)/cancel$)");
        std::regex pause_pattern(R"(^(?:/v1)?/downloads/([^/]+)/pause$)");
        std::regex resume_pattern(R"(^(?:/v1)?/downloads/([^/]+)/resume$)");
        std::regex cancel_all_pattern(R"(^(?:/v1)?/downloads/cancel$)");
        
        bool matches = false;
        
        if (method == "GET")
        {
            matches = std::regex_match(path, all_pattern) || std::regex_match(path, single_pattern);
        }
        else if (method == "DELETE")
        {
            matches = std::regex_match(path, all_pattern) || std::regex_match(path, single_pattern);
        }
        else if (method == "POST")
        {
            matches = std::regex_match(path, cancel_pattern) || 
                     std::regex_match(path, pause_pattern) ||
                     std::regex_match(path, resume_pattern) ||
                     std::regex_match(path, cancel_all_pattern) ||
                     std::regex_match(path, all_pattern); // POST to /downloads for cancel all
        }

        if (matches)
        {
            // Store the path for later use in handle method
            matched_path_ = path;
        }

        return matches;
    }

    void DownloadsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            // Get HTTP method from the socket (assuming we can determine it)
            // For now, we'll parse the path to determine the action
            
            std::regex all_pattern(R"(^(?:/v1)?/downloads$)");
            std::regex single_pattern(R"(^(?:/v1)?/downloads/([^/]+)$)");
            std::regex cancel_pattern(R"(^(?:/v1)?/downloads/([^/]+)/cancel$)");
            std::regex pause_pattern(R"(^(?:/v1)?/downloads/([^/]+)/pause$)");
            std::regex resume_pattern(R"(^(?:/v1)?/downloads/([^/]+)/resume$)");
            std::regex cancel_all_pattern(R"(^(?:/v1)?/downloads/cancel$)");
            
            // Determine the action based on path patterns
            if (std::regex_match(matched_path_, cancel_pattern))
            {
                // Handle cancel single download
                std::string model_id = extractModelId(matched_path_);
                if (!model_id.empty())
                {
                    handleCancelDownload(sock, model_id);
                }
                else
                {
                    json jError = {
                        {"error", {
                            {"message", "Cannot extract model ID from cancel request path"}, 
                            {"type", "invalid_request_error"}, 
                            {"param", "path"}, 
                            {"code", "invalid_path_format"}
                        }}
                    };
                    send_response(sock, 400, jError.dump());
                }
            }
            else if (std::regex_match(matched_path_, pause_pattern))
            {
                // Handle pause download
                std::string model_id = extractModelId(matched_path_);
                if (!model_id.empty())
                {
                    handlePauseDownload(sock, model_id);
                }
                else
                {
                    json jError = {
                        {"error", {
                            {"message", "Cannot extract model ID from pause request path"}, 
                            {"type", "invalid_request_error"}, 
                            {"param", "path"}, 
                            {"code", "invalid_path_format"}
                        }}
                    };
                    send_response(sock, 400, jError.dump());
                }
            }
            else if (std::regex_match(matched_path_, resume_pattern))
            {
                // Handle resume download
                std::string model_id = extractModelId(matched_path_);
                if (!model_id.empty())
                {
                    handleResumeDownload(sock, model_id);
                }
                else
                {
                    json jError = {
                        {"error", {
                            {"message", "Cannot extract model ID from resume request path"}, 
                            {"type", "invalid_request_error"}, 
                            {"param", "path"}, 
                            {"code", "invalid_path_format"}
                        }}
                    };
                    send_response(sock, 400, jError.dump());
                }
            }
            else if (std::regex_match(matched_path_, cancel_all_pattern) || 
                    (std::regex_match(matched_path_, all_pattern) && body.find("cancel") != std::string::npos))
            {
                // Handle cancel all downloads (either /downloads/cancel or POST to /downloads with cancel action)
                handleCancelAllDownloads(sock);
            }
            else if (std::regex_match(matched_path_, all_pattern))
            {
                // Handle all downloads status (GET /downloads)
                handleAllDownloads(sock);
            }
            else if (std::regex_match(matched_path_, single_pattern))
            {
                // Handle specific download progress or cancel (depending on method)
                std::string model_id = extractModelId(matched_path_);
                if (model_id.empty())
                {
                    json jError = {
                        {"error", {
                            {"message", "Cannot extract model ID from request path. Please ensure the URL format is /downloads/{model-id}"}, 
                            {"type", "invalid_request_error"}, 
                            {"param", "path"}, 
                            {"code", "invalid_path_format"}
                        }}
                    };

                    send_response(sock, 400, jError.dump());
                    ServerLogger::logError("[Thread %u] Cannot extract model ID from request path: %s", std::this_thread::get_id(), matched_path_.c_str());
                    return;
                }
                
                // This could be either GET (progress) or DELETE (cancel)
                // For now, we'll assume GET for progress since we need to determine HTTP method
                // In a real implementation, you'd check the actual HTTP method
                handleSingleDownload(sock, model_id);
            }
            else
            {
                json jError = {
                    {"error", {
                        {"message", "Invalid downloads endpoint"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "path"}, 
                        {"code", "invalid_endpoint"}
                    }}
                };
                send_response(sock, 400, jError.dump());
            }
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling downloads request: %s", std::this_thread::get_id(), ex.what());

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

    std::string DownloadsRoute::extractModelId(const std::string &path)
    {
        // Handle different path patterns:
        // /downloads/{model_id} or /v1/downloads/{model_id}
        // /downloads/{model_id}/cancel or /v1/downloads/{model_id}/cancel
        // /downloads/{model_id}/pause or /v1/downloads/{model_id}/pause
        // /downloads/{model_id}/resume or /v1/downloads/{model_id}/resume
        
        std::regex single_pattern(R"(^(?:/v1)?/downloads/([^/]+)$)");
        std::regex action_pattern(R"(^(?:/v1)?/downloads/([^/]+)/(?:cancel|pause|resume)$)");
        std::smatch match;

        if (std::regex_match(path, match, single_pattern) || 
            std::regex_match(path, match, action_pattern))
        {
            if (match.size() > 1)
            {
                return match[1].str();
            }
        }

        return "";
    }

    void DownloadsRoute::handleSingleDownload(SocketType sock, const std::string& model_id)
    {
        ServerLogger::logInfo("[Thread %u] Received download progress request for model: %s", std::this_thread::get_id(), model_id.c_str());

        // Get download progress from download manager
        auto &download_manager = DownloadManager::getInstance();
        auto progress = download_manager.getDownloadProgress(model_id);

        if (!progress)
        {
            // No download found for this model ID
            json jError = {
                {"error", {
                    {"message", "No download found for model ID: " + model_id}, 
                    {"type", "not_found_error"}, 
                    {"param", "model_id"}, 
                    {"code", "download_not_found"}
                }}
            };

            send_response(sock, 404, jError.dump());
            ServerLogger::logInfo("[Thread %u] No download found for model ID: %s", std::this_thread::get_id(), model_id.c_str());
            return;
        }

        // Calculate elapsed time
        auto now = std::chrono::system_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                   (progress->status == "downloading" || progress->status == "creating_engine" ? now : progress->end_time) - progress->start_time)
                                   .count();

        // Calculate download speed (bytes per second)
        double download_speed = elapsed_seconds > 0 ? static_cast<double>(progress->downloaded_bytes) / elapsed_seconds : 0.0;

        // Estimate remaining time (only for active downloads)
        int estimated_remaining_seconds = -1;
        if (progress->status == "downloading" && progress->percentage > 0 && download_speed > 0)
        {
            size_t remaining_bytes = progress->total_bytes - progress->downloaded_bytes;
            estimated_remaining_seconds = static_cast<int>(remaining_bytes / download_speed);
        }

        // Ensure percentage is valid before sending response
        double safe_percentage = progress->percentage;
        if (std::isnan(safe_percentage) || std::isinf(safe_percentage) || safe_percentage < 0.0 || safe_percentage > 100.0)
        {
            ServerLogger::logWarning("Invalid percentage value %.2f for model %s in API response, using 0.0", 
                                   progress->percentage, model_id.c_str());
            safe_percentage = 0.0;
        }

        // Build response JSON
        json response = {
            {"model_id", progress->model_id},
            {"status", progress->status},
            {"url", progress->url},
            {"local_path", progress->local_path},
            {"progress", {
                {"downloaded_bytes", progress->downloaded_bytes}, 
                {"total_bytes", progress->total_bytes}, 
                {"percentage", safe_percentage}, 
                {"download_speed_bps", download_speed}
            }},
            {"timing", {
                {"start_time", std::chrono::duration_cast<std::chrono::milliseconds>(progress->start_time.time_since_epoch()).count()}, 
                {"elapsed_seconds", elapsed_seconds}
            }}
        };

        // Add end time and error message if applicable
        if (progress->status != "downloading" && progress->status != "creating_engine")
        {
            response["timing"]["end_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(progress->end_time.time_since_epoch()).count();
        }

        if (!progress->error_message.empty())
        {
            response["error_message"] = progress->error_message;
        }

        if (estimated_remaining_seconds >= 0)
        {
            response["timing"]["estimated_remaining_seconds"] = estimated_remaining_seconds;
        }

        // Add engine creation info if applicable
        if (progress->engine_params)
        {
            response["engine_creation"] = {
                {"model_id", progress->engine_params->model_id},
                {"load_immediately", progress->engine_params->load_immediately},
                {"main_gpu_id", progress->engine_params->main_gpu_id}
            };
        }

        send_response(sock, 200, response.dump());
        ServerLogger::logInfo("[Thread %u] Successfully provided download progress for model: %s (%.1f%%)",
                              std::this_thread::get_id(), model_id.c_str(), progress->percentage);
    }

    void DownloadsRoute::handleAllDownloads(SocketType sock)
    {
        ServerLogger::logInfo("[Thread %u] Received downloads status request", std::this_thread::get_id());

        // Get all active downloads from download manager
        auto &download_manager = DownloadManager::getInstance();
        auto active_downloads = download_manager.getAllActiveDownloads();

        json downloads_array = json::array();

        for (const auto &pair : active_downloads)
        {
            const auto &progress = pair.second;
            // Calculate elapsed time
            auto now = std::chrono::system_clock::now();
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                       (progress->status == "downloading" || progress->status == "creating_engine" ? now : progress->end_time) - progress->start_time)
                                       .count();

            // Calculate download speed (bytes per second)
            double download_speed = elapsed_seconds > 0 ? static_cast<double>(progress->downloaded_bytes) / elapsed_seconds : 0.0;

            // Estimate remaining time (only for active downloads)
            int estimated_remaining_seconds = -1;
            if (progress->status == "downloading" && progress->percentage > 0 && download_speed > 0)
            {
                size_t remaining_bytes = progress->total_bytes - progress->downloaded_bytes;
                estimated_remaining_seconds = static_cast<int>(remaining_bytes / download_speed);
            }

            // Ensure percentage is valid before adding to response
            double safe_percentage = progress->percentage;
            if (std::isnan(safe_percentage) || std::isinf(safe_percentage) || safe_percentage < 0.0 || safe_percentage > 100.0)
            {
                ServerLogger::logWarning("Invalid percentage value %.2f for model %s in downloads status, using 0.0", 
                                       progress->percentage, progress->model_id.c_str());
                safe_percentage = 0.0;
            }

            json download_info = {
                {"model_id", progress->model_id},
                {"status", progress->status},
                {"download_type", progress->engine_params ? "startup" : "regular"},
                {"url", progress->url},
                {"local_path", progress->local_path},
                {"progress", {
                    {"downloaded_bytes", progress->downloaded_bytes}, 
                    {"total_bytes", progress->total_bytes}, 
                    {"percentage", safe_percentage}, 
                    {"download_speed_bps", download_speed}
                }},
                {"timing", {
                    {"start_time", std::chrono::duration_cast<std::chrono::milliseconds>(progress->start_time.time_since_epoch()).count()}, 
                    {"elapsed_seconds", elapsed_seconds}
                }}
            };

            // Add end time and error message if applicable
            if (progress->status != "downloading" && progress->status != "creating_engine")
            {
                download_info["timing"]["end_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(progress->end_time.time_since_epoch()).count();
            }

            if (!progress->error_message.empty())
            {
                download_info["error_message"] = progress->error_message;
            }

            if (estimated_remaining_seconds >= 0)
            {
                download_info["timing"]["estimated_remaining_seconds"] = estimated_remaining_seconds;
            }

            // Add engine creation info if applicable
            if (progress->engine_params)
            {
                download_info["engine_creation"] = {
                    {"model_id", progress->engine_params->model_id},
                    {"load_immediately", progress->engine_params->load_immediately},
                    {"main_gpu_id", progress->engine_params->main_gpu_id}
                };
            }
            downloads_array.push_back(download_info);
        }

        // Count startup vs regular downloads
        int startup_count = 0;
        int regular_count = 0;
        for (const auto &pair : active_downloads)
        {
            if (pair.second->engine_params)
            {
                startup_count++;
            }
            else
            {
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
                              std::chrono::system_clock::now().time_since_epoch())
                              .count()}
        };

        send_response(sock, 200, response.dump());
        ServerLogger::logInfo("[Thread %u] Successfully provided downloads status - %zu active downloads (%d startup, %d regular)",
                              std::this_thread::get_id(), active_downloads.size(), startup_count, regular_count);
    }

    void DownloadsRoute::handleCancelDownload(SocketType sock, const std::string& model_id)
    {
        ServerLogger::logInfo("[Thread %u] Received cancel download request for model: %s", std::this_thread::get_id(), model_id.c_str());

        // Get download manager instance and cancel download
        auto &download_manager = DownloadManager::getInstance();

        // Check if download exists and is in progress
        auto progress = download_manager.getDownloadProgress(model_id);
        if (!progress)
        {
            json jError = {
                {"error", {
                    {"message", "Download not found for model ID: " + model_id}, 
                    {"type", "not_found_error"}, 
                    {"param", nullptr}, 
                    {"code", nullptr}
                }}
            };
            send_response(sock, 404, jError.dump());
            return;
        }

        // Check if download is already completed or cancelled
        if (progress->status == "completed" || progress->status == "cancelled" || progress->status == "failed")
        {
            json jError = {
                {"error", {
                    {"message", "Cannot cancel download. Current status: " + progress->status}, 
                    {"type", "invalid_request_error"}, 
                    {"param", nullptr}, 
                    {"code", nullptr}
                }}
            };
            send_response(sock, 400, jError.dump());
            return;
        }

        // Cancel the download
        bool cancelled = download_manager.cancelDownload(model_id);

        if (cancelled)
        {
            json response = {
                {"success", true},
                {"message", "Download cancelled successfully"},
                {"model_id", model_id},
                {"previous_status", progress->status},
                {"download_type", progress->engine_params ? "startup" : "regular"},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count()}
            };

            // Add engine creation info if this was a startup download
            if (progress->engine_params)
            {
                response["engine_creation"] = {
                    {"model_id", progress->engine_params->model_id},
                    {"load_immediately", progress->engine_params->load_immediately},
                    {"main_gpu_id", progress->engine_params->main_gpu_id}
                };
            }

            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("[Thread %u] Successfully cancelled download for model: %s",
                                  std::this_thread::get_id(), model_id.c_str());
        }
        else
        {
            json jError = {
                {"error", {
                    {"message", "Failed to cancel download for model: " + model_id}, 
                    {"type", "server_error"}, 
                    {"param", nullptr}, 
                    {"code", nullptr}
                }}
            };
            send_response(sock, 500, jError.dump());
        }
    }

    void DownloadsRoute::handleCancelAllDownloads(SocketType sock)
    {
        ServerLogger::logInfo("[Thread %u] Received cancel all downloads request", std::this_thread::get_id());

        auto &download_manager = DownloadManager::getInstance();
        auto active_downloads = download_manager.getAllActiveDownloads();
        json cancelled_downloads = json::array();
        json failed_cancellations = json::array();
        int successful_cancellations = 0;
        int startup_cancellations = 0;
        int regular_cancellations = 0;

        for (const auto &pair : active_downloads)
        {
            const std::string &model_id = pair.first;
            const auto &progress = pair.second;

            // Skip downloads that are already completed, failed, or cancelled
            if (progress->status == "completed" || progress->status == "failed" || progress->status == "cancelled")
            {
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

            if (cancelled)
            {
                bool is_startup = progress->engine_params != nullptr;
                json cancel_info = {
                    {"model_id", model_id},
                    {"previous_status", progress->status},
                    {"download_type", is_startup ? "startup" : "regular"},
                    {"cancelled_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count()}
                };

                // Add engine creation info if this was a startup download
                if (is_startup)
                {
                    cancel_info["engine_creation"] = {
                        {"model_id", progress->engine_params->model_id},
                        {"load_immediately", progress->engine_params->load_immediately},
                        {"main_gpu_id", progress->engine_params->main_gpu_id}
                    };
                    startup_cancellations++;
                }
                else
                {
                    regular_cancellations++;
                }

                cancelled_downloads.push_back(cancel_info);
                successful_cancellations++;

                ServerLogger::logInfo("[Thread %u] Successfully cancelled %s download for model: %s",
                                      std::this_thread::get_id(), is_startup ? "startup" : "regular", model_id.c_str());
            }
            else
            {
                json fail_info = {
                    {"model_id", model_id},
                    {"status", progress->status},
                    {"reason", "Failed to cancel"}
                };
                failed_cancellations.push_back(fail_info);

                ServerLogger::logWarning("[Thread %u] Failed to cancel download for model: %s",
                                         std::this_thread::get_id(), model_id.c_str());
            }
        }

        json response = {
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
                              std::chrono::system_clock::now().time_since_epoch())
                              .count()}
        };

        send_response(sock, 200, response.dump());
        ServerLogger::logInfo("[Thread %u] Bulk cancellation completed: %d successful (%d startup, %d regular), %d failed",
                              std::this_thread::get_id(), successful_cancellations, startup_cancellations,
                              regular_cancellations, static_cast<int>(failed_cancellations.size()));
    }

    void DownloadsRoute::handlePauseDownload(SocketType sock, const std::string& model_id)
    {
        ServerLogger::logInfo("[Thread %u] Received pause download request for model: %s", std::this_thread::get_id(), model_id.c_str());

        // Get the download manager and pause the download
        auto &download_manager = DownloadManager::getInstance();
        bool success = download_manager.pauseDownload(model_id);

        if (success)
        {
            ServerLogger::logInfo("Successfully paused download for model: %s", model_id.c_str());

            json response = {
                {"success", true},
                {"message", "Download paused successfully"},
                {"model_id", model_id}
            };

            send_response(sock, 200, response.dump());
        }
        else
        {
            std::string error_msg = "Could not pause download for model: " + model_id + 
                                   " (download may not exist or may not be in progress)";
            ServerLogger::logWarning("%s", error_msg.c_str());

            json response = {
                {"success", false},
                {"error", error_msg},
                {"model_id", model_id}
            };

            send_response(sock, 404, response.dump());
        }
    }

    void DownloadsRoute::handleResumeDownload(SocketType sock, const std::string& model_id)
    {
        ServerLogger::logInfo("[Thread %u] Received resume download request for model: %s", std::this_thread::get_id(), model_id.c_str());

        // Get the download manager and resume the download
        auto &download_manager = DownloadManager::getInstance();
        bool success = download_manager.resumeDownload(model_id);

        if (success)
        {
            ServerLogger::logInfo("Successfully resumed download for model: %s", model_id.c_str());

            json response = {
                {"success", true},
                {"message", "Download resumed successfully"},
                {"model_id", model_id}
            };

            send_response(sock, 200, response.dump());
        }
        else
        {
            std::string error_msg = "Could not resume download for model: " + model_id + 
                                   " (download may not exist or may not be paused)";
            ServerLogger::logWarning("%s", error_msg.c_str());

            json response = {
                {"success", false},
                {"error", error_msg},
                {"model_id", model_id}
            };

            send_response(sock, 404, response.dump());
        }
    }

} // namespace kolosal
