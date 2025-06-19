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

void ServerLogger::log(LogLevel level, const std::string& message) {
	std::lock_guard<std::mutex> lock(pImpl->logMutex);
	
	// Check if we should log this level
	if (level > pImpl->minLevel) {
		return;
	}

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
}

// Instance methods
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

// Static methods
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

// Get stored logs
std::vector<LogEntry> ServerLogger::getLogs() const {
	std::lock_guard<std::mutex> lock(pImpl->logMutex);
	return pImpl->logs; // Returns a copy
}