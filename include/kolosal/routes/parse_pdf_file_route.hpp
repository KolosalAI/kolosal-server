#pragma once

#include "kolosal/routes/route_interface.hpp"
#include "kolosal/utils.hpp"

namespace kolosal
{
    class ParsePDFFileRoute : public IRoute
    {
    public:
        bool match(const std::string &method, const std::string &path) override;
        void handle(SocketType sock, const std::string &body) override;
    };
}
