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

ServerLogger::ServerLogger() : minLevel(LogLevel::SERVER_INFO), quietMode(false), showRequestDetails(true)
{
    // Default constructor
}

ServerLogger::~ServerLogger() {
	if (logFile.is_open()) {
		logFile.close();
	}
}

void ServerLogger::setLevel(LogLevel level) {
	std::lock_guard<std::mutex> lock(logMutex);
	minLevel = level;
}

bool ServerLogger::setLogFile(const std::string& filePath) {
	std::lock_guard<std::mutex> lock(logMutex);
	
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

void ServerLogger::log(LogLevel level, const std::string& message) {
	std::lock_guard<std::mutex> lock(logMutex);
	
	// Check if we should log this level
	if (level > minLevel) {
		return;
	}

	std::string timestamp = getCurrentTimestamp();
	std::string levelStr = levelToString(level);
	
	// Create log entry
	LogEntry entry;
	entry.level = level;
	entry.timestamp = timestamp;
	entry.message = message;
	
	// Store in memory with size limit
	const size_t maxLogEntries = 1000;
	if (logs.size() >= maxLogEntries) {
		logs.erase(logs.begin(), logs.begin() + (logs.size() - maxLogEntries + 1));
	}
	logs.push_back(entry);
	
	// Format message
	std::string formattedMessage = "[" + timestamp + "] [" + levelStr + "] " + message;
	
	// Output to console (only if not in quiet mode or if it's an error)
	if (!quietMode || level == LogLevel::SERVER_ERROR) {
		if (level == LogLevel::SERVER_ERROR) {
			std::cerr << formattedMessage << std::endl;
		} else {
			std::cout << formattedMessage << std::endl;
		}
	}
	
	// Output to file
	if (logFile.is_open()) {
		logFile << formattedMessage << std::endl;
		logFile.flush();
	}
}

std::string ServerLogger::formatString(const char* format, va_list args) {
	// Security check to prevent format string vulnerabilities
	if (!format) {
		return "";
	}
	
	// Get the required buffer size
	va_list args_copy;
	va_copy(args_copy, args);
	int size = vsnprintf(nullptr, 0, format, args_copy);
	va_end(args_copy);
	
	if (size <= 0) {
		return "";
	}
	
	// Limit maximum log message size to prevent memory exhaustion
	const int maxLogSize = 8192; // 8KB max log message
	if (size > maxLogSize) {
		size = maxLogSize;
	}
	
	// Create buffer and format string
	std::string result(size, '\0');
	int written = vsnprintf(&result[0], size + 1, format, args);
	
	if (written < 0) {
		return ""; // Formatting error
	}
	
	if (written > size) {
		result.resize(size);
		result += "... [truncated]";
	} else {
		result.resize(written);
	}
	
	return result;
}

std::string ServerLogger::getCurrentTimestamp() {
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;
	
	std::stringstream ss;
#ifdef _WIN32
	std::tm tm_buf;
	if (localtime_s(&tm_buf, &time_t_now) == 0) {
		ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
	}
#else
	ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
#endif
	ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
	
	return ss.str();
}

std::string ServerLogger::levelToString(LogLevel level) {
	switch (level) {
		case LogLevel::SERVER_ERROR:   return "ERROR";
		case LogLevel::SERVER_WARNING: return "WARNING";
		case LogLevel::SERVER_INFO:    return "INFO";
		case LogLevel::SERVER_DEBUG:   return "DEBUG";
		default:                       return "UNKNOWN";
	}
}

// Singleton instance method
ServerLogger& ServerLogger::instance() {
	static ServerLogger instance;
	return instance;
}

// Public logging methods
void ServerLogger::error(const std::string& message) {
	log(LogLevel::SERVER_ERROR, message);
}

void ServerLogger::warning(const std::string& message) {
	log(LogLevel::SERVER_WARNING, message);
}

void ServerLogger::info(const std::string& message) {
	log(LogLevel::SERVER_INFO, message);
}

void ServerLogger::debug(const std::string& message) {
	log(LogLevel::SERVER_DEBUG, message);
}

// Formatted logging methods
void ServerLogger::error(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = formatString(format, args);
	va_end(args);
	log(LogLevel::SERVER_ERROR, message);
}

void ServerLogger::warning(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = formatString(format, args);
	va_end(args);
	log(LogLevel::SERVER_WARNING, message);
}

void ServerLogger::info(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = formatString(format, args);
	va_end(args);
	log(LogLevel::SERVER_INFO, message);
}

void ServerLogger::debug(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = formatString(format, args);
	va_end(args);
	log(LogLevel::SERVER_DEBUG, message);
}

// Static logging methods
void ServerLogger::logError(const std::string& message) {
	instance().error(message);
}

void ServerLogger::logWarning(const std::string& message) {
	instance().warning(message);
}

void ServerLogger::logInfo(const std::string& message) {
	instance().info(message);
}

void ServerLogger::logDebug(const std::string& message) {
	instance().debug(message);
}

void ServerLogger::logError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = instance().formatString(format, args);
	va_end(args);
	instance().error(message);
}

void ServerLogger::logWarning(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = instance().formatString(format, args);
	va_end(args);
	instance().warning(message);
}

void ServerLogger::logInfo(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = instance().formatString(format, args);
	va_end(args);
	instance().info(message);
}

void ServerLogger::logDebug(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string message = instance().formatString(format, args);
	va_end(args);
	instance().debug(message);
}

// Configuration methods
void ServerLogger::setQuietMode(bool enabled) {
	std::lock_guard<std::mutex> lock(logMutex);
	quietMode = enabled;
}

void ServerLogger::setShowRequestDetails(bool enabled) {
	std::lock_guard<std::mutex> lock(logMutex);
	showRequestDetails = enabled;
}

// Get stored logs
std::vector<LogEntry> ServerLogger::getLogs() const {
	std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(logMutex));
	return logs;
}