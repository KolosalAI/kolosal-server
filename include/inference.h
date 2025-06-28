#ifndef INFERENCE_H
#define INFERENCE_H

/**
 * @file inference.h
 * @brief Inference Engine for Large Language Model Processing
 * 
 * This header provides a comprehensive interface for running inference on large language models.
 * The engine supports both text completion and chat completion tasks with various configuration
 * options for sampling, caching, and performance optimization.
 * 
 * @section features Features
 * - Text completion and chat completion support
 * - Configurable sampling parameters (temperature, top-p)
 * - GPU acceleration with configurable layer offloading
 * - Session caching for improved performance
 * - Streaming and non-streaming generation modes
 * - Asynchronous job processing with thread-safe operations
 * - Tool usage support for chat completions
 * 
 * @section usage Basic Usage Example
 * 
 * @subsection text_completion Text Completion
 * @code
 * // Create and configure the inference engine
 * InferenceEngine engine;
 * 
 * // Set up loading parameters
 * LoadingParameters loadParams;
 * loadParams.n_ctx = 4096;           // Context window size
 * loadParams.n_gpu_layers = 32;      // Offload 32 layers to GPU
 * loadParams.n_batch = 512;          // Batch size
 *  * // Load the model
 * if (!engine.loadModel("path/to/model.gguf", loadParams)) {
 *     // Handle error
 *     return -1;
 * }
 * 
 * // Configure completion parameters
 * CompletionParameters params;
 * params.prompt = "The capital of France is";
 * params.maxNewTokens = 50;
 * params.temperature = 0.7f;
 * params.topP = 0.9f;
 * params.streaming = false;
 * 
 * // Submit the job
 * int jobId = engine.submitCompletionsJob(params);
 * 
 * // Wait for completion
 * engine.waitForJob(jobId);
 * 
 * // Get results
 * if (!engine.hasJobError(jobId)) {
 *     CompletionResult result = engine.getJobResult(jobId);
 *     std::cout << "Generated text: " << result.text << std::endl;
 *     std::cout << "Tokens per second: " << result.tps << std::endl;
 * } else {
 *     std::cout << "Error: " << engine.getJobError(jobId) << std::endl;
 * }
 * @endcode
 * 
 * @subsection chat_completion Chat Completion
 * @code * // Create and load model (same as above)
 * InferenceEngine engine;
 * LoadingParameters loadParams;
 * engine.loadModel("path/to/model.gguf", loadParams);
 * 
 * // Set up chat messages
 * ChatCompletionParameters chatParams;
 * chatParams.messages = {
 *     {"system", "You are a helpful assistant."},
 *     {"user", "What is the weather like today?"}
 * };
 * chatParams.maxNewTokens = 100;
 * chatParams.temperature = 0.8f;
 * chatParams.streaming = true;  // Enable streaming for real-time responses
 * 
 * // Submit chat completion job
 * int chatJobId = engine.submitChatCompletionsJob(chatParams);
 * 
 * // For streaming, poll for results
 * while (!engine.isJobFinished(chatJobId)) {
 *     CompletionResult partialResult = engine.getJobResult(chatJobId);
 *     std::cout << partialResult.text;  // Print incremental text
 *     std::this_thread::sleep_for(std::chrono::milliseconds(100));
 * }
 * 
 * // Get final result
 * CompletionResult finalResult = engine.getJobResult(chatJobId);
 * std::cout << std::endl << "Final TPS: " << finalResult.tps << std::endl;
 * @endcode
 * 
 * @subsection session_caching Session Caching
 * @code
 * // Enable session caching for faster subsequent requests
 * CompletionParameters cachedParams;
 * cachedParams.prompt = "Continue this story: Once upon a time";
 * cachedParams.kvCacheFilePath = "cache/session_001.bin";
 * cachedParams.seqId = 1;  // Unique sequence ID
 * 
 * int jobId = engine.submitCompletionsJob(cachedParams);
 * engine.waitForJob(jobId);
 * 
 * // Subsequent requests with the same seqId will reuse cached context
 * @endcode
 * 
 * @section threading Thread Safety
 * The InferenceEngine is designed to be thread-safe. Multiple jobs can be submitted
 * concurrently, and the engine will process them efficiently. Each job has its own
 * synchronization primitives to ensure safe access to job state.
 * 
 * @section performance Performance Considerations
 * - Use GPU acceleration (n_gpu_layers) for better performance
 * - Adjust batch sizes (n_batch, n_ubatch) based on available memory
 * - Enable session caching for repeated similar requests
 * - Use streaming mode for real-time applications
 * - Configure context length (n_ctx) appropriately for your use case
 * 
 * @author Kolosal AI Team
 * @version 1.0
 * @date 2025
 */

#include "inference_interface.h"
#include "llama.h"
#include "common.h"
#include "sampling.h"

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <exception>
#include <condition_variable>

// =============================================================================
// API Export/Import Macros
// =============================================================================
#ifdef KOLOSAL_SERVER_STATIC
    // For static library linking, no import/export needed
    #define INFERENCE_API
#elif defined(_WIN32)
    #ifdef INFERENCE_EXPORTS
        #define INFERENCE_API __declspec(dllexport)
    #else
        #define INFERENCE_API __declspec(dllimport)
    #endif
#else
    #ifdef INFERENCE_EXPORTS
        #define INFERENCE_API __attribute__((visibility("default")))
    #else
        #define INFERENCE_API
    #endif
#endif

// =============================================================================
// Job Management Structure
// =============================================================================

/**
 * @brief Represents a single inference job with all its state and resources.
 */
struct Job {
    // Job identification
    int jobId;
    int seqId;
    
    // Synchronization primitives
    std::mutex              mtx;
    std::condition_variable cv;
    
    // Generation results
    std::vector<int32_t>    generatedTokens;
    std::string             generatedText;
    
    // Job state
    bool                    isFinished      = false;
    bool                    hasError        = false;
    std::string             errorMessage;
    
    // Performance metrics
    float                   tps             = 0.0f;    // Tokens per second
    float                   tts             = 0.0f;    // Time to start
    
    // Atomic flags for thread-safe operations
    std::atomic<bool>       cancelRequested{false};
    std::atomic<bool>       session_load_attempted{false};
    
    // Job parameters
    CompletionParameters    params;
    
    // Processing state
    bool                    isDecodingPrompt            = true;
    int                     n_past                      = 0;
    int                     n_remain                    = 0;
    int                     i_prompt                    = 0;
    int                     n_prompt                    = 0;
    int                     batch_pos                   = 0;
    size_t                  n_matching_session_tokens   = 0;
    
    // Token management
    std::vector<llama_token> session_tokens;
    std::vector<llama_token> embd_inp;
    std::string              path_session;
    
    // Sampling interface
    struct common_sampler*   smpl           = nullptr;
    
    // Destructor - cleanup resources
    ~Job() {
        if (smpl) {
            common_sampler_free(smpl);
        }
    }
};

// =============================================================================
// Inference Engine Interface
// =============================================================================

/**
 * @brief Interface for an inference engine.
 *
 * This class provides an interface for submitting completion jobs to an inference engine.
 * The engine can be implemented using a CPU or GPU and is responsible for managing
 * completion jobs and returning results.
 */
class INFERENCE_API InferenceEngine : public IInferenceEngine {
public:
    explicit InferenceEngine();    // Model management
    bool loadModel(const char* modelPath, const LoadingParameters lParams, const int mainGpuId = -1);
    bool unloadModel();

    // Job submission
    /**
     * @brief Submits a completion job and returns the job ID.
     * @param params The parameters for the completion job.
     * @return The ID of the submitted job.
     */
    int submitCompletionsJob(const CompletionParameters& params);

    /**
     * @brief Submits a chat completion job and returns the job ID.
     * @param params The parameters for the chat completion job.
     * @return The ID of the submitted job.
     */
    int submitChatCompletionsJob(const ChatCompletionParameters& params);

    // Job control
    /**
     * @brief Stops a job.
     * @param job_id The ID of the job to stop.
     */
    void stopJob(int job_id);

    /**
     * @brief Waits for a job to finish.
     * @param job_id The ID of the job to wait for.
     */
    void waitForJob(int job_id);

    // Job status and results
    /**
     * @brief Checks if a job is finished.
     * @param job_id The ID of the job to check.
     * @return True if the job is finished, false otherwise.
     */
    bool isJobFinished(int job_id);

    /**
     * @brief Gets the current result of a job.
     * @param job_id The ID of the job to get the result for.
     * @return The result of the job.
     * @note This function returns any results currently available, even if the job is not finished.
     */
    CompletionResult getJobResult(int job_id);

    // Error handling
    /**
     * @brief Checks if a job has an error.
     * @param job_id The ID of the job to check.
     * @return True if the job has an error, false otherwise.
     */
    bool hasJobError(int job_id);    
    
    /**
     * @brief Gets the error message for a job.
     * @param job_id The ID of the job to get the error message for.
     * @return The error message for the job.
     */
    std::string getJobError(int job_id);

    /**
     * @brief Checks if there are any active jobs currently running.
     * @return True if there are active jobs, false otherwise.
     */
    bool hasActiveJobs();

    /**
     * @brief Destructor for the InferenceEngine.
     */
    ~InferenceEngine();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

// =============================================================================
// C Interface
// =============================================================================

extern "C" INFERENCE_API IInferenceEngine* createInferenceEngine();
extern "C" INFERENCE_API void destroyInferenceEngine(IInferenceEngine* engine);

#endif // INFERENCE_H