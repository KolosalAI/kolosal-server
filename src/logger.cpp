#include "kolosal/logger.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cstdarg>
// PIMPL Implementation - All STL containers are hidden here
class ServerLogger::Impl {
public:
	LogLevel minLevel = LogLevel::SERVER_INFO;
	std::vector<LogEntry> logs;
	std::ofstream logFile;
	std::string logFilePath;
	std::mutex logMutex;

<<<<<<< HEAD
	// Get string representation of log level
	std::string levelToString(LogLevel level) {
		switch (level) {
			case LogLevel::SERVER_ERROR:   return "ERROR";
			case LogLevel::SERVER_WARNING: return "WARNING";
			case LogLevel::SERVER_INFO:    return "INFO";
			case LogLevel::SERVER_DEBUG:   return "DEBUG";
			default: return "UNKNOWN";
		}
	}

	// Get current timestamp
	std::string getCurrentTimestamp() {
		auto now = std::chrono::system_clock::now();
		auto time_t = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()) % 1000;
		
		std::ostringstream oss;
		oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
		oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
		return oss.str();
	}
};

// Singleton implementation
ServerLogger& ServerLogger::instance() {
	static ServerLogger instance;
	return instance;
=======
ServerLogger::ServerLogger() : minLevel(LogLevel::SERVER_INFO), quietMode(false), showRequestDetails(true)
{
    // Default constructor
>>>>>>> e45dbad8fd9aafe89b192b548e51b6598f36470d
}

ServerLogger::ServerLogger() : pImpl(new Impl()) {}

ServerLogger::~ServerLogger() {
	if (pImpl) {
		if (pImpl->logFile.is_open()) {
			pImpl->logFile.close();
		}
		delete pImpl;
		pImpl = nullptr;
	}
}

void ServerLogger::setLevel(LogLevel level) {
	std::lock_guard<std::mutex> lock(pImpl->logMutex);
	pImpl->minLevel = level;
}

bool ServerLogger::setLogFile(const std::string& filePath) {
	std::lock_guard<std::mutex> lock(pImpl->logMutex);
	
	if (pImpl->logFile.is_open()) {
		pImpl->logFile.close();
	}
	
	pImpl->logFilePath = filePath;
	pImpl->logFile.open(filePath, std::ios::app);
	
	if (!pImpl->logFile.is_open()) {
		std::cerr << "Failed to open log file: " << filePath << std::endl;
		return false;
	}
	
	return true;
}

<<<<<<< HEAD
void ServerLogger::log(LogLevel level, const std::string& message) {
	std::lock_guard<std::mutex> lock(pImpl->logMutex);
	
	// Check if we should log this level
	if (level > pImpl->minLevel) {
		return;
	}
=======
void ServerLogger::setQuietMode(bool enabled)
{
    std::lock_guard<std::mutex> lock(logMutex);
    quietMode = enabled;
}

void ServerLogger::setShowRequestDetails(bool enabled)
{
    std::lock_guard<std::mutex> lock(logMutex);
    showRequestDetails = enabled;
}

bool ServerLogger::setLogFile(const std::string &filePath)
{
    std::lock_guard<std::mutex> lock(logMutex);
>>>>>>> e45dbad8fd9aafe89b192b548e51b6598f36470d

	std::string timestamp = pImpl->getCurrentTimestamp();
	std::string levelStr = pImpl->levelToString(level);
	
	// Create log entry
	LogEntry entry;
	entry.level = level;
	entry.timestamp = timestamp;
	entry.message = message;
	
	// Store in memory
	pImpl->logs.push_back(entry);
	
	// Format message
	std::string formattedMessage = "[" + timestamp + "] [" + levelStr + "] " + message;
	
	// Output to console
	if (level == LogLevel::SERVER_ERROR) {
		std::cerr << formattedMessage << std::endl;
	} else {
		std::cout << formattedMessage << std::endl;
	}
	
	// Output to file
	if (pImpl->logFile.is_open()) {
		pImpl->logFile << formattedMessage << std::endl;
		pImpl->logFile.flush();
	}
}

<<<<<<< HEAD
std::string ServerLogger::formatString(const char* format, va_list args) {
	// Get the required buffer size
	va_list args_copy;
	va_copy(args_copy, args);
	int size = vsnprintf(nullptr, 0, format, args_copy);
	va_end(args_copy);
	
	if (size <= 0) {
		return "";
	}
	
	// Create buffer and format string
	std::string result(size, '\0');
	vsnprintf(&result[0], size + 1, format, args);
	
	return result;
=======
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

    // Filter out routine operational messages in quiet mode
    if (quietMode && level == LogLevel::SERVER_INFO)
    {
        // Allow important startup/shutdown messages but filter routine operations
        if (message.find("New client connection") != std::string::npos ||
            message.find("Processing request") != std::string::npos ||
            message.find("Completed request") != std::string::npos ||
            message.find("Successfully provided") != std::string::npos ||
            message.find("Successfully listed") != std::string::npos)
        {
            return; // Skip these routine messages
        }
    }

    // Filter request details if disabled
    if (!showRequestDetails && level == LogLevel::SERVER_INFO)
    {
        if (message.find("[Thread") != std::string::npos ||
            message.find("Content-Length:") != std::string::npos ||
            message.find("Auth middleware") != std::string::npos ||
            message.find("CORS preflight") != std::string::npos)
        {
            return; // Skip detailed request processing info
        }
    }

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
>>>>>>> e45dbad8fd9aafe89b192b548e51b6598f36470d
}