#pragma once

#include "routes/route_interface.hpp"
#include "export.hpp"

#include <string>
#include <vector>
#include <memory>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
using SocketType = SOCKET;
#else
using SocketType = int;
#endif

namespace kolosal {

    class KOLOSAL_SERVER_API Server {
    public:
        explicit Server(const std::string& port);
        ~Server();

        bool init();
        void addRoute(std::unique_ptr<IRoute> route);
        void run();
        void stop(); // New method to stop the server

    private:
        std::string port;
        SocketType listen_sock;
        std::vector<std::unique_ptr<IRoute>> routes;
        std::atomic<bool> running; // Control flag for server loop
    };

}; // namespace kolosal
