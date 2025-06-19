#ifndef KOLOSAL_ENHANCED_GPU_MONITOR_HPP
#define KOLOSAL_ENHANCED_GPU_MONITOR_HPP

#include "system_monitor.hpp"
#include <memory>

namespace kolosal {

    /**
     * Enhanced GPU Monitor that uses cppuprofile library for improved GPU monitoring
     * capabilities, particularly for NVIDIA GPUs.
     */
    class EnhancedGPUMonitor {
    public:
        EnhancedGPUMonitor();
        ~EnhancedGPUMonitor();

        // Initialize GPU monitoring with cppuprofile
        bool initialize();

        // Start continuous GPU monitoring
        void startMonitoring(int updateIntervalMs = 1000);

        // Stop GPU monitoring
        void stopMonitoring();

        // Get current GPU information using cppuprofile
        std::vector<GPUInfo> getGPUInfo();

        // Check if GPU monitoring is available
        bool isAvailable() const;

        // Get the number of detected GPUs
        size_t getGPUCount() const;

        // Check if currently monitoring
        bool isMonitoring() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace kolosal

#endif // KOLOSAL_ENHANCED_GPU_MONITOR_HPP
