#include "kolosal/routes/cancel_download_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <regex>
#include <thread>

using json = nlohmann::json;

namespace kolosal
{

    bool CancelDownloadRoute::match(const std::string &method, const std::string &path)
    {
        // Match DELETE requests to /downloads/{model_id} or /v1/downloads/{model_id}
        if (method != "DELETE" && method != "POST")
        {
            return false;
        }

        std::regex pattern;
        if (method == "DELETE")
        {
            pattern = std::regex(R"(^(?:/v1)?/downloads/([^/]+)$)");
        }
        else
        { // POST method
            pattern = std::regex(R"(^(?:/v1)?/downloads/([^/]+)/cancel$)");
        }

        bool matches = std::regex_match(path, pattern);

        if (matches)
        {
            // Store the path for later use in handle method
            matched_path_ = path;
        }

        return matches;
    }

    void CancelDownloadRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received cancel download request", std::this_thread::get_id());

            // Extract model ID from the stored path
            std::string model_id = extractModelId(matched_path_);

            if (model_id.empty())
            {
                json jError = {
                    {"error", {{"message", "Model ID not found in URL"}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
                send_response(sock, 400, jError.dump());
                return;
            }

            // Get download manager instance and cancel download
            auto &download_manager = DownloadManager::getInstance();

            // Check if download exists and is in progress
            auto progress = download_manager.getDownloadProgress(model_id);
            if (!progress)
            {
                json jError = {
                    {"error", {{"message", "Download not found for model ID: " + model_id}, {"type", "not_found_error"}, {"param", nullptr}, {"code", nullptr}}}};
                send_response(sock, 404, jError.dump());
                return;
            }

            // Check if download is already completed or cancelled
            if (progress->status == "completed" || progress->status == "cancelled" || progress->status == "failed")
            {
                json jError = {
                    {"error", {{"message", "Cannot cancel download. Current status: " + progress->status}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
                send_response(sock, 400, jError.dump());
                return;
            } // Cancel the download
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
                                      .count()}};

                // Add engine creation info if this was a startup download
                if (progress->engine_params)
                {
                    response["engine_creation"] = {
                        {"engine_id", progress->engine_params->engine_id},
                        {"load_immediately", progress->engine_params->load_immediately},
                        {"main_gpu_id", progress->engine_params->main_gpu_id}};
                }

                send_response(sock, 200, response.dump());
                ServerLogger::logInfo("[Thread %u] Successfully cancelled download for model: %s",
                                      std::this_thread::get_id(), model_id.c_str());
            }
            else
            {
                json jError = {
                    {"error", {{"message", "Failed to cancel download for model: " + model_id}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};
                send_response(sock, 500, jError.dump());
            }
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling cancel download request: %s", std::this_thread::get_id(), ex.what());

            json jError = {
                {"error", {{"message", std::string("Server error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 500, jError.dump());
        }
    }

    std::string CancelDownloadRoute::extractModelId(const std::string &path)
    {
        // Extract model ID from paths like:
        // /downloads/{model_id} or /v1/downloads/{model_id} (DELETE)
        // /downloads/{model_id}/cancel or /v1/downloads/{model_id}/cancel (POST)

        std::regex delete_pattern(R"(^(?:/v1)?/downloads/([^/]+)$)");
        std::regex post_pattern(R"(^(?:/v1)?/downloads/([^/]+)/cancel$)");
        std::smatch match;

        if (std::regex_match(path, match, delete_pattern) || std::regex_match(path, match, post_pattern))
        {
            if (match.size() > 1)
            {
                return match[1].str();
            }
        }

        return "";
    }

} // namespace kolosal
