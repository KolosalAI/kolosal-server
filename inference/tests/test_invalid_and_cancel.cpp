#include "test_common.h"

int main(int argc, char **argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <model.gguf>\n"; return 64; }
    const char *model = argv[1];
    InferenceEngine engine; if (!load_test_model(engine, model)) { std::cerr << "[TEST] load fail\n"; return 65; }

    // Invalid: empty prompt
    CompletionParameters bad; bad.prompt=""; bad.maxNewTokens=8; int badId = engine.submitCompletionsJob(bad);
    if (badId >= 0) { engine.waitForJob(badId); if (!engine.hasJobError(badId)) { std::cerr << "[TEST] expected error for empty prompt\n"; return 66; } }

    // Valid but we'll cancel early
    CompletionParameters p; p.prompt="This will be cancelled"; p.maxNewTokens=200; p.streaming=true; p.seqId=0; int job = engine.submitCompletionsJob(p);
    if (job < 0) { std::cerr << "[TEST] submit fail\n"; return 67; }
    // Let a bit of generation happen
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    engine.stopJob(job);
    engine.waitForJob(job);
    if (!engine.hasJobError(job)) {
        auto r = engine.getJobResult(job);
        if (r.tokens.size() >= (size_t)p.maxNewTokens) { std::cerr << "[TEST] cancellation ineffective\n"; return 68; }
    }
    std::cout << "[TEST] OK invalid+cancel scenarios" << std::endl;
    return 0;
}
