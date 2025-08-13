#pragma once
#include "inference.h"
#include <chrono>
#include <thread>
#include <iostream>

inline bool wait_for_completion(InferenceEngine &engine, int jobId, int timeout_ms = 15000) {
    auto start = std::chrono::steady_clock::now();
    while (!engine.isJobFinished(jobId)) {
        if (engine.hasJobError(jobId)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count() > timeout_ms) {
            std::cerr << "[TEST] Timeout waiting for job " << jobId << std::endl;
            return false;
        }
    }
    return !engine.hasJobError(jobId);
}

inline bool load_test_model(InferenceEngine &engine, const char *modelPath, int n_parallel = 1) {
    LoadingParameters lp; lp.n_ctx = 1024; lp.n_keep = 512; lp.n_parallel = n_parallel; lp.n_batch = 256; lp.n_ubatch = 64; lp.n_gpu_layers = 0;
    return engine.loadModel(modelPath, lp);
}
