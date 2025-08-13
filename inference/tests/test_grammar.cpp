#include "test_common.h"
#include <iostream>

// Simple test that a trivial grammar constrains output to digits only (heuristic)
int main(int argc, char **argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <model.gguf>\n"; return 64; }
    const char *model = argv[1];
    InferenceEngine engine; if (!load_test_model(engine, model)) { std::cerr << "[TEST] load fail\n"; return 65; }

    CompletionParameters p; p.prompt = "Output a 4 digit number:"; p.maxNewTokens = 8; p.temperature = 0.2f; p.topP = 0.9f; p.seqId = 0;
    // Minimal GBNF grammar (llama.cpp style): non-terminals without angle brackets
    // Accepts exactly 4 digits
    p.grammar = "root ::= digit digit digit digit\ndigit ::= [0-9]\n";
    int job = engine.submitCompletionsJob(p);
    if (job < 0) { std::cerr << "[TEST] submit failed\n"; return 66; }
    engine.waitForJob(job);
    if (engine.hasJobError(job)) { std::cerr << "[TEST] job error: " << engine.getJobError(job) << "\n"; return 67; }
    auto r = engine.getJobResult(job);
    std::string out = r.text; // may contain leading space/newline depending on model
    // Extract first 4 consecutive digits
    std::string digits;
    for (char c: out) { if (std::isdigit(static_cast<unsigned char>(c))) digits.push_back(c); if (digits.size()==4) break; }
    if (digits.size()!=4) { std::cerr << "[TEST] expected 4 digits in constrained output: got '" << out << "'\n"; return 68; }
    std::cout << "[TEST] OK grammar produced digits=" << digits << "\n";
    return 0;
}
