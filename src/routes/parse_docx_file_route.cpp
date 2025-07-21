#include "kolosal/routes/parse_docx_file_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/parse_docx.hpp"
#include <json.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <filesystem>

using json = nlohmann::json;

namespace kolosal
{
    namespace
    {
        void sendJsonResponse(SocketType sock, const json &response, int status_code = 200)
        {
            std::string status_text;
            switch (status_code)
            {
            case 200:
                status_text = "OK";
                break;
            case 400:
                status_text = "Bad Request";
                break;
            case 404:
                status_text = "Not Found";
                break;
            case 500:
                status_text = "Internal Server Error";
                break;
            default:
                status_text = "Unknown";
                break;
            }

            std::string response_str = response.dump();
            std::ostringstream http_response;
            http_response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
            http_response << "Content-Type: application/json\r\n";
            http_response << "Content-Length: " << response_str.length() << "\r\n";
            http_response << "Access-Control-Allow-Origin: *\r\n";
            http_response << "Access-Control-Allow-Methods: POST, OPTIONS\r\n";
            http_response << "Access-Control-Allow-Headers: Content-Type\r\n";
            http_response << "\r\n";
            http_response << response_str;

            std::string full_response = http_response.str();
            send(sock, full_response.c_str(), static_cast<int>(full_response.length()), 0);
        }
    }

    bool ParseDOCXFileRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "POST" && path == "/parse-docx");
    }

    void ParseDOCXFileRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received DOCX file parse request", std::this_thread::get_id());

            // Handle OPTIONS request for CORS
            if (body.empty())
            {
                json response = {
                    {"message", "DOCX file parse endpoint ready"},
                    {"methods", {"POST"}},
                    {"description", "Send file path to parse DOCX text"}};
                sendJsonResponse(sock, response, 200);
                return;
            }

            // Parse JSON request
            json request;
            try
            {
                request = json::parse(body);
            }
            catch (const json::parse_error &e)
            {
                json error_response = {
                    {"error", "Invalid JSON format"},
                    {"details", e.what()}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            // Validate required fields
            if (!request.contains("file_path") || !request["file_path"].is_string())
            {
                json error_response = {
                    {"error", "Missing or invalid 'file_path' field"},
                    {"details", "Expected file path as string"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            std::string file_path = request["file_path"];
            if (file_path.empty())
            {
                json error_response = {
                    {"error", "Empty file path"},
                    {"details", "File path cannot be empty"}};
                sendJsonResponse(sock, error_response, 400);
                return;
            }

            // Check if file exists
            if (!std::filesystem::exists(file_path))
            {
                json error_response = {
                    {"error", "Failed to parse DOCX: " + file_path},
                    {"details", "File not found"}};
                sendJsonResponse(sock, error_response, 404);
                return;
            }

            ServerLogger::logInfo("Parsing DOCX file: %s", file_path.c_str());

            // Parse DOCX from file using the DOCXParser
            std::string parsed_text;
            bool parsing_success = false;
            std::string error_message = "";

            try
            {
                parsed_text = retrieval::DOCXParser::parse_docx(file_path);
                parsing_success = true;
            }
            catch (const std::exception &e)
            {
                parsing_success = false;
                error_message = e.what();
                ServerLogger::logError("DOCX parsing failed: %s", e.what());
            }

            // Create response
            json response;
            if (parsing_success)
            {
                response = {
                    {"success", true},
                    {"text", parsed_text},
                    {"pages_processed", 1}, // DOCX doesn't have pages like PDF, so we'll use 1
                    {"file_path", file_path}};
                ServerLogger::logInfo("DOCX parsing completed successfully. Text length: %zu",
                                      parsed_text.length());
            }
            else
            {
                response = {
                    {"success", false},
                    {"error", "Failed to parse DOCX: " + file_path},
                    {"details", error_message},
                    {"pages_processed", 0},
                    {"file_path", file_path}};
                ServerLogger::logError("DOCX parsing failed: %s", error_message.c_str());
            }

            sendJsonResponse(sock, response, parsing_success ? 200 : 404);
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Exception in ParseDOCXFileRoute::handle: %s", e.what());
            json error_response = {
                {"success", false},
                {"error", "Internal server error"},
                {"details", e.what()}};
            sendJsonResponse(sock, error_response, 500);
        }
    }

} // namespace kolosal
