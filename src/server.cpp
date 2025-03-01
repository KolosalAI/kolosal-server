#include "server.hpp"
#include "utils.hpp"
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

// Helper: parse the first line of the HTTP request (e.g., "POST /greet HTTP/1.1")
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

Server::Server(const std::string& port) : port(port) {
#ifdef _WIN32
	listen_sock = INVALID_SOCKET;
#else
	listen_sock = -1;
#endif
}

Server::~Server() {
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
		std::cerr << "WSAStartup failed\n";
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
		std::cerr << "getaddrinfo: " << gai_strerrorA(rv) << "\n";
#else
		std::cerr << "getaddrinfo: " << gai_strerror(rv) << "\n";
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
		std::cerr << "Failed to bind socket\n";
		return false;
	}

	if (listen(listen_sock, 10) == -1) {
		std::cerr << "Listen failed\n";
		return false;
	}

	std::cout << "Server listening on port " << port << "\n";
	return true;
}

void Server::addRoute(std::unique_ptr<IRoute> route) {
	routes.push_back(std::move(route));
}

void Server::run() {
	while (true) {
		struct sockaddr_storage client_addr;
#ifdef _WIN32
		int sin_size = sizeof(client_addr);
#else
		socklen_t sin_size = sizeof(client_addr);
#endif

		SocketType client_sock =
			accept(listen_sock, reinterpret_cast<struct sockaddr*>(&client_addr),
				&sin_size);
#ifdef _WIN32
		if (client_sock == INVALID_SOCKET) {
#else
		if (client_sock == -1) {
#endif
			std::cerr << "Accept failed\n";
			continue;
		}

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
			send_response(client_sock, 404, "{\"error\":\"Not Found\"}");
		}

#ifdef _WIN32
		closesocket(client_sock);
#else
		close(client_sock);
#endif
	}
}