#include "server.hpp"
#include "routes/chat_completion_route.hpp"
#include "logger.hpp"
#include <memory>

int main() {
    // Initialize logger
    if (!Logger::instance().setLogFile("server.log")) {
        std::cerr << "Failed to initialize log file" << std::endl;
    }
    Logger::logInfo("Server starting up");

    Server server("8080");
    if (!server.init()) {
        Logger::logError("Failed to initialize server");
        return 1;
    }

    // Register routes
    Logger::logInfo("Registering routes");
    server.addRoute(std::make_unique<ChatCompletionsRoute>());

    Logger::logInfo("Server initialized, starting main loop");
    server.run();
    return 0;
}
