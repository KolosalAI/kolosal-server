#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#ifdef _WIN32
#include <winsock2.h>
using SocketType = SOCKET;
#else
using SocketType = int;
#endif

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

#endif  // UTILS_HPP
