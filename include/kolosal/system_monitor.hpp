#ifndef KOLOSAL_SYSTEM_MONITOR_HPP
#define KOLOSAL_SYSTEM_MONITOR_HPP

#include "export.hpp"
#include <string>
#include <vector>
#include <memory>

// Forward declaration
namespace kolosal
{
    class EnhancedGPUMonitor;
}

namespace kolosal
{
    struct GPUInfo
    {
        int id;
        std::string name;
        std::string vendor;       // GPU vendor (NVIDIA, AMD, Intel, etc.)
        double utilization;       // GPU utilization percentage (0-100)
        size_t totalMemory;       // Total VRAM in bytes
        size_t usedMemory;        // Used VRAM in bytes
        size_t freeMemory;        // Free VRAM in bytes
        double memoryUtilization; // VRAM utilization percentage (0-100)
        double temperature;       // GPU temperature in Celsius
        double powerUsage;        // Power usage in watts
        std::string driverVersion;
    };

    struct SystemMetrics
    {
        double cpuUsage;           // CPU usage percentage (0-100)
        size_t totalRAM;           // Total RAM in bytes
        size_t usedRAM;            // Used RAM in bytes
        size_t freeRAM;            // Free RAM in bytes
        double ramUtilization;     // RAM utilization percentage (0-100)
        std::vector<GPUInfo> gpus; // GPU information
        std::string timestamp;     // ISO 8601 timestamp
    };

    class KOLOSAL_SERVER_API SystemMonitor
    {
    public:
        SystemMonitor();
        ~SystemMonitor();

        // Get current system metrics
        SystemMetrics getSystemMetrics();

        // Get CPU usage percentage
        double getCPUUsage();

        // Get RAM information
        void getRAMInfo(size_t &totalRAM, size_t &usedRAM, size_t &freeRAM);

        // Get GPU information
        std::vector<GPUInfo> getGPUInfo();

        // Initialize GPU monitoring (checks for NVIDIA, AMD, Intel GPUs)
        bool initializeGPUMonitoring();

        // Check if GPU monitoring is available
        bool isGPUMonitoringAvailable() const;

        // Enable enhanced GPU monitoring using cppuprofile (if available)
        bool enableEnhancedGPUMonitoring();

        // Check if enhanced GPU monitoring is active
        bool isEnhancedGPUMonitoringActive() const;

        // Format bytes to human readable string
        static std::string formatBytes(size_t bytes);

        // Get current timestamp in ISO 8601 format
        static std::string getCurrentTimestamp();

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace kolosal

#endif // KOLOSAL_SYSTEM_MONITOR_HPP
