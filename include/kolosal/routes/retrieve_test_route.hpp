#pragma once

#include "../export.hpp"
#include "../utils.hpp"
#include "route_interface.hpp"
#include <string>

namespace kolosal
{

/**
 * @brief Route for testing document retrieval functionality
 * 
 * This route provides diagnostic endpoints to help debug retrieval issues.
 * Available at: GET /retrieve/test
 */
class KOLOSAL_SERVER_API RetrieveTestRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    RetrieveTestRoute();
    
    /**
     * @brief Destructor
     */
    ~RetrieveTestRoute();
    
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
};

} // namespace kolosal
