#include "inference.h"
#include <iostream>
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <model.gguf> <prompt> [max_tokens]\n";
        return 1;
    }

    const char* modelPath = argv[1];
    std::string prompt = argv[2];
    int maxTokens = 32;
    if (argc > 3) {
        try { maxTokens = std::stoi(argv[3]); } catch (...) {}
    }

    InferenceEngine engine;

    LoadingParameters loadParams; // default values
    loadParams.n_ctx = 2048; // keep fairly small for quick test
    loadParams.n_keep = 1024;
    loadParams.n_gpu_layers = 0; // rely on CPU unless backend compiled otherwise
    loadParams.n_batch = 256;
    loadParams.n_ubatch = 64;

    std::cout << "[TEST] Loading model: " << modelPath << std::endl;
    auto t0 = std::chrono::steady_clock::now();
    if (!engine.loadModel(modelPath, loadParams)) {
        std::cerr << "[TEST][ERROR] Failed to load model" << std::endl;
        return 2;
    }
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "[TEST] Model loaded in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
              << " ms" << std::endl;

    CompletionParameters params;
    params.prompt = prompt;
    params.maxNewTokens = maxTokens;
    params.temperature = 0.7f;
    params.topP = 0.9f;
    params.streaming = true; // we will poll
    params.seqId = 1; // arbitrary sequence id

    std::cout << "[TEST] Submitting completion job..." << std::endl;
    int jobId = engine.submitCompletionsJob(params);
    if (jobId < 0) {
        std::cerr << "[TEST][ERROR] Failed to submit job" << std::endl;
        return 3;
    }

    std::string lastText;
    while (!engine.isJobFinished(jobId)) {
        if (engine.hasJobError(jobId)) {
            std::cerr << "[TEST][ERROR] Job error: " << engine.getJobError(jobId) << std::endl;
            return 4;
        }
        auto result = engine.getJobResult(jobId);
        if (result.text.size() > lastText.size()) {
            std::cout << result.text.substr(lastText.size());
            std::cout.flush();
            lastText = result.text;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (engine.hasJobError(jobId)) {
        std::cerr << "\n[TEST][ERROR] Final job error: " << engine.getJobError(jobId) << std::endl;
        return 5;
    }

    auto finalResult = engine.getJobResult(jobId);
    std::cout << "\n[TEST] Generation complete." << std::endl;
    std::cout << "[TEST] Tokens: " << finalResult.tokens.size() << std::endl;
    std::cout << "[TEST] TPS: " << finalResult.tps << std::endl;
    std::cout << "[TEST] TTFT (ms): " << finalResult.ttft << std::endl;

    return 0;
}
