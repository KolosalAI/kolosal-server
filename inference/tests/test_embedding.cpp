#include "test_common.h"

int main(int argc, char **argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <embedding-model.gguf>\n"; return 64; }
    const char *model = argv[1];
    InferenceEngine engine; if (!load_test_model(engine, model)) { std::cerr << "[TEST] load fail\n"; return 65; }

    EmbeddingParameters ep; ep.input = "The quick brown fox jumps over the lazy dog"; ep.seqId = 0; ep.kvCacheFilePath="";
    int job = engine.submitEmbeddingJob(ep);
    if (job < 0) { std::cerr << "[TEST] submit embedding fail\n"; return 66; }
    engine.waitForJob(job);
    if (engine.hasJobError(job)) { std::cerr << "[TEST] embedding job error: " << engine.getJobError(job) << "\n"; return 67; }
    auto res = engine.getEmbeddingResult(job);
    if (res.embedding.empty()) { std::cerr << "[TEST] empty embedding\n"; return 68; }
    std::cout << "[TEST] OK embedding size=" << res.embedding.size() << "\n"; return 0;
}
