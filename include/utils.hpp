#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <sstream>
#include <iomanip>

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

// Regular response helper
inline void send_response(SocketType sock, int status_code,
    const std::string& body,
    const std::string& contentType = "application/json") {
    std::string status_text;
    switch (status_code) {
    case 200: status_text = "OK"; break;
    case 400: status_text = "Bad Request"; break;
    case 404: status_text = "Not Found"; break;
    case 405: status_text = "Method Not Allowed"; break;
    default:  status_text = "Error"; break;
    }

    std::string response =
        "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n" +
        "Content-Type: " + contentType + "\r\n" +
        "Connection: close\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
        body;
    send(sock, response.c_str(), response.size(), 0);
}

// Function to start a streaming response (sends headers)
inline void begin_streaming_response(
    SocketType sock,
    int status_code,
    const std::string& contentType = "application/json") {

    std::string status_text;
    switch (status_code) {
    case 200: status_text = "OK"; break;
    case 400: status_text = "Bad Request"; break;
    case 404: status_text = "Not Found"; break;
    case 405: status_text = "Method Not Allowed"; break;
    default:  status_text = "Error"; break;
    }

    // Send the headers with Transfer-Encoding: chunked
    std::string headers =
        "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n" +
        "Content-Type: " + contentType + "\r\n" +
        "Connection: close\r\n" +
        "Transfer-Encoding: chunked\r\n\r\n";

    send(sock, headers.c_str(), headers.size(), 0);
}

// Function to send a single stream chunk
inline void send_stream_chunk(SocketType sock, const StreamChunk& chunk) {
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

#endif  // UTILS_HPP