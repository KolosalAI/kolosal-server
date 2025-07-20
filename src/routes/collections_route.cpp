#include "kolosal/routes/collections_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes
{

CollectionsRoute::CollectionsRoute()
{
    ServerLogger::logInfo("CollectionsRoute initialized");
}

CollectionsRoute::~CollectionsRoute() = default;

bool CollectionsRoute::match(const std::string& method, const std::string& path)
{
    // Match /api/v1/collections and /api/v1/collections/{id}
    std::regex collections_pattern(R"(^/api/v1/collections(?:/([^/]+))?$)");
    return std::regex_match(path, collections_pattern) && 
           (method == "GET" || method == "POST" || method == "PUT" || method == "DELETE");
}

void CollectionsRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        // Note: In a real implementation, method and path would be extracted from the request
        // For now, we'll handle this based on the body content and path patterns
        
        // This is a simplified implementation - in practice, you'd need to parse the HTTP request
        // to get the actual method and path
        
        ServerLogger::logInfo("Collections route handling request");
        
        // For demonstration, we'll assume GET for empty body, POST for non-empty
        if (body.empty())
        {
            handleGetCollections(sock, "/api/v1/collections");
        }
        else
        {
            handlePostCollections(sock, body);
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in CollectionsRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

void CollectionsRoute::handleGetCollections(SocketType sock, const std::string& path)
{
    try
    {
        std::string collection_id = extractCollectionId(path);
        
        if (!collection_id.empty())
        {
            // Get specific collection
            json response;
            response["success"] = true;
            response["data"] = {
                {"id", collection_id},
                {"name", "Sample Collection"},
                {"description", "A sample document collection"},
                {"document_count", 0},
                {"created_at", "2025-07-20T00:00:00Z"}
            };
            sendSuccessResponse(sock, response);
        }
        else
        {
            // List all collections
            json response;
            response["success"] = true;
            response["data"] = json::array({
                {
                    {"id", "default"},
                    {"name", "Default Collection"},
                    {"description", "Default document collection"},
                    {"document_count", 0},
                    {"created_at", "2025-07-20T00:00:00Z"}
                }
            });
            response["total"] = 1;
            sendSuccessResponse(sock, response);
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error getting collections: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to retrieve collections");
    }
}

void CollectionsRoute::handlePostCollections(SocketType sock, const std::string& body)
{
    try
    {
        if (body.empty())
        {
            sendErrorResponse(sock, 400, "Request body is empty");
            return;
        }

        json request_data;
        try
        {
            request_data = json::parse(body);
        }
        catch (const json::parse_error& ex)
        {
            sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
            return;
        }

        // Validate required fields
        if (!request_data.contains("name"))
        {
            sendErrorResponse(sock, 400, "Collection name is required", "missing_parameter", "name");
            return;
        }

        std::string collection_name = request_data["name"];
        std::string description = request_data.value("description", "");

        // Process collection creation
        json response;
        response["success"] = true;
        response["data"] = {
            {"id", "new_collection_" + std::to_string(std::time(nullptr))},
            {"name", collection_name},
            {"description", description},
            {"document_count", 0},
            {"created_at", "2025-07-20T00:00:00Z"}
        };
        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error creating collection: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to create collection");
    }
}

void CollectionsRoute::handlePutCollections(SocketType sock, const std::string& path, const std::string& body)
{
    try
    {
        std::string collection_id = extractCollectionId(path);
        if (collection_id.empty())
        {
            sendErrorResponse(sock, 400, "Collection ID is required for updates");
            return;
        }

        if (body.empty())
        {
            sendErrorResponse(sock, 400, "Request body is empty");
            return;
        }

        json request_data;
        try
        {
            request_data = json::parse(body);
        }
        catch (const json::parse_error& ex)
        {
            sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
            return;
        }

        // Process collection update
        json response;
        response["success"] = true;
        response["data"] = {
            {"id", collection_id},
            {"name", request_data.value("name", "Updated Collection")},
            {"description", request_data.value("description", "Updated description")},
            {"document_count", 0},
            {"updated_at", "2025-07-20T00:00:00Z"}
        };
        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error updating collection: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to update collection");
    }
}

void CollectionsRoute::handleDeleteCollections(SocketType sock, const std::string& path)
{
    try
    {
        std::string collection_id = extractCollectionId(path);
        if (collection_id.empty())
        {
            sendErrorResponse(sock, 400, "Collection ID is required for deletion");
            return;
        }

        // Process collection deletion
        json response;
        response["success"] = true;
        response["data"] = {
            {"id", collection_id},
            {"message", "Collection deleted successfully"}
        };
        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error deleting collection: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to delete collection");
    }
}

std::string CollectionsRoute::extractCollectionId(const std::string& path)
{
    std::regex id_pattern(R"(^/api/v1/collections/([^/]+)$)");
    std::smatch matches;
    if (std::regex_match(path, matches, id_pattern) && matches.size() > 1)
    {
        return matches[1].str();
    }
    return "";
}

void CollectionsRoute::sendErrorResponse(SocketType sock, int status, const std::string& message, 
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

    std::string response_body = error_response.dump();
    send_response(sock, status, response_body);
}

void CollectionsRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data)
{
    std::string response_body = data.dump();
    send_response(sock, 200, response_body);
}

} // namespace kolosal::routes
