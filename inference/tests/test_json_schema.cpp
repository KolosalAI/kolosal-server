#include "test_common.h"
#include <iostream>
#include <nlohmann/json.hpp>

// Test JSON schema => grammar integration by requesting a simple structured object
int main(int argc, char **argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <model.gguf>\n"; return 64; }
    const char *model = argv[1];
    InferenceEngine engine; if (!load_test_model(engine, model)) { std::cerr << "[TEST] load fail\n"; return 65; }

    nlohmann::json schema = {
        {"type","object"},
        {"properties",{
            {"name", {{"type","string"}}},
            {"age", {{"type","integer"}}}
        }},
        {"required", {"name","age"}}
    };

    CompletionParameters p; p.prompt = "Generate a JSON object with a person's name and age:"; p.maxNewTokens = 64; p.temperature = 0.3f; p.topP = 0.95f; p.seqId = 0; p.jsonSchema = schema.dump();
    int job = engine.submitCompletionsJob(p);
    if (job < 0) { std::cerr << "[TEST] submit failed\n"; return 66; }
    engine.waitForJob(job);
    if (engine.hasJobError(job)) { std::cerr << "[TEST] job error: " << engine.getJobError(job) << "\n"; return 67; }
    auto r = engine.getJobResult(job);
    // Heuristic validation: must contain keys and braces
    if (r.text.find("name") == std::string::npos || r.text.find("age") == std::string::npos || r.text.find("{") == std::string::npos) {
        std::cerr << "[TEST] structured JSON not detected in output: " << r.text << "\n"; return 68; }
    std::cout << "[TEST] OK json schema output: " << r.text << "\n";
    return 0;
}
