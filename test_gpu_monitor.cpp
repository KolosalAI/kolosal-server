#include "include/kolosal/system_monitor.hpp"
#include "include/kolosal/logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        // Initialize logger
        ServerLogger::setLogLevel("info");
        ServerLogger::logInfo("Testing Enhanced GPU Monitor Integration");
        
        // Create system monitor
        kolosal::SystemMonitor monitor;
        
        // Test basic functionality
        std::cout << "=== System Monitor Integration Test ===" << std::endl;
        std::cout << "Testing GPU monitoring availability..." << std::endl;
        
        bool gpuAvailable = monitor.isGPUMonitoringAvailable();
        std::cout << "GPU monitoring available: " << (gpuAvailable ? "Yes" : "No") << std::endl;
        
        // Test enhanced GPU monitoring
        std::cout << "Testing enhanced GPU monitoring..." << std::endl;
        bool enhancedEnabled = monitor.enableEnhancedGPUMonitoring();
        std::cout << "Enhanced GPU monitoring enabled: " << (enhancedEnabled ? "Yes" : "No") << std::endl;
        
        bool enhancedActive = monitor.isEnhancedGPUMonitoringActive();
        std::cout << "Enhanced GPU monitoring active: " << (enhancedActive ? "Yes" : "No") << std::endl;
        
        // Get system metrics
        std::cout << "Getting system metrics..." << std::endl;
        auto metrics = monitor.getSystemMetrics();
        
        std::cout << "CPU Usage: " << metrics.cpuUsage << "%" << std::endl;
        std::cout << "RAM Usage: " << metrics.ramUtilization << "%" << std::endl;
        std::cout << "Number of GPUs detected: " << metrics.gpus.size() << std::endl;
        
        // Display GPU information if available
        for (size_t i = 0; i < metrics.gpus.size(); ++i) {
            const auto& gpu = metrics.gpus[i];
            std::cout << "GPU " << i << ":" << std::endl;
            std::cout << "  Name: " << gpu.name << std::endl;
            std::cout << "  Utilization: " << gpu.utilization << "%" << std::endl;
            std::cout << "  Memory: " << (gpu.usedMemory / 1024 / 1024) << " MB / " 
                     << (gpu.totalMemory / 1024 / 1024) << " MB" << std::endl;
        }
        
        std::cout << "=== Test Completed Successfully ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
