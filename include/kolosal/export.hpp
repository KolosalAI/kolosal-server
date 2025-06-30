#pragma once

#ifdef KOLOSAL_SERVER_STATIC
    // For static library linking, no import/export needed
    #define KOLOSAL_SERVER_API
#elif defined(_WIN32)
    #ifdef KOLOSAL_SERVER_BUILD
        #define KOLOSAL_SERVER_API __declspec(dllexport)
    #else
        #define KOLOSAL_SERVER_API __declspec(dllimport)
    #endif
#else
    // For Linux/Unix systems, use standard visibility attributes
    #ifdef KOLOSAL_SERVER_BUILD
        #define KOLOSAL_SERVER_API __attribute__((visibility("default")))
    #else
        #define KOLOSAL_SERVER_API
    #endif
#endif