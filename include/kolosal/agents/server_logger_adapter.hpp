// File: include/kolosal/agents/server_logger_adapter.hpp
#pragma once

#include "kolosal/logger.hpp"
#include <string>
#include <memory>

namespace kolosal::agents {

/**
 * @brief Logger interface for agent system (moved from removed logger.hpp)
 */
class Logger {
public:
    virtual ~Logger() = default;
    virtual void debug(const std::string& message) = 0;
    virtual void info(const std::string& message) = 0;
    virtual void warn(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;
};

/**
 * Adapter class that wraps ServerLogger to implement the agents::Logger interface
 * This bridges the gap between the singleton ServerLogger and the agents system
 */
class KOLOSAL_SERVER_API ServerLoggerAdapter : public Logger {
public:
    ServerLoggerAdapter() : serverLogger(ServerLogger::instance()) {}
    virtual ~ServerLoggerAdapter() = default;
    
    // Implement the agents::Logger interface
    void debug(const std::string& message) override {
        serverLogger.debug(message);
    }
    void info(const std::string& message) override {
        serverLogger.info(message);
    }
    void warn(const std::string& message) override {
        serverLogger.warning(message);  // Map warn -> warning
    }
    void error(const std::string& message) override {
        serverLogger.error(message);
    }
private:
    ServerLogger& serverLogger;  // Reference to singleton, not owned
};

} // namespace kolosal::agents