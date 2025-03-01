#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <memory>
#include "routes/route_interface.hpp"

#ifdef _WIN32
#include <winsock2.h>
using SocketType = SOCKET;
#else
using SocketType = int;
#endif

class Server {
public:
	explicit Server(const std::string& port);
	~Server();

	bool init();
	void addRoute(std::unique_ptr<IRoute> route);
	void run();

private:
	std::string port;
	SocketType listen_sock;
	std::vector<std::unique_ptr<IRoute>> routes;
};

#endif  // SERVER_HPP
