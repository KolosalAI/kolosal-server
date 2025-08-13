#include "test_common.h"
#include <filesystem>

int main(int argc, char **argv) {
    if (argc < 3) { std::cerr << "Usage: " << argv[0] << " <model.gguf> <cache_dir>\n"; return 64; }
    const char *model = argv[1]; std::string cacheDir = argv[2];
    std::filesystem::create_directories(cacheDir);

    InferenceEngine engine; if (!load_test_model(engine, model)) { std::cerr << "[TEST] load fail\n"; return 65; }

    std::string cacheFile = cacheDir + "/sess1.bin";

    // First request builds cache
    CompletionParameters p1; p1.prompt = "The quick brown fox"; p1.maxNewTokens = 16; p1.seqId = 0; p1.kvCacheFilePath = cacheFile; p1.streaming=false; p1.temperature=0.0f; p1.topP=1.0f;
    int j1 = engine.submitCompletionsJob(p1); engine.waitForJob(j1);
    if (engine.hasJobError(j1)) { std::cerr << "[TEST] first job error: " << engine.getJobError(j1) << "\n"; return 66; }
    auto r1 = engine.getJobResult(j1);

    // Second request extends same prefix - should be faster TTFT (heuristic) or at least succeed
    CompletionParameters p2 = p1; p2.prompt = "The quick brown fox jumps over"; p2.maxNewTokens = 16;
    auto t0 = std::chrono::steady_clock::now();
    int j2 = engine.submitCompletionsJob(p2); engine.waitForJob(j2);
    auto t1 = std::chrono::steady_clock::now();
    if (engine.hasJobError(j2)) { std::cerr << "[TEST] second job error: " << engine.getJobError(j2) << "\n"; return 67; }
    auto r2 = engine.getJobResult(j2);

    if (r2.tokens.empty()) { std::cerr << "[TEST] second job produced no tokens\n"; return 68; }
    std::cout << "[TEST] OK session cache tokens1=" << r1.tokens.size() << " tokens2=" << r2.tokens.size() << " elapsed2_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count() << "\n";
    return 0;
}
