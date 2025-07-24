#pragma once

#include "../export.hpp"
#include "../utils.hpp"
#include "route_interface.hpp"
#include <string>
#include <json.hpp>

namespace kolosal::routes
{

/**
 * @brief Route for deleting collections
 * 
 * This route provides functionality to delete collections.
 * Available at: DELETE /api/v1/collections/{collection_name}
 */
class KOLOSAL_SERVER_API CollectionsDeleteRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    CollectionsDeleteRoute();
    
    /**
     * @brief Destructor
     */
    ~CollectionsDeleteRoute();
    
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
