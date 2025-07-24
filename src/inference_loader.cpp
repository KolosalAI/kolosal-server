#include "kolosal/inference_loader.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include "inference_interface.h"

#include <filesystem>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace kolosal
{

    InferenceLoader::InferenceLoader(const std::string &plugins_dir)
        : plugins_dir_(plugins_dir)
    {
        // Note: plugins_dir is deprecated and not used for engine discovery
        // Engines are now configured through configureEngines() method
        if (!plugins_dir_.empty())
        {
            ServerLogger::logWarning("InferenceLoader plugins_dir parameter is deprecated. Use configureEngines() instead.");
        }
    }

    InferenceLoader::~InferenceLoader()
    {
        // Unload all loaded engines
        for (auto &[name, engine] : loaded_engines_)
        {
            unloadLibrary(name);
        }
    }

    bool InferenceLoader::configureEngines(const std::vector<InferenceEngineConfig>& engines)
    {
        available_engines_.clear();
        
        try
        {
            for (const auto& engineConfig : engines)
            {
                // Validate engine configuration
                if (engineConfig.name.empty())
                {
                    ServerLogger::logWarning("Skipping engine with empty name");
                    continue;
                }
                
                if (engineConfig.library_path.empty())
                {
                    ServerLogger::logWarning("Skipping engine '%s' with empty library path", engineConfig.name.c_str());
                    continue;
                }
                
                // Check if library file exists
                if (!std::filesystem::exists(engineConfig.library_path))
                {
                    ServerLogger::logWarning("Engine library not found: %s for engine '%s'", 
                                           engineConfig.library_path.c_str(), engineConfig.name.c_str());
                    continue;
                }
                
                // Create InferenceEngineInfo from config
                InferenceEngineInfo info;
                info.name = engineConfig.name;
                info.version = engineConfig.version;
                info.description = engineConfig.description.empty() 
                    ? ("Inference engine: " + engineConfig.name) 
                    : engineConfig.description;
                info.library_path = engineConfig.library_path;
                info.is_loaded = false;
                
                available_engines_[engineConfig.name] = info;
                
                ServerLogger::logInfo("Configured inference engine: %s at %s", 
                                    engineConfig.name.c_str(), engineConfig.library_path.c_str());
                
                // Auto-load if specified in config
                if (engineConfig.load_on_startup)
                {
                    if (loadEngine(engineConfig.name))
                    {
                        ServerLogger::logInfo("Auto-loaded inference engine: %s", engineConfig.name.c_str());
                    }
                    else
                    {
                        ServerLogger::logWarning("Failed to auto-load inference engine: %s", engineConfig.name.c_str());
                    }
                }
            }
            
            ServerLogger::logInfo("Engine configuration complete. Configured %zu inference engines.", available_engines_.size());
            return !available_engines_.empty();
        }
        catch (const std::exception &e)
        {
            setLastError("Error configuring engines: " + std::string(e.what()));
            return false;
        }
    }

    std::vector<InferenceEngineInfo> InferenceLoader::getAvailableEngines() const
    {
        std::vector<InferenceEngineInfo> engines;
        engines.reserve(available_engines_.size());

        for (const auto &[name, info] : available_engines_)
        {
            engines.push_back(info);
        }

        return engines;
    }

    bool InferenceLoader::loadEngine(const std::string &engine_name)
    {
        // Check if already loaded
        if (isEngineLoaded(engine_name))
        {
            setLastError("Engine '" + engine_name + "' is already loaded");
            return true; // Not an error
        }

        // Check if engine is available
        auto it = available_engines_.find(engine_name);
        if (it == available_engines_.end())
        {
            setLastError("Engine '" + engine_name + "' is not available. Configure engines using configureEngines() first.");
            return false;
        }

        return loadLibrary(it->second.library_path, engine_name);
    }

    bool InferenceLoader::unloadEngine(const std::string &engine_name)
    {
        if (!isEngineLoaded(engine_name))
        {
            setLastError("Engine '" + engine_name + "' is not loaded");
            return false;
        }

        unloadLibrary(engine_name);

        // Update available engines info
        auto it = available_engines_.find(engine_name);
        if (it != available_engines_.end())
        {
            it->second.is_loaded = false;
        }

        return true;
    }

    bool InferenceLoader::isEngineLoaded(const std::string &engine_name) const
    {
        return loaded_engines_.find(engine_name) != loaded_engines_.end();
    }

    std::unique_ptr<IInferenceEngine, std::function<void(IInferenceEngine *)>>
    InferenceLoader::createEngineInstance(const std::string &engine_name)
    {
        auto it = loaded_engines_.find(engine_name);
        if (it == loaded_engines_.end())
        {
            setLastError("Engine '" + engine_name + "' is not loaded");
            return nullptr;
        }

        const LoadedEngine &engine = it->second;

        // Create the instance using the factory function with safety handling
        IInferenceEngine *instance = nullptr;
        try
        {
            instance = engine.createFunc();
        }
        catch (const std::exception &e)
        {
            setLastError("Exception during engine instance creation for '" + engine_name + "': " + std::string(e.what()));
            return nullptr;
        }
        catch (...)
        {
            setLastError("Unknown exception during engine instance creation for '" + engine_name + "'");
            return nullptr;
        }

        if (!instance)
        {
            setLastError("Failed to create instance of engine '" + engine_name + "'");
            return nullptr;
        }

        // Return a unique_ptr with custom deleter that also has safety handling
        return std::unique_ptr<IInferenceEngine, std::function<void(IInferenceEngine *)>>(
            instance,
            [destroyer = engine.destroyFunc, engine_name](IInferenceEngine *ptr)
            {
                if (ptr && destroyer)
                {
                    try
                    {
                        destroyer(ptr);
                    }
                    catch (const std::exception &e)
                    {
                        ServerLogger::logError("Exception during engine destruction for '%s': %s", engine_name.c_str(), e.what());
                    }
                    catch (...)
                    {
                        ServerLogger::logError("Unknown exception during engine destruction for '%s'", engine_name.c_str());
                    }
                }
            });
    }

    std::string InferenceLoader::getLastError() const
    {
        return last_error_;
    }

    void InferenceLoader::setPluginsDirectory(const std::string &plugins_dir)
    {
        ServerLogger::logWarning("setPluginsDirectory() is deprecated. Use configureEngines() instead.");
        plugins_dir_ = plugins_dir;
    }

    std::string InferenceLoader::getPluginsDirectory() const
    {
        ServerLogger::logWarning("getPluginsDirectory() is deprecated. Use configureEngines() instead.");
        return plugins_dir_;
    }

    bool InferenceLoader::loadLibrary(const std::string &library_path, const std::string &engine_name)
    {
        LIBRARY_HANDLE handle = LOAD_LIBRARY(library_path.c_str());
        if (!handle)
        {
            std::string error = "Failed to load library: " + library_path;

#ifdef _WIN32
            DWORD errorCode = GetLastError();
            LPSTR messageBuffer = nullptr;
            size_t size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&messageBuffer, 0, nullptr);

            if (messageBuffer)
            {
                error += " (Error: " + std::string(messageBuffer) + ")";
                LocalFree(messageBuffer);
            }
#else
            const char *dlError = dlerror();
            if (dlError)
            {
                error += " (Error: " + std::string(dlError) + ")";
            }
#endif

            setLastError(error);
            return false;
        }

        // Get the factory functions
        CreateInferenceEngineFunc createFunc =
            reinterpret_cast<CreateInferenceEngineFunc>(GET_FUNCTION(handle, "createInferenceEngine"));
        DestroyInferenceEngineFunc destroyFunc =
            reinterpret_cast<DestroyInferenceEngineFunc>(GET_FUNCTION(handle, "destroyInferenceEngine"));

        if (!createFunc || !destroyFunc)
        {
            CLOSE_LIBRARY(handle);
            setLastError("Library '" + library_path + "' does not export required functions (createInferenceEngine, destroyInferenceEngine)");
            return false;
        }

        // Store the loaded engine
        LoadedEngine loaded_engine;
        loaded_engine.handle = handle;
        loaded_engine.createFunc = createFunc;
        loaded_engine.destroyFunc = destroyFunc;
        loaded_engine.info = available_engines_[engine_name];
        loaded_engine.info.is_loaded = true;

        loaded_engines_[engine_name] = loaded_engine;

        // Update available engines info
        available_engines_[engine_name].is_loaded = true;

        ServerLogger::logInfo("Successfully loaded inference engine: %s", engine_name.c_str());
        return true;
    }

    void InferenceLoader::unloadLibrary(const std::string &engine_name)
    {
        auto it = loaded_engines_.find(engine_name);
        if (it != loaded_engines_.end())
        {
            CLOSE_LIBRARY(it->second.handle);
            loaded_engines_.erase(it);
            ServerLogger::logInfo("Unloaded inference engine: %s", engine_name.c_str());
        }
    }

    void InferenceLoader::setLastError(const std::string &error) const
    {
        last_error_ = error;
        ServerLogger::logError("InferenceLoader: %s", error.c_str());
    }

} // namespace kolosal
