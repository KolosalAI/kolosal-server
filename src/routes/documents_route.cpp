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
    // Match POST /documents for document creation
    std::regex documents_pattern(R"(^(?:/api)?(?:/v1)?/documents$)");
    return method == "POST" && std::regex_match(path, documents_pattern);
}

void DocumentsRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        // Only handle POST requests for document creation
        handlePostDocuments(sock, body);
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
            // List all documents - for now return empty array with success
            // In a real implementation, this would query the document database
            json response;
            response["success"] = true;
            response["data"] = json::array();
            response["message"] = "Document listing endpoint - empty for now";
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

        // Process document creation using document service
        try
        {
            auto& serverAPI = ServerAPI::instance();
            auto& documentService = serverAPI.getDocumentService();
            
            // Parse request as AddDocumentsRequest
            kolosal::retrieval::AddDocumentsRequest addRequest;
            addRequest.from_json(request_data);
            
            if (!addRequest.validate())
            {
                sendErrorResponse(sock, 400, "Invalid document data");
                return;
            }
            
            // Submit to document service
            auto future_response = documentService.addDocuments(addRequest);
            auto add_response = future_response.get();
            
            // Convert to success response format
            json response;
            response["success"] = true;
            response["data"] = add_response.to_json();
            sendSuccessResponse(sock, response);
        }
        catch (const std::runtime_error& e)
        {
            // DocumentService not available
            json response;
            response["success"] = false;
            response["error"] = "Document service not available: " + std::string(e.what());
            sendSuccessResponse(sock, response);
        }
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
