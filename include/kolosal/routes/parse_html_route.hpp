#ifndef KOLOSAL_PARSE_HTML_ROUTE_HPP
#define KOLOSAL_PARSE_HTML_ROUTE_HPP

#include "route_interface.hpp"
#include <json.hpp>

namespace kolosal {

    class ParseHtmlRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;

    private:
        void sendJsonResponse(SocketType sock, const nlohmann::json& response, int status_code);
    };

} // namespace kolosal

#endif // KOLOSAL_PARSE_HTML_ROUTE_HPP
