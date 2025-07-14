#include "kolosal/inference_loader.hpp"
#include "kolosal/logger.hpp"
#include "inference_interface.h"

#include <filesystem>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace kolosal
{

    InferenceLoader::InferenceLoader(const std::string &plugins_dir)
        : plugins_dir_(plugins_dir)
    {
        // Ensure plugins directory exists or default to current directory
        if (plugins_dir_.empty())
        {
            plugins_dir_ = ".";
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

    bool InferenceLoader::scanForEngines()
    {
        available_engines_.clear();

        try
        {
            if (!std::filesystem::exists(plugins_dir_))
            {
                setLastError("Plugins directory does not exist: " + plugins_dir_);
                return false;
            }

            // Look for inference engine libraries
            for (const auto &entry : std::filesystem::directory_iterator(plugins_dir_))
            {
                if (entry.is_regular_file())
                {
                    std::string filename = entry.path().filename().string();

                    // Check if this looks like an inference engine library
                    bool is_engine_lib = false;
                    std::string engine_name;
                    
                    std::string ext_to_check = LIBRARY_EXTENSION;
                    
                    // Check Windows style first
                    if (filename.find("llama-") == 0 && filename.size() > ext_to_check.size() && 
                        filename.substr(filename.size() - ext_to_check.size()) == ext_to_check)
                    {
                        // Windows style: "llama-cpu.dll"
                        is_engine_lib = true;
                        engine_name = filename.substr(0, filename.size() - ext_to_check.size());
                    }
                    // Check Unix style
                    else if (filename.find("libllama-") == 0 && filename.size() > (3 + ext_to_check.size()) && 
                             filename.substr(filename.size() - ext_to_check.size()) == ext_to_check)
                    {
                        // Unix style: "libllama-cpu.so"
                        is_engine_lib = true;
                        engine_name = filename.substr(3, filename.size() - 3 - ext_to_check.size()); // Remove "lib" prefix and extension
                    }
                    
                    if (is_engine_lib)
                    {
                        // Validate engine name format (should be "llama-xxx")
                        if (engine_name.find("llama-") != 0)
                        {
                            continue;
                        }
                        
                        if (engine_name.length() <= 6)
                        {
                            continue;
                        }

                        // Extract descriptive name for display (e.g., "cpu" from "llama-cpu")
                        std::string display_name = engine_name.substr(6); // Remove "llama-"

                        InferenceEngineInfo info;
                        info.name = engine_name; // Use full name as identifier
                        info.version = "1.0.0";  // TODO: Extract from library
                        info.description = "Inference engine for " + display_name + " acceleration";
                        info.library_path = entry.path().string();
                        info.is_loaded = false;

                        available_engines_[engine_name] = info;

                        ServerLogger::logInfo("Found inference engine: %s at %s", engine_name.c_str(), info.library_path.c_str());
                    }
                }
            }

            ServerLogger::logInfo("Scan complete. Found %zu inference engines.", available_engines_.size());
            return !available_engines_.empty();
        }
        catch (const std::exception &e)
        {
            setLastError("Error scanning for engines: " + std::string(e.what()));
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
            setLastError("Engine '" + engine_name + "' is not available. Run scanForEngines() first.");
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
        plugins_dir_ = plugins_dir;
        if (plugins_dir_.empty())
        {
            plugins_dir_ = ".";
        }
    }

    std::string InferenceLoader::getPluginsDirectory() const
    {
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

    std::string InferenceLoader::getLibraryName(const std::string &engine_name) const
    {
#ifdef _WIN32
        return engine_name + LIBRARY_EXTENSION;
#else
        return "lib" + engine_name + LIBRARY_EXTENSION;
#endif
    }

    void InferenceLoader::setLastError(const std::string &error) const
    {
        last_error_ = error;
        ServerLogger::logError("InferenceLoader: %s", error.c_str());
    }

} // namespace kolosal
