#include "kolosal/routes/download_progress_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <regex>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace kolosal
{

    bool DownloadProgressRoute::match(const std::string &method, const std::string &path)
    {
        // Match GET requests to /download-progress/{model_id} or /v1/download-progress/{model_id}
        if (method != "GET")
        {
            return false;
        }

        std::regex pattern(R"(^(?:/v1)?/download-progress/([^/]+)$)");
        bool matches = std::regex_match(path, pattern);

        if (matches)
        {
            // Store the path for later use in handle method
            matched_path_ = path;
        }

        return matches;
    }

    void DownloadProgressRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received download progress request", std::this_thread::get_id());

            // Extract model ID from the stored path
            std::string model_id = extractModelId(matched_path_);

            if (model_id.empty())
            {
                json jError = {
                    {"error", {{"message", "Cannot extract model ID from request path. Please ensure the URL format is /download-progress/{model-id}"}, {"type", "invalid_request_error"}, {"param", "path"}, {"code", "invalid_path_format"}}}};

                send_response(sock, 400, jError.dump());
                ServerLogger::logError("[Thread %u] Cannot extract model ID from request path: %s", std::this_thread::get_id(), matched_path_.c_str());
                return;
            }

            // Get download progress from download manager
            auto &download_manager = DownloadManager::getInstance();
            auto progress = download_manager.getDownloadProgress(model_id);

            if (!progress)
            {
                // No download found for this model ID
                json jError = {
                    {"error", {{"message", "No download found for model ID: " + model_id}, {"type", "not_found_error"}, {"param", "model_id"}, {"code", "download_not_found"}}}};

                send_response(sock, 404, jError.dump());
                ServerLogger::logInfo("[Thread %u] No download found for model ID: %s", std::this_thread::get_id(), model_id.c_str());
                return;
            } // Calculate elapsed time
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

            // Build response JSON
            json response = {
                {"model_id", progress->model_id},
                {"status", progress->status},
                {"url", progress->url},
                {"local_path", progress->local_path},
                {"progress", {{"downloaded_bytes", progress->downloaded_bytes}, {"total_bytes", progress->total_bytes}, {"percentage", progress->percentage}, {"download_speed_bps", download_speed}}},
                {"timing", {{"start_time", std::chrono::duration_cast<std::chrono::milliseconds>(progress->start_time.time_since_epoch()).count()}, {"elapsed_seconds", elapsed_seconds}}}}; // Add end time and error message if applicable
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
                    {"engine_id", progress->engine_params->engine_id},
                    {"load_immediately", progress->engine_params->load_immediately},
                    {"main_gpu_id", progress->engine_params->main_gpu_id}};
            }

            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("[Thread %u] Successfully provided download progress for model: %s (%.1f%%)",
                                  std::this_thread::get_id(), model_id.c_str(), progress->percentage);
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling download progress request: %s", std::this_thread::get_id(), ex.what());

            json jError = {
                {"error", {{"message", std::string("Server error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 500, jError.dump());
        }
    }

    std::string DownloadProgressRoute::extractModelId(const std::string &path)
    {
        std::regex pattern(R"(^(?:/v1)?/download-progress/([^/]+)$)");
        std::smatch match;

        if (std::regex_match(path, match, pattern))
        {
            return match[1].str();
        }

        return "";
    }

} // namespace kolosal
