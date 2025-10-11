// Multi-GPU loading + simple inference smoke test
// Skips (exit code 0) if fewer than 2 GPU devices are detected or GPU offload not enabled.
// Usage: ./test_multi_gpu <model.gguf>
#include "test_common.h"
#include <vector>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <model.gguf>\n"; return 64; }
    const char *model = argv[1];

#if !(defined(USE_CUDA) || defined(USE_VULKAN)) && !defined(USE_METAL)
    // No multi-GPU capable backend compiled; treat as skipped.
    std::cout << "[TEST][MULTI_GPU] Backend without multi-GPU support compiled; skipping." << std::endl;
    return 0;
#endif

    size_t max_devices = llama_max_devices();
    if (max_devices < 2) {
        std::cout << "[TEST][MULTI_GPU] <2 devices available (" << max_devices << "); skipping." << std::endl;
        return 0; // skip silently
    }

    // Prepare loading parameters with two-way split (50/50) limited to available devices
    LoadingParameters lp; lp.n_ctx = 512; lp.n_keep = 256; lp.n_parallel = 1; lp.n_batch = 128; lp.n_ubatch = 64; lp.n_gpu_layers = 10; // small offload
    lp.split_mode = 1; // layer split
    lp.tensor_split[0] = 0.5f;
    lp.tensor_split[1] = 0.5f;
    if (max_devices > 2) {
        // Normalize to first 2 devices only; remaining implicitly zero (already initialized to 0)
    }

    InferenceEngine engine;
    if (!engine.loadModel(model, lp, /*mainGpuId=*/0)) {
        std::cerr << "[TEST][MULTI_GPU] Failed to load model with multi-GPU params" << std::endl;
        return 65;
    }

    CompletionParameters p; p.prompt = "Hello multi-gpu test"; p.maxNewTokens=16; p.temperature=0.7f; p.topP=0.9f; p.streaming=false;
    int job = engine.submitCompletionsJob(p);
    if (job < 0) { std::cerr << "[TEST][MULTI_GPU] submit failed" << std::endl; return 66; }
    engine.waitForJob(job);
    if (engine.hasJobError(job)) { std::cerr << "[TEST][MULTI_GPU] job error: " << engine.getJobError(job) << std::endl; return 67; }
    auto res = engine.getJobResult(job);
    if (res.tokens.empty()) { std::cerr << "[TEST][MULTI_GPU] no tokens generated" << std::endl; return 68; }
    std::cout << "[TEST][MULTI_GPU] OK tokens=" << res.tokens.size() << " text='" << res.text << "'" << std::endl;
    return 0;
}
