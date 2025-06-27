#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <optional>
#include <json.hpp>

/**
 * @brief Model for add engine request parameters
 * 
 * This model represents the JSON request body for adding a new engine to the server.
 * It includes validation for all required and optional parameters.
 */
class KOLOSAL_SERVER_API AddEngineRequest : public IModel {
public:
    // Required fields
    std::string engine_id;
    std::string model_path;
    
    // Optional fields
    bool load_immediately = true;      // Whether to load immediately after adding (vs register for lazy loading)
    int main_gpu_id = 0;
    
    // Loading parameters (nested object)
    struct LoadingParametersModel {
        int n_ctx = 4096;
        int n_keep = 2048;
        bool use_mlock = true;
        bool use_mmap = true;
        bool cont_batching = true;
        bool warmup = false;
        int n_parallel = 1;
        int n_gpu_layers = 100;
        int n_batch = 2048;
        int n_ubatch = 512;
        
        nlohmann::json to_json() const {
            return nlohmann::json {
                {"n_ctx", n_ctx},
                {"n_keep", n_keep},
                {"use_mlock", use_mlock},
                {"use_mmap", use_mmap},
                {"cont_batching", cont_batching},
                {"warmup", warmup},
                {"n_parallel", n_parallel},
                {"n_gpu_layers", n_gpu_layers},
                {"n_batch", n_batch},
                {"n_ubatch", n_ubatch}
            };
        }
        
        void from_json(const nlohmann::json& j) {
            if (j.contains("n_ctx") && !j["n_ctx"].is_null()) {
                if (!j["n_ctx"].is_number_integer()) {
                    throw std::runtime_error("n_ctx must be an integer");
                }
                n_ctx = j["n_ctx"].get<int>();
            }
            
            if (j.contains("n_keep") && !j["n_keep"].is_null()) {
                if (!j["n_keep"].is_number_integer()) {
                    throw std::runtime_error("n_keep must be an integer");
                }
                n_keep = j["n_keep"].get<int>();
            }
            
            if (j.contains("use_mlock") && !j["use_mlock"].is_null()) {
                if (!j["use_mlock"].is_boolean()) {
                    throw std::runtime_error("use_mlock must be a boolean");
                }
                use_mlock = j["use_mlock"].get<bool>();
            }
            
            if (j.contains("use_mmap") && !j["use_mmap"].is_null()) {
                if (!j["use_mmap"].is_boolean()) {
                    throw std::runtime_error("use_mmap must be a boolean");
                }
                use_mmap = j["use_mmap"].get<bool>();
            }
            
            if (j.contains("cont_batching") && !j["cont_batching"].is_null()) {
                if (!j["cont_batching"].is_boolean()) {
                    throw std::runtime_error("cont_batching must be a boolean");
                }
                cont_batching = j["cont_batching"].get<bool>();
            }
            
            if (j.contains("warmup") && !j["warmup"].is_null()) {
                if (!j["warmup"].is_boolean()) {
                    throw std::runtime_error("warmup must be a boolean");
                }
                warmup = j["warmup"].get<bool>();
            }
            
            if (j.contains("n_parallel") && !j["n_parallel"].is_null()) {
                if (!j["n_parallel"].is_number_integer()) {
                    throw std::runtime_error("n_parallel must be an integer");
                }
                n_parallel = j["n_parallel"].get<int>();
            }
            
            if (j.contains("n_gpu_layers") && !j["n_gpu_layers"].is_null()) {
                if (!j["n_gpu_layers"].is_number_integer()) {
                    throw std::runtime_error("n_gpu_layers must be an integer");
                }
                n_gpu_layers = j["n_gpu_layers"].get<int>();
            }
            
            if (j.contains("n_batch") && !j["n_batch"].is_null()) {
                if (!j["n_batch"].is_number_integer()) {
                    throw std::runtime_error("n_batch must be an integer");
                }
                n_batch = j["n_batch"].get<int>();
            }
            
            if (j.contains("n_ubatch") && !j["n_ubatch"].is_null()) {
                if (!j["n_ubatch"].is_number_integer()) {
                    throw std::runtime_error("n_ubatch must be an integer");
                }
                n_ubatch = j["n_ubatch"].get<int>();
            }
        }
    } loading_parameters;

    bool validate() const override {
        if (engine_id.empty()) {
            return false;
        }

        if (model_path.empty()) {
            return false;
        }

        // Validate loading parameters
        if (loading_parameters.n_ctx <= 0 || loading_parameters.n_ctx > 1000000) {
            return false;
        }

        if (loading_parameters.n_keep < 0 || loading_parameters.n_keep > loading_parameters.n_ctx) {
            return false;
        }

        if (loading_parameters.n_batch <= 0 || loading_parameters.n_batch > 8192) {
            return false;
        }

        if (loading_parameters.n_ubatch <= 0 || loading_parameters.n_ubatch > loading_parameters.n_batch) {
            return false;
        }

        if (loading_parameters.n_parallel <= 0 || loading_parameters.n_parallel > 16) {
            return false;
        }

        if (loading_parameters.n_gpu_layers < 0 || loading_parameters.n_gpu_layers > 1000) {
            return false;
        }

        if (main_gpu_id < -1 || main_gpu_id > 15) {
            return false;
        }

        return true;
    }

    void from_json(const nlohmann::json& j) override {
        // Add basic validation to ensure we have a valid JSON object
        if (!j.is_object()) {
            throw std::runtime_error("Request must be a JSON object");
        }

        // Check for required fields
        if (!j.contains("engine_id") || !j.contains("model_path")) {
            throw std::runtime_error("Missing required fields: engine_id and model_path are required");
        }

        if (!j["engine_id"].is_string()) {
            throw std::runtime_error("engine_id must be a string");
        }
        j.at("engine_id").get_to(engine_id);

        if (!j["model_path"].is_string()) {
            throw std::runtime_error("model_path must be a string");
        }
        j.at("model_path").get_to(model_path);

        // Optional parameters with improved error handling
        if (j.contains("load_immediately") && !j["load_immediately"].is_null()) {
            if (!j["load_immediately"].is_boolean()) {
                throw std::runtime_error("load_immediately must be a boolean");
            }
            j.at("load_immediately").get_to(load_immediately);
        }

        if (j.contains("main_gpu_id") && !j["main_gpu_id"].is_null()) {
            if (!j["main_gpu_id"].is_number_integer()) {
                throw std::runtime_error("main_gpu_id must be an integer");
            }
            j.at("main_gpu_id").get_to(main_gpu_id);
        }

        // Parse loading parameters if present
        if (j.contains("loading_parameters") && !j["loading_parameters"].is_null()) {
            if (!j["loading_parameters"].is_object()) {
                throw std::runtime_error("loading_parameters must be an object");
            }
            loading_parameters.from_json(j["loading_parameters"]);
        }
    }

    nlohmann::json to_json() const override {
        nlohmann::json j = {
            {"engine_id", engine_id},
            {"model_path", model_path},
            {"load_immediately", load_immediately},
            {"main_gpu_id", main_gpu_id},
            {"loading_parameters", loading_parameters.to_json()}
        };

        return j;
    }
};
