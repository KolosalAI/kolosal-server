#ifndef INFERENCE_INTERFACE_H
#define INFERENCE_INTERFACE_H

/**
 * @file inference_interface.h
 * @brief Pure Virtual Interface for Large Language Model Inference Engine
 * 
 * This header defines the abstract interface for inference engines that process
 * large language models. It provides a standardized API for text completion
 * and chat completion tasks with configurable parameters.
 * 
 * @section design Design Pattern
 * This interface follows the Abstract Factory pattern, allowing different
 * implementations (CPU, GPU, distributed) while maintaining a consistent API.
 * 
 * @section usage Usage Pattern
 * 1. Load a model using loadModel()
 * 2. Submit jobs using submitCompletionsJob() or submitChatCompletionsJob()
 * 3. Monitor progress with isJobFinished() or wait with waitForJob()
 * 4. Retrieve results using getJobResult()
 * 5. Handle errors with hasJobError() and getJobError()
 * 
 * @author Kolosal AI Team
 * @version 1.0
 * @date 2025
 */

#include <string>
#include <vector>

// =============================================================================
// API Export/Import Macros
// =============================================================================
#ifdef _WIN32
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
// Parameter Structures
// =============================================================================

/**
 * @brief Parameters for a completion job.
 */
struct CompletionParameters {
    // Core parameters
    std::string prompt;
    int         randomSeed      = 42;
    int         maxNewTokens    = 128;
    int         minLength       = 8;
    
    // Sampling parameters
    float       temperature     = 1.0f;
    float       topP            = 0.5f;
    
    // Behavior settings
    bool        streaming       = false;
    
    // Cache and session management
    std::string kvCacheFilePath = "";
    int         seqId           = -1;

    bool isValid() const;
};

/**
 * @brief A single message in a chat conversation.
 */
struct Message {
    std::string role;           // "user", "assistant", "system"
    std::string content;        // Message content
    
    /**
     * @brief Constructs a message with role and content.
     */
    Message(const std::string& role = "", const std::string& content = "")
        : role(role), content(content) {}
};

/**
 * @brief Parameters for a chat completion job.
 */
struct ChatCompletionParameters {
    // Conversation data
    std::vector<Message> messages;
    
    // Generation parameters
    int         randomSeed      = 42;
    int         maxNewTokens    = 128;
    int         minLength       = 8;
    
    // Sampling parameters
    float       temperature     = 1.0f;
    float       topP            = 0.5f;
    
    // Behavior settings
    bool        streaming       = false;
    
    // Cache and session management
    std::string kvCacheFilePath = "";
    int         seqId           = -1;
    
    // Tool usage parameters
    std::string tools           = "";
    std::string toolChoice      = "auto";

    bool isValid() const;
};

/**
 * @brief Result of a completion job.
 */
struct CompletionResult {
    std::vector<int32_t> tokens;    // Generated token IDs
    std::string          text;      // Generated text
    float                tps;       // Tokens per second
    
    /**
     * @brief Default constructor.
     */
    CompletionResult() : tps(0.0f) {}
};

/**
 * @brief Parameters for loading a model into the inference engine.
 */
struct LoadingParameters {
    // Context and memory settings
    int  n_ctx              = 4096;    // Context length
    int  n_keep             = 2048;    // Tokens to keep in memory
    
    // Memory optimization
    bool use_mlock          = true;    // Lock memory pages
    bool use_mmap           = true;    // Use memory mapping
    
    // Processing settings
    bool cont_batching      = true;    // Enable continuous batching
    bool warmup             = false;   // Perform warmup
    int  n_parallel         = 1;       // Parallel sequences
    
    // Hardware acceleration
    int  n_gpu_layers       = 100;     // Number of GPU layers
    
    // Batch processing
    int  n_batch            = 2048;    // Batch size
    int  n_ubatch           = 512;     // Micro-batch size
};

// =============================================================================
// Inference Engine Interface
// =============================================================================

/**
 * @brief Pure virtual interface for an inference engine.
 *
 * This abstract base class defines the contract that all inference engine
 * implementations must follow. It provides a unified API for different
 * backend implementations (CPU, GPU, distributed, etc.).
 *
 * @note All implementations must be thread-safe and support concurrent
 *       job processing with proper synchronization.
 */
class INFERENCE_API IInferenceEngine {
public:
    /**
     * @brief Virtual destructor for proper cleanup.
     */
    virtual ~IInferenceEngine() = default;

    // Model management
    /**
     * @brief Loads a model from the specified directory.
     * @param engineDir Path to the model directory
     * @param lParams Loading parameters configuration
     * @param mainGpuId Primary GPU ID (-1 for auto-select)
     * @return true if model loaded successfully, false otherwise
     */
    virtual bool loadModel(const char* engineDir, 
                          const LoadingParameters lParams, 
                          const int mainGpuId = -1) = 0;

    /**
     * @brief Unloads the currently loaded model.
     * @return true if model unloaded successfully, false otherwise
     */
    virtual bool unloadModel() = 0;

    // Job submission
    /**
     * @brief Submits a text completion job.
     * @param params Completion parameters
     * @return Job ID for tracking the submitted job
     */
    virtual int submitCompletionsJob(const CompletionParameters& params) = 0;

    /**
     * @brief Submits a chat completion job.
     * @param params Chat completion parameters
     * @return Job ID for tracking the submitted job
     */
    virtual int submitChatCompletionsJob(const ChatCompletionParameters& params) = 0;

    // Job control
    /**
     * @brief Stops a running job.
     * @param job_id ID of the job to stop
     */
    virtual void stopJob(int job_id) = 0;

    /**
     * @brief Waits for a job to complete.
     * @param job_id ID of the job to wait for
     */
    virtual void waitForJob(int job_id) = 0;

    // Job status and results
    /**
     * @brief Checks if a job has finished.
     * @param job_id ID of the job to check
     * @return true if job is finished, false otherwise
     */
    virtual bool isJobFinished(int job_id) = 0;

    /**
     * @brief Retrieves the result of a job.
     * @param job_id ID of the job
     * @return Completion result (may be partial if job is still running)
     * @note This function returns any results currently available, even if the job is not finished.
     */
    virtual CompletionResult getJobResult(int job_id) = 0;

    // Error handling
    /**
     * @brief Checks if a job has encountered an error.
     * @param job_id ID of the job to check
     * @return true if job has an error, false otherwise
     */
    virtual bool hasJobError(int job_id) = 0;

    /**
     * @brief Gets the error message for a job.
     * @param job_id ID of the job
     * @return Error message string (empty if no error)
     */
    virtual std::string getJobError(int job_id) = 0;
};

// =============================================================================
// Factory Function Type
// =============================================================================

/**
 * @brief Function type definition for creating inference engine instances.
 * 
 * This type definition is used for dynamic loading of inference engine
 * implementations from shared libraries.
 */
typedef IInferenceEngine* (*CreateInferenceEngineFn)();

#endif // INFERENCE_INTERFACE_H