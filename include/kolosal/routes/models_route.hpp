#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include <string>

namespace kolosal
{
    /**
     * @brief Route handler for OpenAI-compatible models endpoint
     * Handles GET /v1/models requests to list available models
     */
    class KOLOSAL_SERVER_API ModelsRoute : public IRoute
    {
    public:
        ModelsRoute();
        ~ModelsRoute() override;

        bool match(const std::string &method, const std::string &path) override;
        void handle(SocketType sock, const std::string &body) override;
    };

} // namespace kolosal
