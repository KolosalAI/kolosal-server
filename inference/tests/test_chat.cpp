#include "test_common.h"

int main(int argc, char **argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <model.gguf>\n"; return 64; }
    const char *model = argv[1];
    InferenceEngine engine; if (!load_test_model(engine, model)) { std::cerr << "[TEST] load fail\n"; return 65; }

    ChatCompletionParameters chat; chat.maxNewTokens = 48; chat.temperature = 0.8f; chat.topP = 0.95f; chat.streaming = true; chat.seqId = 0;
    chat.messages.push_back({"system","You are a concise assistant."});
    chat.messages.push_back({"user","List three prime numbers greater than 50."});

    int job = engine.submitChatCompletionsJob(chat);
    if (job < 0) { std::cerr << "[TEST] submit failed\n"; return 66; }
    if (!wait_for_completion(engine, job)) { std::cerr << "[TEST] job failed: " << engine.getJobError(job) << "\n"; return 67; }
    auto r = engine.getJobResult(job);
    if (r.text.find("53")==std::string::npos) {
        std::cerr << "[TEST] expected prime not found in response: " << r.text << "\n"; // heuristic
        return 68;
    }
    std::cout << "[TEST] OK chat completion length=" << r.text.size() << "\n";
    return 0;
}
