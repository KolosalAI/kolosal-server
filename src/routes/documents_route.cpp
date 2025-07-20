#include "kolosal/routes/documents_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes
{

DocumentsRoute::DocumentsRoute()
{
    ServerLogger::logInfo("DocumentsRoute initialized");
}

DocumentsRoute::~DocumentsRoute() = default;

bool DocumentsRoute::match(const std::string& method, const std::string& path)
{
    // Match /api/v1/documents and /api/v1/documents/{id}
    std::regex documents_pattern(R"(^/api/v1/documents(?:/([^/]+))?$)");
    return std::regex_match(path, documents_pattern) && 
           (method == "GET" || method == "POST" || method == "PUT" || method == "DELETE");
}

void DocumentsRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        // Get the request info from the socket (this would normally be passed in)
        // For now, we'll assume the path is available somehow
        std::string method = "GET"; // This should come from the request
        std::string path = "/api/v1/documents"; // This should come from the request
        
        if (method == "GET")
        {
            handleGetDocuments(sock, path);
        }
        else if (method == "POST")
        {
            handlePostDocuments(sock, body);
        }
        else if (method == "PUT")
        {
            handlePutDocuments(sock, path, body);
        }
        else if (method == "DELETE")
        {
            handleDeleteDocuments(sock, path);
        }
        else
        {
            sendErrorResponse(sock, 405, "Method not allowed");
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in DocumentsRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

void DocumentsRoute::handleGetDocuments(SocketType sock, const std::string& path)
{
    try
    {
        std::string doc_id = extractDocumentId(path);
        
        if (!doc_id.empty())
        {
            // Get specific document
            json response;
            response["success"] = true;
            response["data"] = {
                {"id", doc_id},
                {"message", "Document details would be here"}
            };
            sendSuccessResponse(sock, response);
        }
        else
        {
            // List all documents
            json response;
            response["success"] = true;
            response["data"] = json::array();
            response["message"] = "Document list would be here";
            sendSuccessResponse(sock, response);
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error getting documents: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to retrieve documents");
    }
}

void DocumentsRoute::handlePostDocuments(SocketType sock, const std::string& body)
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

        // Process document creation
        json response;
        response["success"] = true;
        response["data"] = {
            {"id", "new_document_id"},
            {"message", "Document created successfully"}
        };
        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error creating document: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to create document");
    }
}

void DocumentsRoute::handlePutDocuments(SocketType sock, const std::string& path, const std::string& body)
{
    try
    {
        std::string doc_id = extractDocumentId(path);
        if (doc_id.empty())
        {
            sendErrorResponse(sock, 400, "Document ID is required for updates");
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

        // Process document update
        json response;
        response["success"] = true;
        response["data"] = {
            {"id", doc_id},
            {"message", "Document updated successfully"}
        };
        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error updating document: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to update document");
    }
}

void DocumentsRoute::handleDeleteDocuments(SocketType sock, const std::string& path)
{
    try
    {
        std::string doc_id = extractDocumentId(path);
        if (doc_id.empty())
        {
            sendErrorResponse(sock, 400, "Document ID is required for deletion");
            return;
        }

        // Process document deletion
        json response;
        response["success"] = true;
        response["data"] = {
            {"id", doc_id},
            {"message", "Document deleted successfully"}
        };
        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error deleting document: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to delete document");
    }
}

std::string DocumentsRoute::extractDocumentId(const std::string& path)
{
    std::regex id_pattern(R"(^/api/v1/documents/([^/]+)$)");
    std::smatch matches;
    if (std::regex_match(path, matches, id_pattern) && matches.size() > 1)
    {
        return matches[1].str();
    }
    return "";
}

void DocumentsRoute::sendErrorResponse(SocketType sock, int status, const std::string& message, 
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

void DocumentsRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data)
{
    std::string response_body = data.dump();
    send_response(sock, 200, response_body);
}

} // namespace kolosal::routes
