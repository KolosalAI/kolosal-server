#pragma once

#include "export.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <memory>
#include <iostream>

enum class LogLevel {
	SERVER_ERROR,
	SERVER_WARNING,
	SERVER_INFO,
	SERVER_DEBUG
};

struct LogEntry {
	LogLevel level;
	std::string timestamp;
	std::string message;
};

class KOLOSAL_SERVER_API ServerLogger {
public:
	static ServerLogger& instance();

	ServerLogger(const ServerLogger&) = delete;
	ServerLogger& operator=(const ServerLogger&) = delete;
	ServerLogger(ServerLogger&&) = delete;
	ServerLogger& operator=(ServerLogger&&) = delete;

	// Set minimum log level
	void setLevel(LogLevel level);

	// Set log file path
	bool setLogFile(const std::string& filePath);

	// Log methods
	void error(const std::string& message);
	void warning(const std::string& message);
	void info(const std::string& message);
	void debug(const std::string& message);

	void error(const char* format, ...);
	void warning(const char* format, ...);
	void info(const char* format, ...);
	void debug(const char* format, ...);

	static void logError(const std::string& message);
	static void logWarning(const std::string& message);
	static void logInfo(const std::string& message);
	static void logDebug(const std::string& message);

	static void logError(const char* format, ...);
	static void logWarning(const char* format, ...);
	static void logInfo(const char* format, ...);
	static void logDebug(const char* format, ...);

	// Get stored logs
	const std::vector<LogEntry>& getLogs() const;

private:
	// Private constructor for singleton
	ServerLogger();
	~ServerLogger();

	void log(LogLevel level, const std::string& message);

	std::string formatString(const char* format, va_list args);

	// Get string representation of log level
	std::string levelToString(LogLevel level);

	// Get current timestamp
	std::string getCurrentTimestamp();

	LogLevel minLevel;
	std::vector<LogEntry> logs;
	std::ofstream logFile;
	std::string logFilePath;
	std::mutex logMutex;
};