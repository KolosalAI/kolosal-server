#ifndef KOLOSAL_PARSE_PDF_ROUTE_HPP
#define KOLOSAL_PARSE_PDF_ROUTE_HPP

#include "route_interface.hpp"

namespace kolosal {

    class ParsePDFRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    };

} // namespace kolosal

#endif // KOLOSAL_PARSE_PDF_ROUTE_HPP
