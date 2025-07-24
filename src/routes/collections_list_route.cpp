#include "kolosal/routes/collections_list_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes
{

CollectionsListRoute::CollectionsListRoute()
{
    ServerLogger::logInfo("CollectionsListRoute initialized");
}

CollectionsListRoute::~CollectionsListRoute() = default;

bool CollectionsListRoute::match(const std::string& method, const std::string& path)
{
    // Match GET /api/v1/collections, GET /api/v1/collections/{id}, and GET /collections
    std::regex collections_pattern(R"(^/(?:api/v1/)?collections(?:/([^/]+))?$)");
    return method == "GET" && std::regex_match(path, collections_pattern);
}

void CollectionsListRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("Collections list request received");
        
        // Note: We would need to extract path from request to determine if it's a specific collection
        // For now, we'll assume listing all collections
        
        // Simulate getting collections from the document service
        try
        {
            // In a real implementation, this would query the document service for collections
            json response;
            response["success"] = true;
            response["collections"] = json::array({
                {
                    {"name", "documents"},
                    {"id", "documents"},
                    {"description", "Default document collection"},
                    {"document_count", 0},
                    {"created_at", "2025-07-23T00:00:00Z"},
                    {"updated_at", "2025-07-23T00:00:00Z"},
                    {"status", "active"},
                    {"dimension", 384},
                    {"metric", "cosine"}
                },
                {
                    {"name", "default"},
                    {"id", "default"},
                    {"description", "Default collection for embeddings"},
                    {"document_count", 0},
                    {"created_at", "2025-07-23T00:00:00Z"},
                    {"updated_at", "2025-07-23T00:00:00Z"},
                    {"status", "active"},
                    {"dimension", 384},
                    {"metric", "cosine"}
                }
            });
            response["total"] = 2;
            response["message"] = "Collections retrieved successfully";
            
            sendSuccessResponse(sock, response);
        }
        catch (const std::runtime_error& e)
        {
            ServerLogger::logError("Error getting collections: %s", e.what());
            sendErrorResponse(sock, 503, "Collection service not available: " + std::string(e.what()));
            return;
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in CollectionsListRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

std::string CollectionsListRoute::extractCollectionId(const std::string& path)
{
    std::regex id_pattern(R"(^/api/v1/collections/([^/]+)$)");
    std::smatch matches;
    if (std::regex_match(path, matches, id_pattern) && matches.size() > 1)
    {
        return matches[1].str();
    }
    return "";
}

void CollectionsListRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
                                           const std::string& error_type, const std::string& param)
{
    json error_response;
    error_response["success"] = false;
    error_response["error"] = {
        {"type", error_type},
        {"message", message},
        {"code", status}
    };
    if (!param.empty())
    {
        error_response["error"]["param"] = param;
    }

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, error_response.dump(), headers);
}

void CollectionsListRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data)
{
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, 200, data.dump(), headers);
}

} // namespace kolosal::routes
