#pragma once

#include "route_interface.hpp"
#include "../export.hpp"
#include <string>

namespace kolosal::routes {

class KOLOSAL_SERVER_API QdrantRoute : public IRoute {
public:
    QdrantRoute();
    
    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    std::string current_method;
    std::string current_path;
    
    void handle_status(SocketType sock, const std::string& method);
    void handle_collections(SocketType sock, const std::string& method);
    
    void send_json_response(SocketType sock, int status_code, const std::string& data);
    void send_error_response(SocketType sock, int status_code, const std::string& error);
};

} // namespace kolosal::routes
