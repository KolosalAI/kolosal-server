#pragma once

#ifdef _WIN32
// Suppress C4251 warnings for STL containers in exported classes
// These warnings are benign when all modules use the same runtime
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

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

#ifdef _WIN32
#pragma warning(pop)
#endif