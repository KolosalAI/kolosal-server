#pragma once

#include "../export.hpp"

#include <string>

#ifdef _WIN32
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
using SocketType = SOCKET;
#else
#include <sys/socket.h>
#include <unistd.h>
using SocketType = int;
#endif

class KOLOSAL_SERVER_API IRoute {
public:
    // Returns true if this route should handle the given method and path.
    virtual bool match(const std::string& method,
        const std::string& path) = 0;
    // Handle the request. The body contains the payload (if any).
    virtual void handle(SocketType sock, const std::string& body) = 0;
    virtual ~IRoute() {}
};