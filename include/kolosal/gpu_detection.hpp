#ifndef KOLOSAL_GPU_DETECTION_HPP
#define KOLOSAL_GPU_DETECTION_HPP

/**
 * @file gpu_detection.hpp
 * @brief GPU detection utilities for determining hardware acceleration capabilities
 * 
 * This header provides functionality to detect available GPU hardware and determine
 * whether Vulkan-based inference acceleration should be used by default.
 * 
 * Note: On Apple systems (macOS), GPU detection is not needed as Metal acceleration
 * is the preferred default and does not require discrete GPU detection.
 */

namespace kolosal {

/**
 * @brief Detects if the system has a dedicated GPU that supports Vulkan acceleration
 * 
 * This function detects the presence of dedicated GPUs from NVIDIA, AMD, or ATI that
 * are capable of Vulkan acceleration.
 * 
 * On Windows systems, it uses Windows Management Instrumentation (WMI) to query
 * video controllers and check their properties.
 * 
 * On Linux systems, it uses multiple detection methods:
 * - Checks loaded kernel modules (/proc/modules)
 * - Examines DRM devices in /sys/class/drm/
 * - Uses lspci command to list PCI devices
 * - Checks for Vulkan support via vulkaninfo or library presence
 * 
 * On Apple systems (macOS), this function is not typically used as Metal
 * acceleration is preferred and does not require discrete GPU detection.
 * 
 * @return true if a dedicated GPU is detected, false otherwise
 */
bool hasVulkanCapableGPU();

} // namespace kolosal

#endif // KOLOSAL_GPU_DETECTION_HPP
