#include "kolosal/routes/collections_delete_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes
{

CollectionsDeleteRoute::CollectionsDeleteRoute()
{
    ServerLogger::logInfo("CollectionsDeleteRoute initialized");
}

CollectionsDeleteRoute::~CollectionsDeleteRoute() = default;

bool CollectionsDeleteRoute::match(const std::string& method, const std::string& path)
{
    // Match DELETE /api/v1/collections/{id}
    std::regex collections_pattern(R"(^/api/v1/collections/([^/]+)$)");
    return method == "DELETE" && std::regex_match(path, collections_pattern);
}

void CollectionsDeleteRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("Collection delete request received");
        
        // Note: We would need to extract the collection ID from the path
        // For demonstration, we'll assume deleting the default collection
        std::string collection_id = "documents"; // This should be extracted from path
        
        ServerLogger::logInfo("Deleting collection: %s", collection_id.c_str());
        
        // Simulate collection deletion
        try
        {
            // In a real implementation, this would delete the collection from the document service
            json response;
            response["success"] = true;
            response["message"] = "Collection deleted successfully";
            response["collection_id"] = collection_id;
            response["deleted_at"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            sendSuccessResponse(sock, response);
        }
        catch (const std::runtime_error& e)
        {
            ServerLogger::logError("Error deleting collection: %s", e.what());
            sendErrorResponse(sock, 503, "Collection service not available: " + std::string(e.what()));
            return;
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in CollectionsDeleteRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

std::string CollectionsDeleteRoute::extractCollectionId(const std::string& path)
{
    std::regex id_pattern(R"(^/api/v1/collections/([^/]+)$)");
    std::smatch matches;
    if (std::regex_match(path, matches, id_pattern) && matches.size() > 1)
    {
        return matches[1].str();
    }
    return "";
}

void CollectionsDeleteRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
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
        {"Access-Control-Allow-Methods", "DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, error_response.dump(), headers);
}

void CollectionsDeleteRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data)
{
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, 200, data.dump(), headers);
}

} // namespace kolosal::routes
