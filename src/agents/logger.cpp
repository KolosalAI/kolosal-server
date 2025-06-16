#include "kolosal/agents/logger.hpp"
#include "kolosal/logger.hpp"

namespace kolosal::agents {

class ServerLoggerBridge : public Logger {
private:
    std::string prefix;

public:
    explicit ServerLoggerBridge(const std::string& log_prefix = "") : prefix(log_prefix) {}

    void debug(const std::string& message) override {
        if (!prefix.empty()) {
            ServerLogger::logDebug("[%s] %s", prefix.c_str(), message.c_str());
        } else {
            ServerLogger::logDebug("%s", message.c_str());
        }
    }

    void info(const std::string& message) override {
        if (!prefix.empty()) {
            ServerLogger::logInfo("[%s] %s", prefix.c_str(), message.c_str());
        } else {
            ServerLogger::logInfo("%s", message.c_str());
        }
    }

    void warn(const std::string& message) override {
        if (!prefix.empty()) {
            ServerLogger::logWarning("[%s] %s", prefix.c_str(), message.c_str());
        } else {
            ServerLogger::logWarning("%s", message.c_str());
        }
    }

    void error(const std::string& message) override {
        if (!prefix.empty()) {
            ServerLogger::logError("[%s] %s", prefix.c_str(), message.c_str());
        } else {
            ServerLogger::logError("%s", message.c_str());
        }
    }
};

std::shared_ptr<Logger> LoggerFactory::create(const std::string& prefix) {
    return std::make_shared<ServerLoggerBridge>(prefix);
}

} // namespace kolosal::agents