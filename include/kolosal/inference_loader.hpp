#ifndef KOLOSAL_INFERENCE_LOADER_HPP
#define KOLOSAL_INFERENCE_LOADER_HPP

/**
 * @file inference_loader.hpp
 * @brief Dynamic loader for inference engine plugins
 * 
 * This header provides functionality to dynamically load inference engine
 * plugins at runtime. It abstracts platform-specific dynamic library loading
 * and provides a unified interface for loading different inference engines
 * (CPU, CUDA, Vulkan, etc.).
 * 
 * @author Kolosal AI Team
 * @version 1.0
 * @date 2025
 */

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>

// Forward declarations
class IInferenceEngine;
namespace kolosal { struct InferenceEngineConfig; }

#ifdef _WIN32
    #include <windows.h>
    #define LIBRARY_HANDLE HMODULE
    #define LOAD_LIBRARY(path) LoadLibraryA(path)
    #define GET_FUNCTION(handle, name) GetProcAddress(handle, name)
    #define CLOSE_LIBRARY(handle) FreeLibrary(handle)
    #define LIBRARY_EXTENSION ".dll"
#elif defined(__APPLE__)
    #include <dlfcn.h>
    #define LIBRARY_HANDLE void*
    #define LOAD_LIBRARY(path) dlopen(path, RTLD_LAZY)
    #define GET_FUNCTION(handle, name) dlsym(handle, name)
    #define CLOSE_LIBRARY(handle) dlclose(handle)
    #define LIBRARY_EXTENSION ".dylib"
#else
    #include <dlfcn.h>
    #define LIBRARY_HANDLE void*
    #define LOAD_LIBRARY(path) dlopen(path, RTLD_LAZY)
    #define GET_FUNCTION(handle, name) dlsym(handle, name)
    #define CLOSE_LIBRARY(handle) dlclose(handle)
    #define LIBRARY_EXTENSION ".so"
#endif

// Forward declaration of the inference interface
class IInferenceEngine;

namespace kolosal {

/**
 * @brief Factory function type for creating inference engine instances
 */
typedef IInferenceEngine* (*CreateInferenceEngineFunc)();

/**
 * @brief Factory function type for destroying inference engine instances
 */
typedef void (*DestroyInferenceEngineFunc)(IInferenceEngine*);

/**
 * @brief Information about an available inference engine plugin
 */
struct InferenceEngineInfo {
    std::string name;           ///< Engine name (e.g., "cpu", "cuda", "vulkan")
    std::string version;        ///< Engine version
    std::string description;    ///< Human-readable description
    std::string library_path;   ///< Path to the shared library
    bool is_loaded;            ///< Whether the engine is currently loaded
};

/**
 * @brief Dynamic loader for inference engine plugins
 * 
 * This class manages the loading and unloading of inference engine plugins
 * at runtime. It provides functionality to:
 * - Configure available inference engines from config
 * - Load/unload engines dynamically
 * - Create engine instances
 * - Manage engine lifecycle
 */
class InferenceLoader {
public:
    /**
     * @brief Constructor
     * @param plugins_dir Directory for legacy plugin discovery (deprecated, not used)
     */
    explicit InferenceLoader(const std::string& plugins_dir = "");

    /**
     * @brief Destructor - automatically unloads all loaded engines
     */
    ~InferenceLoader();

    // Non-copyable but movable
    InferenceLoader(const InferenceLoader&) = delete;
    InferenceLoader& operator=(const InferenceLoader&) = delete;
    InferenceLoader(InferenceLoader&&) = default;
    InferenceLoader& operator=(InferenceLoader&&) = default;

    /**
     * @brief Configure available inference engines from config
     * @param engines Vector of inference engine configurations
     * @return True if engines were configured successfully
     */
    bool configureEngines(const std::vector<InferenceEngineConfig>& engines);

    /**
     * @brief Get list of available inference engines
     * @return Vector of engine information structures
     */
    std::vector<InferenceEngineInfo> getAvailableEngines() const;

    /**
     * @brief Load a specific inference engine
     * @param engine_name Name of the engine to load (e.g., "cpu", "cuda")
     * @return True if the engine was loaded successfully
     */
    bool loadEngine(const std::string& engine_name);

    /**
     * @brief Unload a specific inference engine
     * @param engine_name Name of the engine to unload
     * @return True if the engine was unloaded successfully
     */
    bool unloadEngine(const std::string& engine_name);

    /**
     * @brief Check if an engine is currently loaded
     * @param engine_name Name of the engine to check
     * @return True if the engine is loaded
     */
    bool isEngineLoaded(const std::string& engine_name) const;

    /**
     * @brief Create an instance of a loaded inference engine
     * @param engine_name Name of the engine to create an instance of
     * @return Pointer to the inference engine instance, or nullptr on failure
     */
    std::unique_ptr<IInferenceEngine, std::function<void(IInferenceEngine*)>>
    createEngineInstance(const std::string& engine_name);

    /**
     * @brief Get the last error message
     * @return Error message string
     */
    std::string getLastError() const;

    /**
     * @brief Set the plugins directory (DEPRECATED)
     * @deprecated This method is no longer used. Use configureEngines() instead.
     * @param plugins_dir New plugins directory path
     */
    [[deprecated("Use configureEngines() instead")]]
    void setPluginsDirectory(const std::string& plugins_dir);

    /**
     * @brief Get the current plugins directory (DEPRECATED)
     * @deprecated This method is no longer used. Use configureEngines() instead.
     * @return Current plugins directory path
     */
    [[deprecated("Use configureEngines() instead")]]
    std::string getPluginsDirectory() const;

private:
    /**
     * @brief Information about a loaded engine
     */
    struct LoadedEngine {
        LIBRARY_HANDLE handle;
        CreateInferenceEngineFunc createFunc;
        DestroyInferenceEngineFunc destroyFunc;
        InferenceEngineInfo info;
    };

    std::string plugins_dir_;                                   ///< Directory to search for plugins
    std::map<std::string, InferenceEngineInfo> available_engines_; ///< Available engines
    std::map<std::string, LoadedEngine> loaded_engines_;       ///< Currently loaded engines
    mutable std::string last_error_;                           ///< Last error message

    /**
     * @brief Load a library and extract engine functions
     * @param library_path Path to the shared library
     * @param engine_name Name of the engine
     * @return True if the library was loaded successfully
     */
    bool loadLibrary(const std::string& library_path, const std::string& engine_name);

    /**
     * @brief Unload a library
     * @param engine_name Name of the engine to unload
     */
    void unloadLibrary(const std::string& engine_name);

    /**
     * @brief Set the last error message
     * @param error Error message
     */
    void setLastError(const std::string& error) const;
};

} // namespace kolosal

#endif // KOLOSAL_INFERENCE_LOADER_HPP
