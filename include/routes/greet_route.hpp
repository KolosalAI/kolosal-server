#ifndef GREET_ROUTE_HPP
#define GREET_ROUTE_HPP

#include "route_interface.hpp"

class GreetRoute : public IRoute {
public:
    // This route accepts a POST request to "/greet".
    bool match(const std::string& method,
        const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;
};

#endif  // GREET_ROUTE_HPP
