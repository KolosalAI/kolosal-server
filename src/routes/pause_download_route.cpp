#include "kolosal/routes/pause_download_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <regex>
#include <thread>

using json = nlohmann::json;

namespace kolosal
{

    bool PauseDownloadRoute::match(const std::string &method, const std::string &path)
    {
        // Match POST requests to /downloads/{model_id}/pause or /v1/downloads/{model_id}/pause
        if (method != "POST")
        {
            return false;
        }

        std::regex pattern(R"(^(?:/v1)?/downloads/([^/]+)/pause$)");
        bool matches = std::regex_match(path, pattern);

        if (matches)
        {
            // Store the path for later use in handle method
            matched_path_ = path;
        }

        return matches;
    }

    void PauseDownloadRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received pause download request", std::this_thread::get_id());

            // Extract model ID from the stored path
            std::string model_id = extractModelId(matched_path_);

            if (model_id.empty())
            {
                std::string error_msg = "Invalid model ID in path: " + matched_path_;
                ServerLogger::logError("%s", error_msg.c_str());

                json response = {
                    {"success", false},
                    {"error", error_msg}
                };

                send_response(sock, 400, response.dump());
                return;
            }

            ServerLogger::logInfo("Attempting to pause download for model: %s", model_id.c_str());

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
        catch (const std::exception &e)
        {
            std::string error_msg = std::string("Error pausing download: ") + e.what();
            ServerLogger::logError("%s", error_msg.c_str());

            json response = {
                {"success", false},
                {"error", error_msg}
            };

            send_response(sock, 500, response.dump());
        }
    }

    std::string PauseDownloadRoute::extractModelId(const std::string &path)
    {
        // Extract model ID from paths like /downloads/{model_id}/pause or /v1/downloads/{model_id}/pause
        std::regex pattern(R"(^(?:/v1)?/downloads/([^/]+)/pause$)");
        std::smatch matches;

        if (std::regex_match(path, matches, pattern) && matches.size() > 1)
        {
            return matches[1].str();
        }

        return "";
    }

} // namespace kolosal
