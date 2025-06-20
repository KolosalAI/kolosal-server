#pragma once

#include "routes/route_interface.hpp"
#include "auth/auth_middleware.hpp"
#include "export.hpp"

#include <string>
#include <vector>
#include <memory>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
using SocketType = SOCKET;
#else
#include <sys/socket.h>
#include <unistd.h>
using SocketType = int;
#endif

namespace kolosal {

    class KOLOSAL_SERVER_API Server {    public:
        explicit Server(const std::string& port, const std::string& host = "0.0.0.0");
        ~Server();

        bool init();
        void addRoute(std::unique_ptr<IRoute> route);
        void run();
        void stop(); // New method to stop the server

        // Authentication middleware access
        auth::AuthMiddleware& getAuthMiddleware() { return *authMiddleware_; }
        const auth::AuthMiddleware& getAuthMiddleware() const { return *authMiddleware_; }

    private:
        std::string port;
        std::string host;
        SocketType listen_sock;
        std::vector<std::unique_ptr<IRoute>> routes;
        std::atomic<bool> running; // Control flag for server loop
        std::unique_ptr<auth::AuthMiddleware> authMiddleware_; // Authentication middleware
    };

}; // namespace kolosal
