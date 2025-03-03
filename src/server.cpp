#include "kolosal/server.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

namespace kolosal {

	// Helper: parse the first line of the HTTP request
	static void parse_request_line(const std::string& requestLine,
		std::string& method, std::string& path) {
		size_t first = requestLine.find(' ');
		if (first == std::string::npos)
			return;
		method = requestLine.substr(0, first);
		size_t second = requestLine.find(' ', first + 1);
		if (second == std::string::npos)
			return;
		path = requestLine.substr(first + 1, second - first - 1);
	}

	Server::Server(const std::string& port) : port(port), running(false) {
#ifdef _WIN32
		listen_sock = INVALID_SOCKET;
#else
		listen_sock = -1;
#endif
	}

	Server::~Server() {
		stop();
#ifdef _WIN32
		if (listen_sock != INVALID_SOCKET)
			closesocket(listen_sock);
		WSACleanup();
#else
		if (listen_sock != -1)
			close(listen_sock);
#endif
	}

	bool Server::init() {
#ifdef _WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			Logger::logError("WSAStartup failed");
			return false;
		}
#endif

		struct addrinfo hints, * servinfo, * p;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;

		int rv;
		if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
#ifdef _WIN32
			Logger::logError("getaddrinfo: %s", gai_strerrorA(rv));
#else
			Logger::logError("getaddrinfo: %s", gai_strerror(rv));
#endif
			return false;
		}

		for (p = servinfo; p != nullptr; p = p->ai_next) {
#ifdef _WIN32
			listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (listen_sock == INVALID_SOCKET)
				continue;
#else
			listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (listen_sock == -1)
				continue;
#endif

			int yes = 1;
			if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
				reinterpret_cast<const char*>(&yes),
				sizeof(yes)) == -1) {
#ifdef _WIN32
				closesocket(listen_sock);
#else
				close(listen_sock);
#endif
				continue;
			}

			if (bind(listen_sock, p->ai_addr, p->ai_addrlen) == -1) {
#ifdef _WIN32
				closesocket(listen_sock);
#else
				close(listen_sock);
#endif
				continue;
			}
			break;
		}
		freeaddrinfo(servinfo);

#ifdef _WIN32
		if (listen_sock == INVALID_SOCKET) {
#else
		if (p == nullptr) {
#endif
			Logger::logError("Failed to bind socket");
			return false;
		}

		if (listen(listen_sock, 10) == -1) {
			Logger::logError("Listen failed");
			return false;
		}

		Logger::logInfo("Server initialized and listening on port %s", port.c_str());
		return true;
	}

	void Server::addRoute(std::unique_ptr<IRoute> route) {
		routes.push_back(std::move(route));
	}

	void Server::run() {
		running = true;
		Logger::logInfo("Server entering main loop");

		while (running) {
			struct sockaddr_storage client_addr;
#ifdef _WIN32
			int sin_size = sizeof(client_addr);
#else
			socklen_t sin_size = sizeof(client_addr);
#endif

			// Setup select for timeout to check running flag periodically
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(listen_sock, &readfds);

			struct timeval tv;
			tv.tv_sec = 1;  // 1 second timeout
			tv.tv_usec = 0;

			int select_result = select(listen_sock + 1, &readfds, NULL, NULL, &tv);

			if (select_result == -1) {
				Logger::logError("Select failed");
				break;
			}

			if (select_result == 0) {
				// Timeout occurred, check if we should continue running
				continue;
			}

			if (!FD_ISSET(listen_sock, &readfds)) {
				continue;
			}

			SocketType client_sock = accept(listen_sock,
				reinterpret_cast<struct sockaddr*>(&client_addr),
				&sin_size);
#ifdef _WIN32
			if (client_sock == INVALID_SOCKET) {
#else
			if (client_sock == -1) {
#endif
				Logger::logError("Accept failed");
				continue;
			}

			// Get client information for logging
			char clientIP[INET6_ADDRSTRLEN];
#ifdef _WIN32
			inet_ntop(client_addr.ss_family,
				client_addr.ss_family == AF_INET ?
				(void*)&(((struct sockaddr_in*)&client_addr)->sin_addr) :
				(void*)&(((struct sockaddr_in6*)&client_addr)->sin6_addr),
				clientIP, sizeof(clientIP));
#else
			inet_ntop(client_addr.ss_family,
				client_addr.ss_family == AF_INET ?
				(void*)&(((struct sockaddr_in*)&client_addr)->sin_addr) :
				(void*)&(((struct sockaddr_in6*)&client_addr)->sin6_addr),
				clientIP, sizeof(clientIP));
#endif
			Logger::logInfo("New client connection from %s", clientIP);

			// Read the request.
			const int bufferSize = 4096;
			char buffer[bufferSize];
			int bytes_received = recv(client_sock, buffer, bufferSize - 1, 0);
			if (bytes_received <= 0) {
#ifdef _WIN32
				closesocket(client_sock);
#else
				close(client_sock);
#endif
				continue;
			}
			buffer[bytes_received] = '\0';
			std::string request(buffer);

			// Parse the HTTP request line
			size_t pos = request.find("\r\n");
			if (pos == std::string::npos) {
				Logger::logWarning("Malformed request received");
				send_response(client_sock, 400, "{\"error\":\"Bad Request\"}");
#ifdef _WIN32
				closesocket(client_sock);
#else
				close(client_sock);
#endif
				continue;
			}
			std::string requestLine = request.substr(0, pos);
			std::string method, path;
			parse_request_line(requestLine, method, path);

			Logger::logInfo("Received %s request for %s", method.c_str(), path.c_str());

			// Extract body (if any)
			size_t headerEnd = request.find("\r\n\r\n");
			std::string body;
			if (headerEnd != std::string::npos)
				body = request.substr(headerEnd + 4);

			bool routeFound = false;
			for (auto& route : routes) {
				if (route->match(method, path)) {
					routeFound = true;
					route->handle(client_sock, body);
					break;
				}
			}

			if (!routeFound) {
				Logger::logWarning("No route found for %s %s", method.c_str(), path.c_str());
				send_response(client_sock, 404, "{\"error\":\"Not Found\"}");
			}

#ifdef _WIN32
			closesocket(client_sock);
#else
			close(client_sock);
#endif
		}

		Logger::logInfo("Server main loop exited");
	}

	void Server::stop() {
		if (running) {
			Logger::logInfo("Stopping server");
			running = false;

			// Additional cleanup could be added here
		}
	}

} // namespace kolosal