#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <json.hpp>

/**
 * @brief Model for add engine response
 * 
 * This model represents the JSON response body for a successful engine addition.
 */
class KOLOSAL_SERVER_API AddEngineResponse : public IModel {
public:
    std::string engine_id;
    std::string model_path;
    std::string status;  // "loaded" or "created"
    bool load_immediately;
    int main_gpu_id;
    std::string message;
    
    // Loading parameters (nested object)
    struct LoadingParametersModel {
        int n_ctx;
        int n_keep;
        bool use_mlock;
        bool use_mmap;
        bool cont_batching;
        bool warmup;
        int n_parallel;
        int n_gpu_layers;
        int n_batch;
        int n_ubatch;
        
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
    } loading_parameters;

    bool validate() const override {
        // Basic validation for response
        return !engine_id.empty() && !model_path.empty() && !status.empty();
    }

    void from_json(const nlohmann::json& j) override {
        // This would be used if parsing a response JSON
        if (j.contains("engine_id")) j.at("engine_id").get_to(engine_id);
        if (j.contains("model_path")) j.at("model_path").get_to(model_path);
        if (j.contains("status")) j.at("status").get_to(status);
        if (j.contains("load_immediately")) j.at("load_immediately").get_to(load_immediately);
        if (j.contains("main_gpu_id")) j.at("main_gpu_id").get_to(main_gpu_id);
        if (j.contains("message")) j.at("message").get_to(message);
        
        if (j.contains("loading_parameters")) {
            auto lp = j["loading_parameters"];
            loading_parameters.n_ctx = lp.value("n_ctx", 4096);
            loading_parameters.n_keep = lp.value("n_keep", 2048);
            loading_parameters.use_mlock = lp.value("use_mlock", true);
            loading_parameters.use_mmap = lp.value("use_mmap", true);
            loading_parameters.cont_batching = lp.value("cont_batching", true);
            loading_parameters.warmup = lp.value("warmup", false);
            loading_parameters.n_parallel = lp.value("n_parallel", 1);
            loading_parameters.n_gpu_layers = lp.value("n_gpu_layers", 100);
            loading_parameters.n_batch = lp.value("n_batch", 2048);
            loading_parameters.n_ubatch = lp.value("n_ubatch", 512);
        }
    }

    nlohmann::json to_json() const override {
        nlohmann::json j = {
            {"engine_id", engine_id},
            {"model_path", model_path},
            {"status", status},
            {"load_immediately", load_immediately},
            {"main_gpu_id", main_gpu_id},
            {"loading_parameters", loading_parameters.to_json()},
            {"message", message}
        };

        return j;
    }
};
