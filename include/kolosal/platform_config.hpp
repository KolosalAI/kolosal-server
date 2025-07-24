#pragma once

/**
 * @file platform_config.hpp
 * @brief Platform-specific configuration and includes
 * 
 * This header provides centralized platform-specific configuration
 * to prevent Windows socket conflicts and other platform issues.
 * 
 * @author Kolosal AI Team
 * @version 1.0
 * @date 2025
 */

#ifdef _WIN32
    // Prevent inclusion of winsock.h (old version) to avoid conflicts
    #ifndef _WINSOCKAPI_
    #define _WINSOCKAPI_
    #endif
    
    // Define lean and mean to reduce Windows header bloat
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    
    // Prevent min/max macros from Windows headers
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    
    // Include Windows socket headers in the correct order
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    using SocketType = SOCKET;
#else
    #include <sys/socket.h>
    #include <unistd.h>
    using SocketType = int;
#endif
