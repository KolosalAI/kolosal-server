#pragma once

#include "../export.hpp"
#include "../utils.hpp"
#include "route_interface.hpp"
#include <string>
#include <json.hpp>

namespace kolosal::routes
{

/**
 * @brief Route for handling bulk document operations
 * 
 * This route provides bulk document upload, retrieval, and management functionality.
 * Available at: POST /api/v1/documents/bulk, POST /retrieve-bulk
 */
class KOLOSAL_SERVER_API BulkOperationsRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    BulkOperationsRoute();
    
    /**
     * @brief Destructor
     */
    ~BulkOperationsRoute();
    
    /**
     * @brief Check if this route matches the request
     * @param method HTTP method
     * @param path URL path
     * @return true if this route should handle the request
     */
    bool match(const std::string& method, const std::string& path) override;
    
    /**
     * @brief Handle the request
     * @param sock Socket for response
     * @param body Request body
     */
    void handle(SocketType sock, const std::string& body) override;

private:
    void handleBulkDocuments(SocketType sock, const std::string& body);
    void handleBulkRetrieval(SocketType sock, const std::string& body);
    
    void sendErrorResponse(SocketType sock, int status, const std::string& message,
                          const std::string& error_type = "internal_error", 
                          const std::string& param = "");
    
    void sendSuccessResponse(SocketType sock, const nlohmann::json& data);
};

} // namespace kolosal::routes
