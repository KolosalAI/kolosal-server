#ifndef KOLOSAL_DOCUMENT_SEARCH_ROUTE_HPP
#define KOLOSAL_DOCUMENT_SEARCH_ROUTE_HPP

#include "route_interface.hpp"

namespace kolosal::routes
{

class DocumentSearchRoute : public IRoute
{
public:
    DocumentSearchRoute();
    ~DocumentSearchRoute();

    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    void sendSuccessResponse(SocketType sock, const std::string& content);
    void sendErrorResponse(SocketType sock, int status_code, const std::string& message);
};

} // namespace kolosal::routes

#endif // KOLOSAL_DOCUMENT_SEARCH_ROUTE_HPP
