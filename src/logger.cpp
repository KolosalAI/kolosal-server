#include "kolosal/logger.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdarg>

Logger::Logger() : minLevel(LogLevel::SERVER_INFO) {
    // Default constructor
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    minLevel = level;
}

bool Logger::setLogFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(logMutex);

    // Close existing file if open
    if (logFile.is_open()) {
        logFile.close();
    }

    logFilePath = filePath;
    logFile.open(filePath, std::ios::app);

    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << filePath << std::endl;
        return false;
    }

    return true;
}

void Logger::error(const std::string& message) {
    log(LogLevel::SERVER_ERROR, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::SERVER_WARNING, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::SERVER_INFO, message);
}

void Logger::debug(const std::string& message) {
    log(LogLevel::SERVER_DEBUG, message);
}

void Logger::error(const char* format, ...) {
    if (LogLevel::SERVER_ERROR > minLevel) return;

    va_list args;
    va_start(args, format);
    std::string formattedMsg = formatString(format, args);
    va_end(args);

    log(LogLevel::SERVER_ERROR, formattedMsg);
}

void Logger::warning(const char* format, ...) {
    if (LogLevel::SERVER_WARNING > minLevel) return;

    va_list args;
    va_start(args, format);
    std::string formattedMsg = formatString(format, args);
    va_end(args);

    log(LogLevel::SERVER_WARNING, formattedMsg);
}

void Logger::info(const char* format, ...) {
    if (LogLevel::SERVER_INFO > minLevel) return;

    va_list args;
    va_start(args, format);
    std::string formattedMsg = formatString(format, args);
    va_end(args);

    log(LogLevel::SERVER_INFO, formattedMsg);
}

void Logger::debug(const char* format, ...) {
    if (LogLevel::SERVER_DEBUG > minLevel) return;

    va_list args;
    va_start(args, format);
    std::string formattedMsg = formatString(format, args);
    va_end(args);

    log(LogLevel::SERVER_DEBUG, formattedMsg);
}

void Logger::logError(const std::string& message) {
    instance().error(message);
}

void Logger::logWarning(const std::string& message) {
    instance().warning(message);
}

void Logger::logInfo(const std::string& message) {
    instance().info(message);
}

void Logger::logDebug(const std::string& message) {
    instance().debug(message);
}

void Logger::logError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMsg = instance().formatString(format, args);
    va_end(args);

    instance().error(formattedMsg);
}

void Logger::logWarning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMsg = instance().formatString(format, args);
    va_end(args);

    instance().warning(formattedMsg);
}

void Logger::logInfo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMsg = instance().formatString(format, args);
    va_end(args);

    instance().info(formattedMsg);
}

void Logger::logDebug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::string formattedMsg = instance().formatString(format, args);
    va_end(args);

    instance().debug(formattedMsg);
}

const std::vector<LogEntry>& Logger::getLogs() const {
    return logs;
}

std::string Logger::formatString(const char* format, va_list args) {
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = vsnprintf(nullptr, 0, format, argsCopy) + 1; // +1 for null terminator
    va_end(argsCopy);

    if (size <= 0) {
        return "Error formatting string";
    }

    std::vector<char> buffer(size);

    vsnprintf(buffer.data(), size, format, args);

    return std::string(buffer.data(), buffer.data() + size - 1); // -1 to exclude null terminator
}

void Logger::log(LogLevel level, const std::string& message) {
    // Skip if level is below minimum
    if (level > minLevel) {
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
    LogEntry entry{ level, timestamp, message };
    logs.push_back(entry);

    // Output to console
    std::cout << formattedMessage << std::endl;

    // Write to file if open
    if (logFile.is_open()) {
        logFile << formattedMessage << std::endl;
        logFile.flush();
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::SERVER_ERROR:   return "ERROR";
    case LogLevel::SERVER_WARNING: return "WARNING";
    case LogLevel::SERVER_INFO:    return "INFO";
    case LogLevel::SERVER_DEBUG:   return "DEBUG";
    default:                return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}