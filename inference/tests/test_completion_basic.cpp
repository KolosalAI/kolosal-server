#include "test_common.h"
#include <cstdlib>

int main(int argc, char **argv) {
    if (argc < 3) { std::cerr << "Usage: " << argv[0] << " <model.gguf> <prompt>\n"; return 64; }
    const char *model = argv[1]; std::string prompt = argv[2];

    InferenceEngine engine;
    if (!load_test_model(engine, model)) { std::cerr << "[TEST] Failed to load model\n"; return 65; }

    CompletionParameters p; p.prompt = prompt; p.maxNewTokens = 32; p.temperature = 0.7f; p.topP = 0.9f; p.streaming = false; p.seqId = 0;
    int job = engine.submitCompletionsJob(p);
    if (job < 0) { std::cerr << "[TEST] submit failed\n"; return 66; }
    engine.waitForJob(job);
    if (engine.hasJobError(job)) { std::cerr << "[TEST] job error: " << engine.getJobError(job) << "\n"; return 67; }
    auto r = engine.getJobResult(job);
    if (r.tokens.empty()) { std::cerr << "[TEST] no tokens generated\n"; return 68; }
    std::cout << "[TEST] OK basic completion tokens=" << r.tokens.size() << " ttft=" << r.ttft << " tps=" << r.tps << "\n";
    return 0;
}
