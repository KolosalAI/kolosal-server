#pragma once

#include "../export.hpp"
#include "../utils.hpp"
#include "route_interface.hpp"
#include <string>
#include <json.hpp>

namespace kolosal::routes
{

/**
 * @brief Route for handling document file uploads
 * 
 * This route provides file upload functionality for document indexing.
 * Available at: POST /documents/upload
 */
class KOLOSAL_SERVER_API DocumentsUploadRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    DocumentsUploadRoute();
    
    /**
     * @brief Destructor
     */
    ~DocumentsUploadRoute();
    
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
    void sendErrorResponse(SocketType sock, int status, const std::string& message,
                          const std::string& error_type = "internal_error", 
                          const std::string& param = "");
    
    void sendSuccessResponse(SocketType sock, const nlohmann::json& data);
};

} // namespace kolosal::routes
