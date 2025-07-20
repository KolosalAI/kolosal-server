#pragma once

#include "export.hpp"

#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
using SocketType = SOCKET;
#else
#include <sys/socket.h>
#include <unistd.h>
using SocketType = int;
#endif

struct StreamChunk {
    std::string data;        // The content to stream
    bool isComplete = false; // Whether this is the final chunk

    StreamChunk() : data(""), isComplete(false) {}
    StreamChunk(const std::string& d, bool complete = false)
        : data(d), isComplete(complete) {
    }
};

// Get standard status text for HTTP status code
inline std::string get_status_text(int status_code) {
    switch (status_code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 422: return "Unprocessable Entity";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    default:  return "Error";
    }
}

// Regular response helper with support for custom headers
inline KOLOSAL_SERVER_API void send_response(
    SocketType sock,
    int status_code,
    const std::string& body,
    const std::map<std::string, std::string>& headers = { {"Content-Type", "application/json"} }) {

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << get_status_text(status_code) << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";

    // Add all custom headers
    for (const auto& [name, value] : headers) {
        response << name << ": " << value << "\r\n";
    }

    // End of headers
    response << "\r\n";
    // Add body
    response << body;

    send(sock, response.str().c_str(), static_cast<int>(response.str().size()), 0);
}

// Function to start a streaming response with SSE support
inline KOLOSAL_SERVER_API void begin_streaming_response(
    SocketType sock,
    int status_code,
    const std::map<std::string, std::string>& headers = {}) {

    std::ostringstream headerStream;
    headerStream << "HTTP/1.1 " << status_code << " " << get_status_text(status_code) << "\r\n";

    // Default headers for SSE streaming - DO NOT use chunked encoding for SSE
    headerStream << "Connection: keep-alive\r\n";
    headerStream << "Cache-Control: no-cache\r\n";
    headerStream << "Access-Control-Allow-Origin: *\r\n";
    headerStream << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    headerStream << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, X-API-Key\r\n";
    headerStream << "X-Content-Type-Options: nosniff\r\n";
    headerStream << "X-Frame-Options: DENY\r\n";
    headerStream << "X-XSS-Protection: 1; mode=block\r\n";

    // Add SSE headers if not present in custom headers
    bool hasContentType = false;

    // Add all custom headers
    for (const auto& [name, value] : headers) {
        headerStream << name << ": " << value << "\r\n";
        if (name == "Content-Type" || name == "content-type") {
            hasContentType = true;
        }
    }

    // Default to text/event-stream for SSE streaming if no Content-Type provided
    // This is important for OpenAI API compatibility with streaming responses
    if (!hasContentType) {
        headerStream << "Content-Type: text/event-stream; charset=utf-8\r\n";
    }

    // End of headers
    headerStream << "\r\n";

    std::string headerString = headerStream.str();
    send(sock, headerString.c_str(), static_cast<int>(headerString.size()), 0);
}

// Function to send a single stream chunk - for SSE streaming without chunked encoding
inline KOLOSAL_SERVER_API void send_stream_chunk(SocketType sock, const StreamChunk& chunk) {
    // For SSE, send data directly without HTTP chunked encoding
    if (!chunk.data.empty()) {
        send(sock, chunk.data.c_str(), static_cast<int>(chunk.data.size()), 0);
    }

    // Note: For SSE streams, we don't need to send terminating chunks like in HTTP chunked encoding
    // The connection is simply closed when the stream ends
}