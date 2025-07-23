#pragma once

#include "../export.hpp"
#include "../utils.hpp"
#include "route_interface.hpp"
#include <string>
#include <json.hpp>

namespace kolosal::routes
{

/**
 * @brief Route for listing collections
 * 
 * This route provides functionality to list all collections.
 * Available at: GET /api/v1/collections
 */
class KOLOSAL_SERVER_API CollectionsListRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    CollectionsListRoute();
    
    /**
     * @brief Destructor
     */
    ~CollectionsListRoute();
    
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
    
    std::string extractCollectionId(const std::string& path);
};

} // namespace kolosal::routes
