#include "kolosal/gpu_detection.hpp"

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>
#include <iostream>

#pragma comment(lib, "wbemuuid.lib")
#else
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <array>
#endif

namespace kolosal {

#ifdef _WIN32
bool hasVulkanCapableGPU() {
    bool useVulkan = false;

    // Initialize COM
    HRESULT hres = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cerr << "[Error] Failed to initialize COM library. HR = 0x"
                  << std::hex << hres << std::endl;
        return useVulkan;
    }

    // Set COM security levels
    hres = CoInitializeSecurity(
        nullptr,
        -1,
        nullptr,
        nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE,
        nullptr
    );

    if (FAILED(hres) && hres != RPC_E_TOO_LATE) { // Ignore if security is already initialized
        std::cerr << "[Error] Failed to initialize security. HR = 0x"
                  << std::hex << hres << std::endl;
        CoUninitialize();
        return useVulkan;
    }

    // Obtain the initial locator to WMI
    IWbemLocator* pLoc = nullptr;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        reinterpret_cast<LPVOID*>(&pLoc)
    );

    if (FAILED(hres)) {
        std::cerr << "[Error] Failed to create IWbemLocator object. HR = 0x"
                  << std::hex << hres << std::endl;
        CoUninitialize();
        return useVulkan;
    }

    // Connect to the ROOT\CIMV2 namespace
    IWbemServices* pSvc = nullptr;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        nullptr,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        &pSvc
    );

    if (FAILED(hres)) {
        std::cerr << "[Error] Could not connect to WMI. HR = 0x"
                  << std::hex << hres << std::endl;
        pLoc->Release();
        CoUninitialize();
        return useVulkan;
    }

    // Set security levels on the WMI proxy
    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE
    );

    if (FAILED(hres)) {
        std::cerr << "[Error] Could not set proxy blanket. HR = 0x"
                  << std::hex << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return useVulkan;
    }

    // Query for all video controllers
    IEnumWbemClassObject* pEnumerator = nullptr;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_VideoController WHERE VideoProcessor IS NOT NULL"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator
    );

    if (FAILED(hres)) {
        std::cerr << "[Error] WMI query for Win32_VideoController failed. HR = 0x"
                  << std::hex << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return useVulkan;
    }

    // Enumerate the results
    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        // Check multiple properties to improve detection reliability
        VARIANT vtName, vtDesc, vtProcName;
        bool isGPUFound = false;

        // Check Name property
        hr = pclsObj->Get(L"Name", 0, &vtName, 0, 0);
        if (SUCCEEDED(hr) && vtName.vt == VT_BSTR && vtName.bstrVal != nullptr) {
            std::wstring name = vtName.bstrVal;
            if (name.find(L"NVIDIA") != std::wstring::npos ||
                name.find(L"AMD") != std::wstring::npos ||
                name.find(L"ATI") != std::wstring::npos ||
                name.find(L"Radeon") != std::wstring::npos) {
                isGPUFound = true;
            }
        }
        VariantClear(&vtName);

        // Check Description property if GPU not found yet
        if (!isGPUFound) {
            hr = pclsObj->Get(L"Description", 0, &vtDesc, 0, 0);
            if (SUCCEEDED(hr) && vtDesc.vt == VT_BSTR && vtDesc.bstrVal != nullptr) {
                std::wstring desc = vtDesc.bstrVal;
                if (desc.find(L"NVIDIA") != std::wstring::npos ||
                    desc.find(L"AMD") != std::wstring::npos ||
                    desc.find(L"ATI") != std::wstring::npos ||
                    desc.find(L"Radeon") != std::wstring::npos) {
                    isGPUFound = true;
                }
            }
            VariantClear(&vtDesc);
        }

        // Check VideoProcessor property if GPU not found yet
        if (!isGPUFound) {
            hr = pclsObj->Get(L"VideoProcessor", 0, &vtProcName, 0, 0);
            if (SUCCEEDED(hr) && vtProcName.vt == VT_BSTR && vtProcName.bstrVal != nullptr) {
                std::wstring procName = vtProcName.bstrVal;
                if (procName.find(L"NVIDIA") != std::wstring::npos ||
                    procName.find(L"AMD") != std::wstring::npos ||
                    procName.find(L"ATI") != std::wstring::npos ||
                    procName.find(L"Radeon") != std::wstring::npos) {
                    isGPUFound = true;
                }
            }
            VariantClear(&vtProcName);
        }

        if (isGPUFound) {
            useVulkan = true;
            pclsObj->Release();
            break;
        }

        pclsObj->Release();
    }

    // Cleanup
    pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();

    return useVulkan;
}

#else

// Helper function to execute a command and capture its output
std::string executeCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Check if string contains any of the GPU vendor names (case insensitive)
bool containsGPUVendor(const std::string& text) {
    std::string lowerText = text;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
    
    return lowerText.find("nvidia") != std::string::npos ||
           lowerText.find("amd") != std::string::npos ||
           lowerText.find("ati") != std::string::npos ||
           lowerText.find("radeon") != std::string::npos ||
           lowerText.find("geforce") != std::string::npos ||
           lowerText.find("quadro") != std::string::npos ||
           lowerText.find("tesla") != std::string::npos ||
           lowerText.find("firepro") != std::string::npos ||
           lowerText.find("rx ") != std::string::npos ||
           lowerText.find("gtx") != std::string::npos ||
           lowerText.find("rtx") != std::string::npos;
}

// Method 1: Check loaded kernel modules
bool checkKernelModules() {
    try {
        std::ifstream modules("/proc/modules");
        if (!modules.is_open()) {
            return false;
        }
        
        std::string line;
        while (std::getline(modules, line)) {
            if (line.find("nvidia") != std::string::npos ||
                line.find("amdgpu") != std::string::npos ||
                line.find("radeon") != std::string::npos ||
                line.find("nouveau") != std::string::npos) {
                return true;
            }
        }
    } catch (const std::exception&) {
        // Ignore errors and try other methods
    }
    return false;
}

// Method 2: Check DRM devices
bool checkDRMDevices() {
    try {
        if (!std::filesystem::exists("/sys/class/drm")) {
            return false;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm")) {
            if (entry.is_directory()) {
                std::string dirname = entry.path().filename().string();
                
                // Look for card devices (not render nodes)
                if (dirname.find("card") == 0 && dirname.find("-") == std::string::npos) {
                    // Check the device vendor/device info
                    std::string devicePath = entry.path().string() + "/device";
                    
                    // Try to read vendor information
                    std::ifstream vendor(devicePath + "/vendor");
                    std::ifstream device(devicePath + "/device");
                    
                    if (vendor.is_open() && device.is_open()) {
                        std::string vendorId, deviceId;
                        std::getline(vendor, vendorId);
                        std::getline(device, deviceId);
                        
                        // Check for known GPU vendor IDs
                        // 0x10de = NVIDIA, 0x1002 = AMD, 0x1022 = AMD
                        if (vendorId.find("0x10de") != std::string::npos ||
                            vendorId.find("0x1002") != std::string::npos ||
                            vendorId.find("0x1022") != std::string::npos) {
                            return true;
                        }
                    }
                }
            }
        }
    } catch (const std::exception&) {
        // Ignore errors and try other methods
    }
    return false;
}

// Method 3: Use lspci command
bool checkLspci() {
    try {
        std::string lspciOutput = executeCommand("lspci 2>/dev/null");
        if (lspciOutput.empty()) {
            return false;
        }
        
        std::istringstream stream(lspciOutput);
        std::string line;
        
        while (std::getline(stream, line)) {
            // Look for VGA or 3D controller entries
            if ((line.find("VGA") != std::string::npos || 
                 line.find("3D controller") != std::string::npos ||
                 line.find("Display controller") != std::string::npos) &&
                containsGPUVendor(line)) {
                return true;
            }
        }
    } catch (const std::exception&) {
        // Ignore errors and continue
    }
    return false;
}

// Method 4: Check for Vulkan support
bool checkVulkanSupport() {
    try {
        // Check if vulkan-tools or vulkaninfo is available
        std::string vulkanInfo = executeCommand("vulkaninfo --summary 2>/dev/null | head -20");
        if (!vulkanInfo.empty() && vulkanInfo.find("ERROR") == std::string::npos) {
            // If vulkaninfo runs successfully and doesn't show errors, likely have Vulkan support
            return vulkanInfo.find("GPU") != std::string::npos || 
                   vulkanInfo.find("Device") != std::string::npos;
        }
        
        // Alternative: check for Vulkan loader library
        return std::filesystem::exists("/usr/lib/x86_64-linux-gnu/libvulkan.so.1") ||
               std::filesystem::exists("/usr/lib64/libvulkan.so.1") ||
               std::filesystem::exists("/usr/lib/libvulkan.so.1");
    } catch (const std::exception&) {
        // Ignore errors
    }
    return false;
}

bool hasVulkanCapableGPU() {
#ifdef __APPLE__
    // On Apple systems, we don't need Vulkan GPU detection since Metal is preferred
    // Always return false to ensure Metal is used as the primary acceleration method
    return false;
#else
    // Method 1: Check kernel modules (most reliable)
    if (checkKernelModules()) {
        return true;
    }
    
    // Method 2: Check DRM devices
    if (checkDRMDevices()) {
        return true;
    }
    
    // Method 3: Use lspci command
    if (checkLspci()) {
        return true;
    }
    
    // Method 4: Check Vulkan support (indicates GPU presence)
    if (checkVulkanSupport()) {
        return true;
    }
    
    // If all methods fail, assume no dedicated GPU
    return false;
#endif
}

#endif

} // namespace kolosal
