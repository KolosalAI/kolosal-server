#include "server.hpp"
#include "routes/greet_route.hpp"
#include <memory>

int main() {
    Server server("8080");
    if (!server.init()) {
        return 1;
    }

    // Register routes. New routes can be added here easily.
    server.addRoute(std::make_unique<GreetRoute>());

    server.run();
    return 0;
}
