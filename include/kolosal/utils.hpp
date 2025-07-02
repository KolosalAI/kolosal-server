#pragma once

#include "export.hpp"

#include <string>
#include <sstream>
#include <iomanip>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
using SocketType = SOCKET;
#else
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

    send(sock, response.str().c_str(), response.str().size(), 0);
}

// Function to start a streaming response with SSE support
inline KOLOSAL_SERVER_API void begin_streaming_response(
    SocketType sock,
    int status_code,
    const std::map<std::string, std::string>& headers = {}) {

    std::ostringstream headerStream;
    headerStream << "HTTP/1.1 " << status_code << " " << get_status_text(status_code) << "\r\n";

    // Default headers for streaming
    headerStream << "Transfer-Encoding: chunked\r\n";
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

    // Default to text/plain for streaming if no Content-Type provided
    // This is important for OpenAI API compatibility with streaming responses
    if (!hasContentType) {
        headerStream << "Content-Type: text/plain; charset=utf-8\r\n";
    }

    // End of headers
    headerStream << "\r\n";

    std::string headerString = headerStream.str();
    send(sock, headerString.c_str(), headerString.size(), 0);
}

// Function to send a single stream chunk - modified to handle SSE format better
inline KOLOSAL_SERVER_API void send_stream_chunk(SocketType sock, const StreamChunk& chunk) {
    // Only send non-empty chunks
    if (!chunk.data.empty()) {
        // Format the chunk according to HTTP chunked encoding
        std::stringstream ss;
        ss << std::hex << chunk.data.size();
        std::string hex_length = ss.str();

        std::string chunk_header = hex_length + "\r\n";
        std::string chunk_data = chunk.data + "\r\n";

        send(sock, chunk_header.c_str(), chunk_header.size(), 0);
        send(sock, chunk_data.c_str(), chunk_data.size(), 0);
    }

    // If this is the final chunk, send the terminating empty chunk
    if (chunk.isComplete) {
        const char* end_chunk = "0\r\n\r\n";
        send(sock, end_chunk, strlen(end_chunk), 0);
    }
}