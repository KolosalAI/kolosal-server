#include "kolosal/server.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <mutex>
#include <json.hpp>

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

namespace kolosal
{

	// Helper: extract client IP from socket address
	static std::string extractClientIP(const struct sockaddr_storage &client_addr)
	{
		char clientIP[INET6_ADDRSTRLEN];
#ifdef _WIN32
		inet_ntop(client_addr.ss_family,
				  client_addr.ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)&client_addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)&client_addr)->sin6_addr),
				  clientIP, sizeof(clientIP));
#else
		inet_ntop(client_addr.ss_family,
				  client_addr.ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)&client_addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)&client_addr)->sin6_addr),
				  clientIP, sizeof(clientIP));
#endif
		return std::string(clientIP);
	}

	// Helper: parse HTTP headers from request
	static std::map<std::string, std::string> parseHeaders(const std::string &request)
	{
		std::map<std::string, std::string> headers;

		size_t start = request.find("\r\n");
		if (start == std::string::npos)
			return headers;

		start += 2; // Skip the first \r\n
		size_t end = request.find("\r\n\r\n");
		if (end == std::string::npos)
			return headers;

		std::string headerSection = request.substr(start, end - start);
		std::istringstream headerStream(headerSection);
		std::string line;

		while (std::getline(headerStream, line) && !line.empty())
		{
			if (line.back() == '\r')
				line.pop_back(); // Remove \r

			size_t colonPos = line.find(':');
			if (colonPos != std::string::npos)
			{
				std::string name = line.substr(0, colonPos);
				std::string value = line.substr(colonPos + 1);

				// Trim whitespace
				name.erase(0, name.find_first_not_of(" \t"));
				name.erase(name.find_last_not_of(" \t") + 1);
				value.erase(0, value.find_first_not_of(" \t"));
				value.erase(value.find_last_not_of(" \t") + 1);

				headers[name] = value;
			}
		}

		return headers;
	}

	// Helper: parse the first line of the HTTP request
	static void parse_request_line(const std::string &requestLine,
								   std::string &method, std::string &path)
	{
		size_t first = requestLine.find(' ');
		if (first == std::string::npos)
			return;
		method = requestLine.substr(0, first);
		size_t second = requestLine.find(' ', first + 1);
		if (second == std::string::npos)
			return;
		path = requestLine.substr(first + 1, second - first - 1);
	}

	// Thread function to handle a client request
	void handle_client(SocketType client_sock,
					   const std::vector<std::unique_ptr<IRoute>> &routes,
					   const std::string &request,
					   const char *clientIP)
	{
		// Parse the HTTP request line
		size_t pos = request.find("\r\n");
		if (pos == std::string::npos)
		{
			ServerLogger::logWarning("Malformed request received from %s", clientIP);
			send_response(client_sock, 400, "{\"error\":\"Bad Request\"}");
#ifdef _WIN32
			closesocket(client_sock);
#else
			close(client_sock);
#endif
			return;
		}
		std::string requestLine = request.substr(0, pos);
		std::string method, path;
		parse_request_line(requestLine, method, path);

		ServerLogger::logInfo("[Thread %u] Processing %s request for %s from %s",
							  std::this_thread::get_id(), method.c_str(), path.c_str(), clientIP);

		// Extract body (if any)
		size_t headerEnd = request.find("\r\n\r\n");
		std::string body;
		if (headerEnd != std::string::npos)
			body = request.substr(headerEnd + 4);

		bool routeFound = false;
		for (const auto &route : routes)
		{
			if (route->match(method, path))
			{
				routeFound = true;
				route->handle(client_sock, body);
				break;
			}
		}

		if (!routeFound)
		{
			ServerLogger::logWarning("No route found for %s %s", method.c_str(), path.c_str());
			send_response(client_sock, 404, "{\"error\":\"Not Found\"}");
		}

#ifdef _WIN32
		closesocket(client_sock);
#else
		close(client_sock);
#endif
		ServerLogger::logInfo("[Thread %u] Completed request for %s",
							  std::this_thread::get_id(), path.c_str());
	}
	Server::Server(const std::string &port, const std::string &host) : port(port), host(host), running(false)
	{
#ifdef _WIN32
		listen_sock = INVALID_SOCKET;
#else
		listen_sock = -1;
#endif
		// Initialize authentication middleware with default settings
		authMiddleware_ = std::make_unique<auth::AuthMiddleware>();
	}

	Server::~Server()
	{
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

	bool Server::init()
	{
#ifdef _WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			ServerLogger::logError("WSAStartup failed");
			return false;
		}
#endif

		struct addrinfo hints, *servinfo, *p;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		int rv;
		// Use the specified host instead of NULL to bind to specific interface
		const char *bind_host = (host == "0.0.0.0") ? NULL : host.c_str();
		if ((rv = getaddrinfo(bind_host, port.c_str(), &hints, &servinfo)) != 0)
		{
#ifdef _WIN32
			ServerLogger::logError("getaddrinfo: %s", gai_strerrorA(rv));
#else
			ServerLogger::logError("getaddrinfo: %s", gai_strerror(rv));
#endif
			return false;
		}

		for (p = servinfo; p != nullptr; p = p->ai_next)
		{
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
						   reinterpret_cast<const char *>(&yes),
						   sizeof(yes)) == -1)
			{
#ifdef _WIN32
				closesocket(listen_sock);
#else
				close(listen_sock);
#endif
				continue;
			}

			if (bind(listen_sock, p->ai_addr, p->ai_addrlen) == -1)
			{
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
		if (listen_sock == INVALID_SOCKET)
		{
#else
		if (p == nullptr)
		{
#endif
			ServerLogger::logError("Failed to bind socket");
			return false;
		}

		if (listen(listen_sock, 10) == -1)
		{
			ServerLogger::logError("Listen failed");
			return false;
		}

		ServerLogger::logInfo("Server initialized and listening on %s:%s",
							  host.c_str(), port.c_str());
		return true;
	}

	void Server::addRoute(std::unique_ptr<IRoute> route)
	{
		routes.push_back(std::move(route));
	}

	void Server::run()
	{
		running = true;
		ServerLogger::logInfo("Server entering main loop with concurrent request handling");

		while (running)
		{
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
			tv.tv_sec = 1; // 1 second timeout
			tv.tv_usec = 0;

			int select_result = select(listen_sock + 1, &readfds, NULL, NULL, &tv);

			if (select_result == -1)
			{
				ServerLogger::logError("Select failed");
				break;
			}

			if (select_result == 0)
			{
				// Timeout occurred, check if we should continue running
				continue;
			}

			if (!FD_ISSET(listen_sock, &readfds))
			{
				continue;
			}

			SocketType client_sock = accept(listen_sock,
											reinterpret_cast<struct sockaddr *>(&client_addr),
											&sin_size);
#ifdef _WIN32
			if (client_sock == INVALID_SOCKET)
			{
#else
			if (client_sock == -1)
			{
#endif
				ServerLogger::logError("Accept failed");
				continue;
			}

			// Get client information for logging
			char clientIP[INET6_ADDRSTRLEN];
#ifdef _WIN32
			inet_ntop(client_addr.ss_family,
					  client_addr.ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)&client_addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)&client_addr)->sin6_addr),
					  clientIP, sizeof(clientIP));
#else
			inet_ntop(client_addr.ss_family,
					  client_addr.ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)&client_addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)&client_addr)->sin6_addr),
					  clientIP, sizeof(clientIP));
#endif			ServerLogger::logDebug("New client connection from %s", clientIP); // Spawn a thread to handle this client
			std::thread([this, client_sock, clientIP]()
						{
							ServerLogger::logDebug("[Thread %d] Processing request from %s",
												  std::this_thread::get_id(), clientIP);

							// Read the HTTP headers
							const int headerBufferSize = 8192;
							char headerBuffer[headerBufferSize];
							int headerBytesReceived = recv(client_sock, headerBuffer, headerBufferSize - 1, 0);

							if (headerBytesReceived <= 0)
							{
								ServerLogger::logError("[Thread %d] Failed to read HTTP headers", std::this_thread::get_id());
#ifdef _WIN32
								closesocket(client_sock);
#else
								close(client_sock);
#endif
								return;
							}

							headerBuffer[headerBytesReceived] = '\0';
							std::string request(headerBuffer, headerBytesReceived);

							// Parse the HTTP request line
							size_t endOfLine = request.find("\r\n");
							if (endOfLine == std::string::npos)
							{
								ServerLogger::logWarning("[Thread %d] Malformed request received", std::this_thread::get_id());
								send_response(client_sock, 400, "{\"error\":\"Bad Request\"}");
#ifdef _WIN32
								closesocket(client_sock);
#else
								close(client_sock);
#endif
								return;
							}

							std::string requestLine = request.substr(0, endOfLine);
							std::string method, path;
							parse_request_line(requestLine, method, path);

							// Parse headers for authentication middleware
							auto headers = parseHeaders(request);							ServerLogger::logDebug("[Thread %d] Processing %s request for %s from %s",
												  std::this_thread::get_id(), method.c_str(), path.c_str(), clientIP); // Process authentication middleware
							ServerLogger::logDebug("[Thread %d] Calling auth middleware for %s %s from %s",
												  std::this_thread::get_id(), method.c_str(), path.c_str(), clientIP);

							auth::AuthMiddleware::RequestInfo authRequest(method, path, clientIP);
							authRequest.headers = headers;

							auto authResult = authMiddleware_->processRequest(authRequest);

							ServerLogger::logDebug("[Thread %d] Auth middleware result - Allowed: %s, Status: %d, Reason: %s",
												  std::this_thread::get_id(),
												  authResult.allowed ? "true" : "false",
												  authResult.statusCode,
												  authResult.reason.c_str());

							// Add authentication response headers
							std::map<std::string, std::string> responseHeaders = {{"Content-Type", "application/json"}};
							responseHeaders.insert(authResult.headers.begin(), authResult.headers.end());

							// Check if request is blocked by authentication
							if (!authResult.allowed)
							{
								nlohmann::json jError = {
									{"error", {{"message", authResult.reason}, {"type", authResult.statusCode == 429 ? "rate_limit_exceeded" : "authentication_error"}, {"code", authResult.statusCode}}}};

								send_response(client_sock, authResult.statusCode, jError.dump(), responseHeaders);

								ServerLogger::logWarning("[Thread %d] Request blocked: %s",
														 std::this_thread::get_id(), authResult.reason.c_str());

#ifdef _WIN32
								closesocket(client_sock);
#else
								close(client_sock);
#endif
								return;
							}

							// Handle CORS preflight requests
							if (authResult.isPreflight)
							{
								send_response(client_sock, authResult.statusCode, "", responseHeaders);								ServerLogger::logDebug("[Thread %d] CORS preflight request handled",
													  std::this_thread::get_id());

#ifdef _WIN32
								closesocket(client_sock);
#else
								close(client_sock);
#endif
								return;
							}

							// Find Content-Length header
							int contentLength = 0;
							std::string contentLengthHeader = "Content-Length: ";
							size_t contentLengthPos = request.find(contentLengthHeader);

							if (contentLengthPos != std::string::npos)
							{
								size_t valueStart = contentLengthPos + contentLengthHeader.length();
								size_t valueEnd = request.find("\r\n", valueStart);
								if (valueEnd != std::string::npos)
								{
									std::string lengthStr = request.substr(valueStart, valueEnd - valueStart);
									try
									{
										contentLength = std::stoi(lengthStr);
										ServerLogger::logDebug("[Thread %d] Content-Length: %d",
															   std::this_thread::get_id(), contentLength);
									}
									catch (const std::exception &e)
									{
										ServerLogger::logWarning("[Thread %d] Invalid Content-Length header: %s",
																 std::this_thread::get_id(), lengthStr.c_str());
									}
								}
							}

							// Find the start of the body
							size_t bodyStart = request.find("\r\n\r\n");
							std::string body;

							if (bodyStart != std::string::npos)
							{
								// Extract the body we've already read
								body = request.substr(bodyStart + 4);

								// If Content-Length indicates there's more data to read
								if (contentLength > 0 && body.length() < static_cast<size_t>(contentLength))
								{
									int remaining = contentLength - body.length();
									std::vector<char> bodyBuffer(remaining + 1, 0);

									int totalRead = 0;
									while (totalRead < remaining)
									{
										int bytesRead = recv(client_sock, bodyBuffer.data() + totalRead,
															 remaining - totalRead, 0);
										if (bytesRead <= 0)
										{
											break; // Error or connection closed
										}
										totalRead += bytesRead;
									}

									if (totalRead > 0)
									{
										body.append(bodyBuffer.data(), totalRead);
									}

									ServerLogger::logDebug("[Thread %d] Read %d additional bytes for body",
														   std::this_thread::get_id(), totalRead);
								}
							} // Route the request
							bool routeFound = false;
							for (auto &route : routes)
							{
								if (route->match(method, path))
								{
									routeFound = true;
									try
									{
										// Note: Routes will need to be updated to handle authentication headers
										// For now, they'll work as before but won't include auth headers
										route->handle(client_sock, body);
									}
									catch (const std::exception &ex)
									{
										ServerLogger::logError("[Thread %d] Error in route handler: %s",
															   std::this_thread::get_id(), ex.what());

										// If we haven't sent a response yet, send an error
										nlohmann::json jError = {{"error", {{"message", std::string("Internal error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};
										send_response(client_sock, 500, jError.dump(), responseHeaders);
									}
									break;
								}
							}

							if (!routeFound)
							{
								ServerLogger::logWarning("[Thread %d] No route found for %s %s",
														 std::this_thread::get_id(), method.c_str(), path.c_str());

								nlohmann::json jError = {{"error", {{"message", "Not found"}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
								send_response(client_sock, 404, jError.dump(), responseHeaders);
							}							ServerLogger::logDebug("[Thread %d] Completed request for %s",
												  std::this_thread::get_id(), path.c_str());

#ifdef _WIN32
							closesocket(client_sock);
#else
							close(client_sock);
#endif
						})
				.detach(); // Detach the thread to handle the request independently
		}

		ServerLogger::logInfo("Server main loop exited");
	}

	void Server::stop()
	{
		if (running)
		{
			ServerLogger::logInfo("Stopping server");
			running = false;

			// Additional cleanup could be added here
		}
	}

} // namespace kolosal