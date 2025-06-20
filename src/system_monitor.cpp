#include "kolosal/system_monitor.hpp"
#include "kolosal/enhanced_gpu_monitor.hpp"
#include "kolosal/logger.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <comdef.h>
#include <wbemidl.h>
#include <dxgi.h>
#include <d3d11.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "wbemuuid.lib")
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#include <fstream>
#endif

// Check if NVML is available
#ifdef NVML_AVAILABLE
#include <nvml.h>
#define NVML_SUPPORT 1
#else
#define NVML_SUPPORT 0
// Define minimal NVML types and constants when not available
#define NVML_SUCCESS 0
#define NVML_ERROR_UNINITIALIZED 1
#define NVML_DEVICE_NAME_BUFFER_SIZE 64
#define NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE 80
#define NVML_TEMPERATURE_GPU 0
typedef int nvmlReturn_t;
typedef void *nvmlDevice_t;

struct nvmlUtilization_t
{
    unsigned int gpu;
    unsigned int memory;
};

struct nvmlMemory_t
{
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

inline const char *nvmlErrorString(nvmlReturn_t result)
{
    return "NVML not available";
}
#endif

namespace kolosal
{

    struct SystemMonitor::Impl
    {
        bool nvmlInitialized = false;
        bool gpuMonitoringAvailable = false;
        std::unique_ptr<EnhancedGPUMonitor> enhancedGPUMonitor;
        bool useEnhancedGPUMonitoring = false;

#ifdef _WIN32
        PDH_HQUERY cpuQuery = nullptr;
        PDH_HCOUNTER cpuTotal = nullptr;
        bool pdhInitialized = false;
#endif

        Impl()
        {
            initializeCPUMonitoring();
            initializeGPUMonitoring();

            // Try to initialize enhanced GPU monitoring
            enhancedGPUMonitor = std::make_unique<EnhancedGPUMonitor>();
            if (enhancedGPUMonitor->initialize())
            {
                useEnhancedGPUMonitoring = true;
                gpuMonitoringAvailable = true;
                ServerLogger::logInfo("Enhanced GPU monitoring initialized successfully");
            }
            else
            {
                ServerLogger::logInfo("Enhanced GPU monitoring not available, falling back to basic monitoring");
            }

            // Check if any GPUs are available via comprehensive detection
            if (!gpuMonitoringAvailable)
            {
                auto detectedGPUs = detectAllGPUs();
                if (!detectedGPUs.empty())
                {
                    gpuMonitoringAvailable = true;
                    ServerLogger::logInfo("GPU monitoring available via comprehensive detection (%zu GPU(s) found)", detectedGPUs.size());
                }
            }
        }

        ~Impl()
        {
#ifdef _WIN32
            if (pdhInitialized && cpuQuery)
            {
                PdhCloseQuery(cpuQuery);
            }
#endif
#if NVML_SUPPORT
            if (nvmlInitialized)
            {
                nvmlShutdown();
            }
#endif
            // Enhanced GPU monitor destructor will handle cleanup automatically
        }

        void initializeCPUMonitoring()
        {
#ifdef _WIN32
            PDH_STATUS status = PdhOpenQuery(NULL, NULL, &cpuQuery);
            if (status == ERROR_SUCCESS)
            {
                status = PdhAddEnglishCounterA(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
                if (status == ERROR_SUCCESS)
                {
                    PdhCollectQueryData(cpuQuery);
                    pdhInitialized = true;
                    ServerLogger::logInfo("CPU monitoring initialized successfully");
                }
                else
                {
                    ServerLogger::logError("Failed to add CPU counter: %d", status);
                }
            }
            else
            {
                ServerLogger::logError("Failed to open PDH query: %d", status);
            }
#endif
        }

        bool initializeGPUMonitoring()
        {
#if NVML_SUPPORT
            // Try to initialize NVML for NVIDIA GPUs
            nvmlReturn_t result = nvmlInit();
            if (result == NVML_SUCCESS)
            {
                nvmlInitialized = true;
                gpuMonitoringAvailable = true;
                ServerLogger::logInfo("NVIDIA GPU monitoring initialized successfully");

                unsigned int deviceCount;
                nvmlDeviceGetCount(&deviceCount);
                ServerLogger::logInfo("Found %u NVIDIA GPU(s)", deviceCount);

                return true;
            }
            else
            {
                ServerLogger::logWarning("NVML initialization failed: %s", nvmlErrorString(result));
                ServerLogger::logInfo("GPU monitoring will not be available for NVIDIA cards");
            }
#else
            ServerLogger::logInfo("NVML support not compiled in - GPU monitoring will be limited");
#endif

            // Initialize comprehensive GPU detection for all vendors
            initializeComprehensiveGPUDetection();

            return false;
        }

        void initializeComprehensiveGPUDetection()
        {
#ifdef _WIN32
            // Initialize COM for WMI queries
            HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
            if (FAILED(hr))
            {
                ServerLogger::logWarning("Failed to initialize COM for GPU detection");
                return;
            }

            // Set general COM security levels
            hr = CoInitializeSecurity(
                NULL, -1, NULL, NULL,
                RPC_C_AUTHN_LEVEL_NONE,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                NULL, EOAC_NONE, NULL);

            if (FAILED(hr) && hr != RPC_E_TOO_LATE)
            {
                ServerLogger::logWarning("Failed to set COM security for GPU detection");
            }

            ServerLogger::logInfo("Comprehensive GPU detection initialized for Windows");
#else
            // Linux implementation would go here
            ServerLogger::logInfo("Comprehensive GPU detection initialized for Linux");
#endif
        }

        std::vector<GPUInfo> detectAllGPUs()
        {
            std::vector<GPUInfo> gpus;

#ifdef _WIN32
            // Method 1: Try DXGI first (works for all modern GPUs)
            auto dxgiGPUs = detectGPUsViaDXGI();
            if (!dxgiGPUs.empty())
            {
                gpus.insert(gpus.end(), dxgiGPUs.begin(), dxgiGPUs.end());
            }

            // Method 2: Try WMI for additional information
            auto wmiGPUs = detectGPUsViaWMI();

            // Merge WMI data with DXGI data or use WMI if DXGI failed
            if (gpus.empty() && !wmiGPUs.empty())
            {
                gpus = wmiGPUs;
            }
            else
            {
                // Enhance DXGI data with WMI information
                enhanceGPUDataWithWMI(gpus, wmiGPUs);
            }
#else
            // Linux: Try different methods
            auto sysfsGPUs = detectGPUsViaSysfs();
            gpus.insert(gpus.end(), sysfsGPUs.begin(), sysfsGPUs.end());
#endif

            return gpus;
        }

#ifdef _WIN32
        std::vector<GPUInfo> detectGPUsViaDXGI()
        {
            std::vector<GPUInfo> gpus;

            IDXGIFactory *pFactory = nullptr;
            HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&pFactory);
            if (FAILED(hr))
            {
                ServerLogger::logWarning("Failed to create DXGI factory for GPU detection");
                return gpus;
            }

            UINT adapterIndex = 0;
            IDXGIAdapter *pAdapter = nullptr;

            while (pFactory->EnumAdapters(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND)
            {
                DXGI_ADAPTER_DESC desc;
                hr = pAdapter->GetDesc(&desc);
                if (SUCCEEDED(hr))
                {
                    GPUInfo gpu;
                    gpu.id = adapterIndex;

                    // Convert wide string to regular string
                    int size = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);
                    std::string name(size, 0);
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, &name[0], size, nullptr, nullptr);
                    if (!name.empty() && name.back() == '\0')
                    {
                        name.pop_back(); // Remove null terminator
                    }
                    gpu.name = name;

                    // Detect vendor based on name and vendor ID
                    gpu.vendor = detectGPUVendor(gpu.name, desc.VendorId);

                    gpu.totalMemory = desc.DedicatedVideoMemory;
                    gpu.utilization = -1.0; // Will be filled later if possible
                    gpu.usedMemory = 0;
                    gpu.freeMemory = gpu.totalMemory;
                    gpu.memoryUtilization = 0.0;
                    gpu.temperature = -1.0;
                    gpu.powerUsage = -1.0;
                    gpu.driverVersion = "Unknown";

                    // Try to get more detailed information via D3D11
                    enhanceGPUInfoWithD3D11(gpu, pAdapter);

                    gpus.push_back(gpu);
                    ServerLogger::logInfo("Detected GPU via DXGI: %s [%s] (VRAM: %llu MB)",
                                          gpu.name.c_str(), gpu.vendor.c_str(), gpu.totalMemory / (1024 * 1024));
                }

                pAdapter->Release();
                adapterIndex++;
            }

            pFactory->Release();
            return gpus;
        }

        void enhanceGPUInfoWithD3D11(GPUInfo &gpu, IDXGIAdapter *pAdapter)
        {
            ID3D11Device *pDevice = nullptr;
            ID3D11DeviceContext *pContext = nullptr;

            HRESULT hr = D3D11CreateDevice(
                pAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
                D3D11_SDK_VERSION, &pDevice, nullptr, &pContext);

            if (SUCCEEDED(hr))
            {
                // Device created successfully - GPU is functional
                // Could add more detailed queries here if needed

                pContext->Release();
                pDevice->Release();
            }
        }

        std::vector<GPUInfo> detectGPUsViaWMI()
        {
            std::vector<GPUInfo> gpus;

            IWbemLocator *pLoc = nullptr;
            IWbemServices *pSvc = nullptr;
            IEnumWbemClassObject *pEnumerator = nullptr;

            HRESULT hr = CoCreateInstance(
                CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                IID_IWbemLocator, (LPVOID *)&pLoc);

            if (FAILED(hr))
            {
                ServerLogger::logWarning("Failed to create WbemLocator for GPU detection");
                return gpus;
            }

            hr = pLoc->ConnectServer(
                _bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, NULL, 0, 0, &pSvc);

            if (FAILED(hr))
            {
                ServerLogger::logWarning("Failed to connect to WMI for GPU detection");
                pLoc->Release();
                return gpus;
            }

            hr = CoSetProxyBlanket(
                pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

            if (FAILED(hr))
            {
                ServerLogger::logWarning("Failed to set proxy blanket for WMI");
                pSvc->Release();
                pLoc->Release();
                return gpus;
            }

            hr = pSvc->ExecQuery(
                bstr_t("WQL"),
                bstr_t("SELECT * FROM Win32_VideoController"),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                NULL, &pEnumerator);

            if (FAILED(hr))
            {
                ServerLogger::logWarning("WMI query for video controllers failed");
                pSvc->Release();
                pLoc->Release();
                return gpus;
            }

            IWbemClassObject *pclsObj = nullptr;
            ULONG uReturn = 0;
            int gpuIndex = 0;

            while (pEnumerator)
            {
                hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                if (0 == uReturn)
                    break;

                GPUInfo gpu;
                gpu.id = gpuIndex++;

                VARIANT vtProp;
                VariantInit(&vtProp); // Get GPU name
                hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR)
                {
                    _bstr_t bstrName(vtProp.bstrVal);
                    gpu.name = std::string(bstrName);
                }
                VariantClear(&vtProp);

                // Detect vendor from name
                gpu.vendor = detectGPUVendor(gpu.name);

                // Get adapter RAM
                hr = pclsObj->Get(L"AdapterRAM", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_I4)
                {
                    gpu.totalMemory = static_cast<size_t>(vtProp.uintVal);
                }
                VariantClear(&vtProp);

                // Get driver version
                hr = pclsObj->Get(L"DriverVersion", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR)
                {
                    _bstr_t bstrVersion(vtProp.bstrVal);
                    gpu.driverVersion = std::string(bstrVersion);
                }
                VariantClear(&vtProp);

                // Set default values for metrics that require real-time monitoring
                gpu.utilization = -1.0;
                gpu.usedMemory = 0;
                gpu.freeMemory = gpu.totalMemory;
                gpu.memoryUtilization = 0.0;
                gpu.temperature = -1.0;
                gpu.powerUsage = -1.0;

                gpus.push_back(gpu);
                ServerLogger::logInfo("Detected GPU via WMI: %s [%s]", gpu.name.c_str(), gpu.vendor.c_str());

                pclsObj->Release();
            }

            pEnumerator->Release();
            pSvc->Release();
            pLoc->Release();

            return gpus;
        }

        void enhanceGPUDataWithWMI(std::vector<GPUInfo> &dxgiGPUs, const std::vector<GPUInfo> &wmiGPUs)
        {
            // Match DXGI GPUs with WMI data based on name similarity
            for (auto &dxgiGPU : dxgiGPUs)
            {
                for (const auto &wmiGPU : wmiGPUs)
                {
                    // Simple name matching - could be improved
                    if (dxgiGPU.name.find(wmiGPU.name.substr(0, 10)) != std::string::npos ||
                        wmiGPU.name.find(dxgiGPU.name.substr(0, 10)) != std::string::npos)
                    {

                        if (dxgiGPU.driverVersion == "Unknown")
                        {
                            dxgiGPU.driverVersion = wmiGPU.driverVersion;
                        }

                        // WMI sometimes has more accurate memory info for integrated GPUs
                        if (dxgiGPU.totalMemory == 0 && wmiGPU.totalMemory > 0)
                        {
                            dxgiGPU.totalMemory = wmiGPU.totalMemory;
                            dxgiGPU.freeMemory = wmiGPU.freeMemory;
                        }
                        break;
                    }                }
            }
        }
#else
        std::vector<GPUInfo> detectGPUsViaSysfs()
        {
            std::vector<GPUInfo> gpus;

            try {
                // Check /sys/class/drm/ for GPU devices
                const std::string drm_path = "/sys/class/drm";
                if (!std::filesystem::exists(drm_path)) {
                    ServerLogger::logWarning("DRM subsystem not available - GPU detection limited");
                    return gpus;
                }

                for (const auto& entry : std::filesystem::directory_iterator(drm_path)) {
                    std::string device_name = entry.path().filename().string();
                      // Look for card devices (not render nodes)
                    if (device_name.find("card") == 0 && device_name.find("-") == std::string::npos) {
                        GPUInfo gpu;
                        gpu.id = std::stoi(device_name.substr(4)); // Extract card number
                        
                        // Read device information
                        std::string device_path = entry.path().string() + "/device";
                        std::string vendor_file = device_path + "/vendor";
                        std::string device_file = device_path + "/device";
                        std::string uevent_file = device_path + "/uevent";
                        
                        // Read vendor ID for vendor detection
                        unsigned int vendorId = 0;
                        std::ifstream vendor_stream(vendor_file);
                        if (vendor_stream.is_open()) {
                            std::string vendor_str;
                            vendor_stream >> vendor_str;
                            try {
                                vendorId = std::stoul(vendor_str, nullptr, 16);
                            } catch (...) {
                                vendorId = 0;
                            }
                        }

                        // Read device ID (for future use)
                        unsigned int deviceId = 0;
                        std::ifstream device_stream(device_file);
                        if (device_stream.is_open()) {
                            std::string device_str;
                            device_stream >> device_str;
                            try {
                                deviceId = std::stoul(device_str, nullptr, 16);
                            } catch (...) {
                                deviceId = 0;
                            }
                        }

                        // Try to get PCI bus info from uevent
                        std::string pciBusId;
                        std::ifstream uevent_stream(uevent_file);
                        if (uevent_stream.is_open()) {
                            std::string line;
                            while (std::getline(uevent_stream, line)) {
                                if (line.find("PCI_SLOT_NAME=") == 0) {
                                    pciBusId = line.substr(14);
                                }
                            }
                        }

                        // Set vendor and basic name based on vendor ID
                        gpu.vendor = detectGPUVendor("", vendorId);
                        if (gpu.vendor == "NVIDIA") {
                            gpu.name = "NVIDIA GPU (Card " + std::to_string(gpu.id) + ")";
                        } else if (gpu.vendor == "AMD") {
                            gpu.name = "AMD GPU (Card " + std::to_string(gpu.id) + ")";
                        } else if (gpu.vendor == "Intel") {
                            gpu.name = "Intel GPU (Card " + std::to_string(gpu.id) + ")";
                        } else {
                            gpu.name = "Unknown GPU (Card " + std::to_string(gpu.id) + ")";
                        }

                        // Initialize memory and performance metrics (limited on Linux without specific drivers)
                        gpu.totalMemory = 0;
                        gpu.freeMemory = 0;
                        gpu.usedMemory = 0;
                        gpu.utilization = 0.0;
                        gpu.memoryUtilization = 0.0;
                        gpu.temperature = -1.0; // -1 indicates unknown/unavailable
                        gpu.powerUsage = -1.0;  // -1 indicates unknown/unavailable
                        gpu.driverVersion = "Unknown";                        gpus.push_back(gpu);
                        ServerLogger::logInfo("Detected GPU: %s (Vendor: %s, ID: %d)", 
                                            gpu.name.c_str(), gpu.vendor.c_str(), gpu.id);
                    }
                }

                if (gpus.empty()) {
                    ServerLogger::logInfo("No GPUs detected via sysfs");
                }

            } catch (const std::exception& e) {
                ServerLogger::logError("Error during Linux GPU detection: %s", e.what());
            }

            return gpus;
        }
#endif

        std::string detectGPUVendor(const std::string &name, unsigned int vendorId = 0)
        {
            // Convert name to lowercase for easier matching
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            // Check vendor IDs first (more reliable)
            switch (vendorId)
            {
            case 0x10DE:
                return "NVIDIA";
            case 0x1002:
                return "AMD";
            case 0x8086:
                return "Intel";
            case 0x1414:
                return "Microsoft"; // Software renderer
            default:
                break;
            }

            // Fallback to name-based detection
            if (lowerName.find("nvidia") != std::string::npos ||
                lowerName.find("geforce") != std::string::npos ||
                lowerName.find("gtx") != std::string::npos ||
                lowerName.find("rtx") != std::string::npos ||
                lowerName.find("quadro") != std::string::npos ||
                lowerName.find("tesla") != std::string::npos)
            {
                return "NVIDIA";
            }

            if (lowerName.find("amd") != std::string::npos ||
                lowerName.find("radeon") != std::string::npos ||
                lowerName.find("rx ") != std::string::npos ||
                lowerName.find("vega") != std::string::npos ||
                lowerName.find("navi") != std::string::npos ||
                lowerName.find("polaris") != std::string::npos)
            {
                return "AMD";
            }

            if (lowerName.find("intel") != std::string::npos ||
                lowerName.find("uhd") != std::string::npos ||
                lowerName.find("hd graphics") != std::string::npos ||
                lowerName.find("iris") != std::string::npos ||
                lowerName.find("arc") != std::string::npos)
            {
                return "Intel";
            }

            if (lowerName.find("microsoft") != std::string::npos ||
                lowerName.find("basic render") != std::string::npos)
            {
                return "Microsoft";
            }
            return "Unknown";
        }

        double getCPUUsage()
        {
#ifdef _WIN32
            if (!pdhInitialized)
            {
                return -1.0;
            }

            PDH_FMT_COUNTERVALUE counterVal;
            PDH_STATUS status = PdhCollectQueryData(cpuQuery);
            if (status != ERROR_SUCCESS)
            {
                return -1.0;
            }

            status = PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
            if (status != ERROR_SUCCESS)
            {
                return -1.0;
            }

            return counterVal.doubleValue;
#else
            // Linux implementation using /proc/stat
            static unsigned long long lastTotalUser = 0, lastTotalUserLow = 0, lastTotalSys = 0, lastTotalIdle = 0;

            std::ifstream file("/proc/stat");
            if (!file.is_open())
            {
                return -1.0;
            }

            std::string line;
            std::getline(file, line);
            file.close();

            unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;
            sscanf(line.c_str(), "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow, &totalSys, &totalIdle);

            if (lastTotalUser == 0)
            {
                lastTotalUser = totalUser;
                lastTotalUserLow = totalUserLow;
                lastTotalSys = totalSys;
                lastTotalIdle = totalIdle;
                return 0.0;
            }

            total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) + (totalSys - lastTotalSys);
            double percent = total;
            total += (totalIdle - lastTotalIdle);
            percent /= total;
            percent *= 100;

            lastTotalUser = totalUser;
            lastTotalUserLow = totalUserLow;
            lastTotalSys = totalSys;
            lastTotalIdle = totalIdle;

            return percent;
#endif
        }

        void getRAMInfo(size_t &totalRAM, size_t &usedRAM, size_t &freeRAM)
        {
#ifdef _WIN32
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&memInfo);

            totalRAM = memInfo.ullTotalPhys;
            freeRAM = memInfo.ullAvailPhys;
            usedRAM = totalRAM - freeRAM;
#else
            struct sysinfo memInfo;
            sysinfo(&memInfo);

            totalRAM = memInfo.totalram * memInfo.mem_unit;
            freeRAM = memInfo.freeram * memInfo.mem_unit;
            usedRAM = totalRAM - freeRAM;
#endif
        }
        std::vector<GPUInfo> getGPUInfo()
        {
            std::vector<GPUInfo> gpus;

            // Use enhanced GPU monitoring if available (NVIDIA-specific)
            if (useEnhancedGPUMonitoring && enhancedGPUMonitor && enhancedGPUMonitor->isAvailable())
            {
                gpus = enhancedGPUMonitor->getGPUInfo();
                if (!gpus.empty())
                {
                    ServerLogger::logDebug("Using enhanced GPU monitoring for %zu GPU(s)", gpus.size());
                    return gpus;
                }
            }

            // Try NVML-based monitoring for NVIDIA GPUs
#if NVML_SUPPORT
            if (nvmlInitialized)
            {
                auto nvmlGPUs = getNVMLGPUInfo();
                if (!nvmlGPUs.empty())
                {
                    ServerLogger::logDebug("Using NVML monitoring for %zu NVIDIA GPU(s)", nvmlGPUs.size());
                    gpus.insert(gpus.end(), nvmlGPUs.begin(), nvmlGPUs.end());
                }
            }
#endif

            // Use comprehensive detection for all GPU types (NVIDIA, AMD, Intel, Integrated)
            auto detectedGPUs = detectAllGPUs();
            if (!detectedGPUs.empty())
            {
                ServerLogger::logDebug("Detected %zu GPU(s) via comprehensive detection", detectedGPUs.size());

                // If we already have NVML/enhanced data, merge with detected data
                if (!gpus.empty())
                {
                    mergeGPUData(gpus, detectedGPUs);
                }
                else
                {
                    gpus = detectedGPUs;
                }
            }

            // If no GPUs detected at all, log a message
            if (gpus.empty())
            {
                ServerLogger::logInfo("No GPUs detected on this system");
            }
            else
            {
                ServerLogger::logInfo("Total GPUs detected: %zu", gpus.size());
                for (const auto &gpu : gpus)
                {
                    ServerLogger::logInfo("  GPU %d: %s (VRAM: %s)",
                                          gpu.id, gpu.name.c_str(),
                                          formatBytes(gpu.totalMemory).c_str());
                }
            }

            return gpus;
        }

#if NVML_SUPPORT
        std::vector<GPUInfo> getNVMLGPUInfo()
        {
            std::vector<GPUInfo> gpus;

            unsigned int deviceCount;
            nvmlReturn_t result = nvmlDeviceGetCount(&deviceCount);
            if (result != NVML_SUCCESS)
            {
                ServerLogger::logError("Failed to get NVIDIA device count: %s", nvmlErrorString(result));
                return gpus;
            }

            for (unsigned int i = 0; i < deviceCount; ++i)
            {
                nvmlDevice_t device;
                result = nvmlDeviceGetHandleByIndex(i, &device);
                if (result != NVML_SUCCESS)
                {
                    ServerLogger::logError("Failed to get NVIDIA device %u handle: %s", i, nvmlErrorString(result));
                    continue;
                }

                GPUInfo gpu;
                gpu.id = i; // Get GPU name
                char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
                if (result == NVML_SUCCESS)
                {
                    gpu.name = std::string(name);
                }
                else
                {
                    gpu.name = "Unknown NVIDIA GPU";
                }

                // NVML is NVIDIA-specific
                gpu.vendor = "NVIDIA";

                // Get GPU utilization
                nvmlUtilization_t utilization;
                result = nvmlDeviceGetUtilizationRates(device, &utilization);
                if (result == NVML_SUCCESS)
                {
                    gpu.utilization = static_cast<double>(utilization.gpu);
                }
                else
                {
                    gpu.utilization = -1.0;
                }

                // Get memory information
                nvmlMemory_t memInfo;
                result = nvmlDeviceGetMemoryInfo(device, &memInfo);
                if (result == NVML_SUCCESS)
                {
                    gpu.totalMemory = memInfo.total;
                    gpu.usedMemory = memInfo.used;
                    gpu.freeMemory = memInfo.free;
                    gpu.memoryUtilization = (static_cast<double>(memInfo.used) / static_cast<double>(memInfo.total)) * 100.0;
                }
                else
                {
                    gpu.totalMemory = 0;
                    gpu.usedMemory = 0;
                    gpu.freeMemory = 0;
                    gpu.memoryUtilization = -1.0;
                }

                // Get temperature
                unsigned int temp;
                result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp);
                if (result == NVML_SUCCESS)
                {
                    gpu.temperature = static_cast<double>(temp);
                }
                else
                {
                    gpu.temperature = -1.0;
                }

                // Get power usage
                unsigned int power;
                result = nvmlDeviceGetPowerUsage(device, &power);
                if (result == NVML_SUCCESS)
                {
                    gpu.powerUsage = static_cast<double>(power) / 1000.0; // Convert from milliwatts to watts
                }
                else
                {
                    gpu.powerUsage = -1.0;
                }

                // Get driver version (only once, as it's system-wide)
                if (i == 0)
                {
                    char version[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];
                    result = nvmlSystemGetDriverVersion(version, NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE);
                    if (result == NVML_SUCCESS)
                    {
                        gpu.driverVersion = std::string(version);
                    }
                    else
                    {
                        gpu.driverVersion = "Unknown";
                    }
                }
                else
                {
                    gpu.driverVersion = ""; // Only set for first GPU to avoid redundancy
                }

                gpus.push_back(gpu);
            }

            return gpus;
        }
#endif

        void mergeGPUData(std::vector<GPUInfo> &primaryGPUs, const std::vector<GPUInfo> &secondaryGPUs)
        {
            // Merge data from two GPU detection methods
            // Primary data (NVML/enhanced) takes precedence for metrics
            // Secondary data (comprehensive) provides fallback and additional GPUs

            for (const auto &secondaryGPU : secondaryGPUs)
            {
                bool found = false;
                for (auto &primaryGPU : primaryGPUs)
                {
                    // Match by name similarity
                    if (primaryGPU.name.find(secondaryGPU.name.substr(0, 10)) != std::string::npos ||
                        secondaryGPU.name.find(primaryGPU.name.substr(0, 10)) != std::string::npos)
                    {

                        // Enhance primary GPU with secondary data if missing
                        if (primaryGPU.driverVersion.empty() || primaryGPU.driverVersion == "Unknown")
                        {
                            primaryGPU.driverVersion = secondaryGPU.driverVersion;
                        }

                        if (primaryGPU.totalMemory == 0 && secondaryGPU.totalMemory > 0)
                        {
                            primaryGPU.totalMemory = secondaryGPU.totalMemory;
                            primaryGPU.freeMemory = secondaryGPU.freeMemory;
                        }

                        found = true;
                        break;
                    }
                }

                // If secondary GPU wasn't found in primary list, add it
                if (!found)
                {
                    GPUInfo newGPU = secondaryGPU;
                    newGPU.id = primaryGPUs.size(); // Assign new ID
                    primaryGPUs.push_back(newGPU);
                }
            }
        }

        bool enableEnhancedGPUMonitoring()
        {
            if (enhancedGPUMonitor && enhancedGPUMonitor->isAvailable())
            {
                enhancedGPUMonitor->startMonitoring(1000); // 1 second interval
                useEnhancedGPUMonitoring = true;
                ServerLogger::logInfo("Enhanced GPU monitoring started");
                return true;
            }
            return false;
        }

        bool isEnhancedGPUMonitoringActive() const
        {
            return useEnhancedGPUMonitoring && enhancedGPUMonitor && enhancedGPUMonitor->isMonitoring();
        }
    };

    SystemMonitor::SystemMonitor() : pImpl(std::make_unique<Impl>()) {}

    SystemMonitor::~SystemMonitor() = default;

    SystemMetrics SystemMonitor::getSystemMetrics()
    {
        SystemMetrics metrics;

        // Get CPU usage
        metrics.cpuUsage = pImpl->getCPUUsage();

        // Get RAM information
        pImpl->getRAMInfo(metrics.totalRAM, metrics.usedRAM, metrics.freeRAM);
        if (metrics.totalRAM > 0)
        {
            metrics.ramUtilization = (static_cast<double>(metrics.usedRAM) / static_cast<double>(metrics.totalRAM)) * 100.0;
        }
        else
        {
            metrics.ramUtilization = -1.0;
        }

        // Get GPU information
        metrics.gpus = pImpl->getGPUInfo();

        // Get timestamp
        metrics.timestamp = getCurrentTimestamp();

        return metrics;
    }

    double SystemMonitor::getCPUUsage()
    {
        return pImpl->getCPUUsage();
    }

    void SystemMonitor::getRAMInfo(size_t &totalRAM, size_t &usedRAM, size_t &freeRAM)
    {
        pImpl->getRAMInfo(totalRAM, usedRAM, freeRAM);
    }

    std::vector<GPUInfo> SystemMonitor::getGPUInfo()
    {
        return pImpl->getGPUInfo();
    }

    bool SystemMonitor::initializeGPUMonitoring()
    {
        return pImpl->initializeGPUMonitoring();
    }

    bool SystemMonitor::isGPUMonitoringAvailable() const
    {
        return pImpl->gpuMonitoringAvailable;
    }

    bool SystemMonitor::enableEnhancedGPUMonitoring()
    {
        return pImpl->enableEnhancedGPUMonitoring();
    }

    bool SystemMonitor::isEnhancedGPUMonitoringActive() const
    {
        return pImpl->isEnhancedGPUMonitoringActive();
    }

    std::string SystemMonitor::formatBytes(size_t bytes)
    {
        const char *units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unitIndex < 4)
        {
            size /= 1024.0;
            unitIndex++;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
        return oss.str();
    }

    std::string SystemMonitor::getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
        return oss.str();
    }

} // namespace kolosal
