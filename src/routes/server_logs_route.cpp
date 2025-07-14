#include "kolosal/routes/server_logs_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace kolosal
{

    bool ServerLogsRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "GET" && (path == "/logs" || path == "/v1/logs" || path == "/server/logs"));
    }

    void ServerLogsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logDebug("[Thread %u] Received server logs request", std::this_thread::get_id());

            // Get the ServerLogger instance and retrieve logs
            auto &logger = ServerLogger::instance();
            const auto &logs = logger.getLogs();

            json logsList = json::array();
            for (const auto &logEntry : logs)
            {
                std::string levelStr;
                switch (logEntry.level)
                {
                case LogLevel::SERVER_ERROR:
                    levelStr = "ERROR";
                    break;
                case LogLevel::SERVER_WARNING:
                    levelStr = "WARNING";
                    break;
                case LogLevel::SERVER_INFO:
                    levelStr = "INFO";
                    break;
                case LogLevel::SERVER_DEBUG:
                    levelStr = "DEBUG";
                    break;
                default:
                    levelStr = "UNKNOWN";
                    break;
                }

                json logObject = {
                    {"level", levelStr},
                    {"timestamp", logEntry.timestamp},
                    {"message", logEntry.message}
                };

                logsList.push_back(logObject);
            }

            // Get current timestamp for the response
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            std::string currentTimestamp = ss.str();

            json response = {
                {"logs", logsList},
                {"total_count", logsList.size()},
                {"retrieved_at", currentTimestamp}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logDebug("[Thread %u] Successfully retrieved %zu log entries", std::this_thread::get_id(), logsList.size());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling server logs request: %s", std::this_thread::get_id(), ex.what());

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
