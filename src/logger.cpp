#include "kolosal/logger.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdarg>

ServerLogger::ServerLogger() : minLevel(LogLevel::SERVER_INFO)
{
    // Default constructor
}

ServerLogger::~ServerLogger()
{
    if (logFile.is_open())
    {
        logFile.close();
    }
}

ServerLogger &ServerLogger::instance()
{
    static ServerLogger instance;
    return instance;
}

void ServerLogger::setLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(logMutex);
    minLevel = level;
}

bool ServerLogger::setLogFile(const std::string &filePath)
{
    std::lock_guard<std::mutex> lock(logMutex);

    // Close existing file if open
    if (logFile.is_open())
    {
        logFile.close();
    }

    logFilePath = filePath;
    logFile.open(filePath, std::ios::app);

    if (!logFile.is_open())
    {
        std::cerr << "Failed to open log file: " << filePath << std::endl;
        return false;
    }

    return true;
}

void ServerLogger::error(const std::string &message)
{
    log(LogLevel::SERVER_ERROR, message);
}

void ServerLogger::warning(const std::string &message)
{
    log(LogLevel::SERVER_WARNING, message);
}

void ServerLogger::info(const std::string &message)
{
    log(LogLevel::SERVER_INFO, message);
}

void ServerLogger::debug(const std::string &message)
{
    log(LogLevel::SERVER_DEBUG, message);
}

void ServerLogger::error(const char *format, ...)
{
    if (LogLevel::SERVER_ERROR > minLevel)
        return;

    va_list args;
    va_start(args, format);
    std::string formattedMsg = formatString(format, args);
    va_end(args);

    log(LogLevel::SERVER_ERROR, formattedMsg);
}

void ServerLogger::warning(const char *format, ...)
{
    if (LogLevel::SERVER_WARNING > minLevel)
        return;

    va_list args;
    va_start(args, format);
    std::string formattedMsg = formatString(format, args);
    va_end(args);

    log(LogLevel::SERVER_WARNING, formattedMsg);
}

void ServerLogger::info(const char *format, ...)
{
    if (LogLevel::SERVER_INFO > minLevel)
        return;

    va_list args;
    va_start(args, format);
    std::string formattedMsg = formatString(format, args);
    va_end(args);

    log(LogLevel::SERVER_INFO, formattedMsg);
}

void ServerLogger::debug(const char *format, ...)
{
    if (LogLevel::SERVER_DEBUG > minLevel)
        return;

    va_list args;
    va_start(args, format);
    std::string formattedMsg = formatString(format, args);
    va_end(args);

    log(LogLevel::SERVER_DEBUG, formattedMsg);
}

void ServerLogger::logError(const std::string &message)
{
    instance().error(message);
}

void ServerLogger::logWarning(const std::string &message)
{
    instance().warning(message);
}

void ServerLogger::logInfo(const std::string &message)
{
    instance().info(message);
}

void ServerLogger::logDebug(const std::string &message)
{
    instance().debug(message);
}

void ServerLogger::logError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::string formattedMsg = instance().formatString(format, args);
    va_end(args);

    instance().error(formattedMsg);
}

void ServerLogger::logWarning(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::string formattedMsg = instance().formatString(format, args);
    va_end(args);

    instance().warning(formattedMsg);
}

void ServerLogger::logInfo(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::string formattedMsg = instance().formatString(format, args);
    va_end(args);

    instance().info(formattedMsg);
}

void ServerLogger::logDebug(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::string formattedMsg = instance().formatString(format, args);
    va_end(args);

    instance().debug(formattedMsg);
}

const std::vector<LogEntry> &ServerLogger::getLogs() const
{
    return logs;
}

std::string ServerLogger::formatString(const char *format, va_list args)
{
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = vsnprintf(nullptr, 0, format, argsCopy) + 1; // +1 for null terminator
    va_end(argsCopy);

    if (size <= 0)
    {
        return "Error formatting string";
    }

    std::vector<char> buffer(size);

    vsnprintf(buffer.data(), size, format, args);

    return std::string(buffer.data(), buffer.data() + size - 1); // -1 to exclude null terminator
}

void ServerLogger::log(LogLevel level, const std::string &message)
{
    // Skip if level is below minimum
    if (level > minLevel)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex);

    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = levelToString(level);

    // Format the log message
    std::ostringstream logStream;
    logStream << "[" << timestamp << "] [" << levelStr << "] " << message;
    std::string formattedMessage = logStream.str();

    // Store in memory
    LogEntry entry{level, timestamp, message};
    logs.push_back(entry);

    // Output to console
    std::cout << formattedMessage << std::endl;

    // Write to file if open
    if (logFile.is_open())
    {
        logFile << formattedMessage << std::endl;
        logFile.flush();
    }
}

std::string ServerLogger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::SERVER_ERROR:
        return "ERROR";
    case LogLevel::SERVER_WARNING:
        return "WARNING";
    case LogLevel::SERVER_INFO:
        return "INFO";
    case LogLevel::SERVER_DEBUG:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}

std::string ServerLogger::getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}