#include "kolosal/routes/parse_html_route.hpp"
#include "kolosal/retrieval/parse_html.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <memory>

using json = nlohmann::json;

namespace kolosal
{

    bool ParseHtmlRoute::match(const std::string& method, const std::string& path)
    {
        return method == "POST" && path == "/parse_html";
    }

    void ParseHtmlRoute::handle(SocketType sock, const std::string& body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received parse HTML request", std::this_thread::get_id());

            // Check for empty body
            if (body.empty())
            {
                json errorResponse;
                errorResponse["success"] = false;
                errorResponse["error"] = "Request body is empty";
                sendJsonResponse(sock, errorResponse, 400);
                return;
            }

            // Parse JSON request
            json request;
            try
            {
                request = json::parse(body);
            }
            catch (const json::parse_error& ex)
            {
                ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
                json errorResponse;
                errorResponse["success"] = false;
                errorResponse["error"] = "Invalid JSON: " + std::string(ex.what());
                sendJsonResponse(sock, errorResponse, 400);
                return;
            }

            // Check if HTML field exists
            if (!request.contains("html") || !request["html"].is_string())
            {
                json errorResponse;
                errorResponse["success"] = false;
                errorResponse["error"] = "Missing or invalid 'html' field in request";
                sendJsonResponse(sock, errorResponse, 400);
                return;
            }

            std::string html = request["html"].get<std::string>();
            
            ServerLogger::logDebug("[Thread %u] Converting HTML to Markdown (length: %zu)", 
                                  std::this_thread::get_id(), html.length());

            // Create HTML parser and parse the content
            retrieval::HtmlParser parser;
            retrieval::HtmlParseResult result = parser.parseHtmlSync(html);

            // Create response
            json response;
            if (result.success)
            {
                response["success"] = true;
                response["markdown"] = result.markdown;
                response["elements_processed"] = result.elements_processed;
                
                ServerLogger::logInfo("[Thread %u] Successfully converted HTML to Markdown", 
                                      std::this_thread::get_id());
                sendJsonResponse(sock, response, 200);
            }
            else
            {
                response["success"] = false;
                response["error"] = result.error_message;
                response["elements_processed"] = result.elements_processed;
                
                ServerLogger::logError("[Thread %u] HTML parsing failed: %s", 
                                       std::this_thread::get_id(), result.error_message.c_str());
                sendJsonResponse(sock, response, 500);
            }
        }
        catch (const json::exception& ex)
        {
            ServerLogger::logError("[Thread %u] JSON error: %s", std::this_thread::get_id(), ex.what());
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "JSON error: " + std::string(ex.what());
            sendJsonResponse(sock, errorResponse, 500);
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("[Thread %u] Error handling parse HTML request: %s", 
                                   std::this_thread::get_id(), ex.what());
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Internal server error: " + std::string(ex.what());
            sendJsonResponse(sock, errorResponse, 500);
        }
    }

    void ParseHtmlRoute::sendJsonResponse(SocketType sock, const json& response, int status_code)
    {
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, status_code, response.dump(), headers);
    }

} // namespace kolosal
