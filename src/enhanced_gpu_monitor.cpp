#include "kolosal/enhanced_gpu_monitor.hpp"
#include "kolosal/logger.hpp"

#ifdef USE_CPPUPROFILE_GPU
#include <uprofile.h>
#include <monitors/nvidiamonitor.h>
#endif

#include <vector>
#include <string>
#include <thread>
#include <chrono>

namespace kolosal {

    struct EnhancedGPUMonitor::Impl {
        bool initialized = false;
        bool monitoring = false;
        size_t gpuCount = 0;
        
#ifdef USE_CPPUPROFILE_GPU
        std::unique_ptr<uprofile::NvidiaMonitor> nvidiaMonitor;
#endif

        Impl() {
#ifdef USE_CPPUPROFILE_GPU
            ServerLogger::logInfo("Enhanced GPU Monitor: cppuprofile support enabled");
#else
            ServerLogger::logInfo("Enhanced GPU Monitor: cppuprofile support not compiled");
#endif
        }

        ~Impl() {
            if (monitoring) {
                stopMonitoring();
            }
        }

        bool initialize() {
#ifdef USE_CPPUPROFILE_GPU
            try {
                // Create and configure NVIDIA monitor
                nvidiaMonitor = std::make_unique<uprofile::NvidiaMonitor>();
                
                // Add the monitor to uprofile
                uprofile::addGPUMonitor(nvidiaMonitor.get());
                
                // Test if GPUs are available by trying to get usage
                const std::vector<float>& usage = nvidiaMonitor->getUsage();
                gpuCount = usage.size();
                
                if (gpuCount > 0) {
                    initialized = true;
                    ServerLogger::logInfo("Enhanced GPU Monitor: Initialized with %zu NVIDIA GPU(s)", gpuCount);
                    return true;
                } else {
                    ServerLogger::logWarning("Enhanced GPU Monitor: No NVIDIA GPUs detected");
                    uprofile::removeGPUMonitor();
                    nvidiaMonitor.reset();
                    return false;
                }
            } catch (const std::exception& e) {
                ServerLogger::logError("Enhanced GPU Monitor: Failed to initialize - %s", e.what());
                if (nvidiaMonitor) {
                    uprofile::removeGPUMonitor();
                    nvidiaMonitor.reset();
                }
                return false;
            }
#else
            ServerLogger::logWarning("Enhanced GPU Monitor: cppuprofile support not compiled - GPU monitoring unavailable");
            return false;
#endif
        }

        void startMonitoring(int updateIntervalMs) {
#ifdef USE_CPPUPROFILE_GPU
            if (!initialized || monitoring) {
                return;
            }

            try {
                // Start GPU usage and memory monitoring with cppuprofile
                nvidiaMonitor->start(updateIntervalMs);
                monitoring = true;
                
                ServerLogger::logInfo("Enhanced GPU Monitor: Started monitoring with %dms interval", updateIntervalMs);
            } catch (const std::exception& e) {
                ServerLogger::logError("Enhanced GPU Monitor: Failed to start monitoring - %s", e.what());
            }
#endif
        }

        void stopMonitoring() {
#ifdef USE_CPPUPROFILE_GPU
            if (!monitoring) {
                return;
            }

            try {
                if (nvidiaMonitor) {
                    nvidiaMonitor->stop();
                }
                monitoring = false;
                ServerLogger::logInfo("Enhanced GPU Monitor: Stopped monitoring");
            } catch (const std::exception& e) {
                ServerLogger::logError("Enhanced GPU Monitor: Error stopping monitoring - %s", e.what());
            }
#endif
        }

        std::vector<GPUInfo> getGPUInfo() {
            std::vector<GPUInfo> gpus;

#ifdef USE_CPPUPROFILE_GPU
            if (!initialized || !nvidiaMonitor) {
                return gpus;
            }

            try {
                // Get current usage data
                const std::vector<float>& usage = nvidiaMonitor->getUsage();
                
                // Get memory data
                std::vector<int> usedMemKiB, totalMemKiB;
                nvidiaMonitor->getMemory(usedMemKiB, totalMemKiB);
                
                // Create GPUInfo objects
                for (size_t i = 0; i < usage.size() && i < usedMemKiB.size() && i < totalMemKiB.size(); ++i) {
                    GPUInfo gpu;
                    gpu.id = static_cast<int>(i);
                    gpu.name = "NVIDIA GPU " + std::to_string(i);  // cppuprofile doesn't provide names
                    gpu.utilization = static_cast<double>(usage[i]);
                    
                    // Convert from KiB to bytes
                    gpu.totalMemory = static_cast<size_t>(totalMemKiB[i]) * 1024;
                    gpu.usedMemory = static_cast<size_t>(usedMemKiB[i]) * 1024;
                    gpu.freeMemory = gpu.totalMemory - gpu.usedMemory;
                    
                    if (gpu.totalMemory > 0) {
                        gpu.memoryUtilization = (static_cast<double>(gpu.usedMemory) / static_cast<double>(gpu.totalMemory)) * 100.0;
                    } else {
                        gpu.memoryUtilization = -1.0;
                    }
                    
                    // cppuprofile doesn't provide these details
                    gpu.temperature = -1.0;
                    gpu.powerUsage = -1.0;
                    gpu.driverVersion = (i == 0) ? "Available via cppuprofile" : "";
                    
                    gpus.push_back(gpu);
                }
                
                ServerLogger::logDebug("Enhanced GPU Monitor: Retrieved info for %zu GPU(s)", gpus.size());
            } catch (const std::exception& e) {
                ServerLogger::logError("Enhanced GPU Monitor: Error getting GPU info - %s", e.what());
            }
#endif

            return gpus;
        }
    };

    EnhancedGPUMonitor::EnhancedGPUMonitor() : pImpl(std::make_unique<Impl>()) {}

    EnhancedGPUMonitor::~EnhancedGPUMonitor() = default;

    bool EnhancedGPUMonitor::initialize() {
        return pImpl->initialize();
    }

    void EnhancedGPUMonitor::startMonitoring(int updateIntervalMs) {
        pImpl->startMonitoring(updateIntervalMs);
    }

    void EnhancedGPUMonitor::stopMonitoring() {
        pImpl->stopMonitoring();
    }

    std::vector<GPUInfo> EnhancedGPUMonitor::getGPUInfo() {
        return pImpl->getGPUInfo();
    }

    bool EnhancedGPUMonitor::isAvailable() const {
        return pImpl->initialized;
    }

    size_t EnhancedGPUMonitor::getGPUCount() const {
        return pImpl->gpuCount;
    }

    bool EnhancedGPUMonitor::isMonitoring() const {
        return pImpl->monitoring;
    }

} // namespace kolosal
