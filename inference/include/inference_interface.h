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
// Parameter Structures
// =============================================================================

/**
 * @brief Parameters for an embedding job.
 */
struct EmbeddingParameters {
    // Input text to embed
    std::string input;
    
    // Normalize the embeddings
    bool normalize = true;
    
    // Cache and session management
    std::string kvCacheFilePath = "";
    int         seqId           = -1;

    bool isValid() const;
};

/**
 * @brief Result of an embedding job.
 */
struct EmbeddingResult {
    std::vector<float> embedding;    // Embedding vector
    int                tokens_count; // Number of tokens processed
    
    /**
     * @brief Default constructor.
     */
    EmbeddingResult() : tokens_count(0) {}
};

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
    
    // Grammar constraint (optional BNF-like grammar to constrain sampling)
    std::string grammar         = "";
    
    // JSON Schema (optional JSON schema that will be converted to grammar)
    std::string jsonSchema      = "";
    
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
    
    // Grammar constraint (optional BNF-like grammar to constrain sampling)
    std::string grammar         = "";
    
    // JSON Schema (optional JSON schema that will be converted to grammar)
    std::string jsonSchema      = "";
    
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
    float                ttft;      // Time to first token (milliseconds)
    int                  prompt_token_count; // Number of prompt tokens processed
    
    /**
     * @brief Default constructor.
     */
    CompletionResult() : tps(0.0f), ttft(0.0f), prompt_token_count(0) {}
};

/**
 * @brief Parameters for loading a model into the inference engine.
 * Based on llama.cpp common_params structure to support all llama.cpp features.
 */
struct LoadingParameters {
    // Core model parameters
    int  n_ctx              = 4096;    // Context length
    int  n_batch            = 2048;    // Logical batch size for prompt processing
    int  n_ubatch           = 512;     // Physical batch size for prompt processing
    int  n_keep             = 0;       // Number of tokens to keep from initial prompt
    int  n_chunks           = -1;      // Max number of chunks to process (-1 = unlimited)
    int  n_parallel         = 1;       // Number of parallel sequences to decode
    int  n_sequences        = 1;       // Number of sequences to decode
    int  grp_attn_n         = 1;       // Group-attention factor
    int  grp_attn_w         = 512;     // Group-attention width
    int  n_print            = -1;      // Print token count every n tokens (-1 = disabled)
    
    // RoPE (Rotary Position Embedding) parameters
    float rope_freq_base    = 0.0f;    // RoPE base frequency
    float rope_freq_scale   = 0.0f;    // RoPE frequency scaling factor
    float yarn_ext_factor   = -1.0f;   // YaRN extrapolation mix factor
    float yarn_attn_factor  = 1.0f;    // YaRN magnitude scaling factor
    float yarn_beta_fast    = 32.0f;   // YaRN low correction dim
    float yarn_beta_slow    = 1.0f;    // YaRN high correction dim
    int   yarn_orig_ctx     = 0;       // YaRN original context length
    float defrag_thold      = 0.1f;    // KV cache defragmentation threshold
    
    // Hardware acceleration and GPU parameters
    int  n_gpu_layers       = -1;      // Number of layers to store in VRAM (-1 = use default)
    int  main_gpu           = 0;       // The GPU that is used for scratch and small tensors
    float tensor_split[128] = {0};     // How split tensors should be distributed across GPUs
    int  split_mode         = 1;       // How to split the model across GPUs (0=none,1=layer,2=row)
    
    // Memory management
    bool use_mmap           = true;     // Use mmap for faster loads
    bool use_mlock          = false;    // Use mlock to keep model in memory
    bool no_kv_offload      = false;    // Disable KV offloading
    bool no_op_offload      = false;    // Globally disable offload host tensor operations to device
    bool no_extra_bufts     = false;    // Disable extra buffer types (used for weight repacking)
    
    // Processing and performance settings
    bool cont_batching      = true;     // Insert new sequences for decoding on-the-fly
    bool flash_attn         = false;    // Flash attention
    bool warmup             = true;     // Warmup run
    bool check_tensors      = false;    // Validate tensor data
    bool swa_full           = false;    // Use full-size SWA cache
    bool kv_unified         = false;    // Enable unified KV cache
    bool ctx_shift          = true;     // Context shift on infinite text generation
    
    // Cache data types
    int cache_type_k        = 1;        // KV cache data type for the K (GGML_TYPE_F16 = 1)
    int cache_type_v        = 1;        // KV cache data type for the V (GGML_TYPE_F16 = 1)
    
    // Rope scaling and pooling types
    int rope_scaling_type   = 0;        // LLAMA_ROPE_SCALING_TYPE_UNSPECIFIED = 0
    int pooling_type        = 0;        // LLAMA_POOLING_TYPE_UNSPECIFIED = 0 (for embeddings)
    int attention_type      = 0;        // LLAMA_ATTENTION_TYPE_UNSPECIFIED = 0 (for embeddings)
    
    // NUMA strategy
    int numa                = 0;        // GGML_NUMA_STRATEGY_DISABLED = 0
    
    // CPU parameters
    struct CpuParams {
        int n_threads          = -1;        // Number of threads (-1 = auto)
        bool cpumask[128]      = {false};   // CPU affinity mask
        bool mask_valid        = false;     // Default: any CPU
        int priority           = 0;         // Scheduling priority (0=normal, 1=medium, 2=high, 3=realtime)
        bool strict_cpu        = false;     // Use strict CPU placement
        int poll               = 50;        // Polling (busywait) level (0-100)
    } cpuparams;
    
    struct CpuParams cpuparams_batch;       // CPU parameters for batch processing
    
    // Embedding parameters
    bool embedding          = false;    // Get only sentence embedding
    int embd_normalize      = 2;        // Normalization for embeddings (-1=none, 0=max absolute, 1=taxicab, 2=euclidean, >2=p-norm)
    
    // Model overrides
    std::vector<std::string> kv_overrides;          // Model key-value overrides
    std::vector<std::string> tensor_buft_overrides; // Tensor buffer type overrides
    
    // LoRA adapters
    bool lora_init_without_apply = false;           // Only load LoRA to memory, don't apply
    std::vector<std::string> lora_adapters;         // LoRA adapter paths with scales
    
    // Control vectors
    std::vector<std::string> control_vectors;       // Control vector paths with scales
    int control_vector_layer_start = -1;            // Layer range for control vector
    int control_vector_layer_end   = -1;            // Layer range for control vector
    
    // Server/API parameters
    int verbosity           = 0;        // Verbosity level
    bool offline            = false;    // Offline mode
    
    // Special model paths and configurations
    std::string model_alias = "";       // Model alias
    std::string hf_token    = "";       // HuggingFace token
    
    // Multimodal parameters
    std::string mmproj_path = "";       // Multimodal projection model path
    bool mmproj_use_gpu     = true;     // Use GPU for multimodal model
    bool no_mmproj          = false;    // Explicitly disable multimodal model
    
    // Chat template parameters
    std::string chat_template = "";     // Custom chat template
    bool use_jinja          = false;    // Use Jinja templating
    bool enable_chat_template = true;   // Enable chat template processing
    
    // Input/output formatting
    bool input_prefix_bos   = false;    // Prefix BOS to user inputs
    bool escape             = true;     // Escape special characters
    bool special            = false;    // Enable special token output
    
    // Performance and debugging
    bool no_perf            = false;    // Disable performance metrics
    bool verbose_prompt     = false;    // Print prompt tokens before generation
    bool display_prompt     = true;     // Print prompt before generation
    
    // Conversation mode
    int conversation_mode   = 2;        // COMMON_CONVERSATION_MODE_AUTO = 2
    
    // Advanced parameters
    std::string lookup_cache_static  = "";  // Path of static ngram cache file for lookup decoding
    std::string lookup_cache_dynamic = "";  // Path of dynamic ngram cache file for lookup decoding
    std::string logits_file         = "";   // File for saving all logits
    
    // Perplexity calculation parameters
    int ppl_stride          = 0;        // Stride for perplexity calculations
    int ppl_output_type     = 0;        // Perplexity output type
    
    // Evaluation parameters
    bool hellaswag           = false;   // Compute HellaSwag score
    int hellaswag_tasks      = 400;     // Number of HellaSwag tasks
    bool winogrande          = false;   // Compute Winogrande score
    int winogrande_tasks     = 0;       // Number of Winogrande tasks
    bool multiple_choice     = false;   // Compute TruthfulQA score
    int multiple_choice_tasks = 0;      // Number of TruthfulQA tasks
    bool kl_divergence       = false;   // Compute KL divergence
    
    // Server-specific parameters
    int port                = 8080;     // Server port
    int timeout_read        = 600;      // HTTP read timeout in seconds
    int timeout_write       = 600;      // HTTP write timeout in seconds
    int n_threads_http      = -1;       // Number of threads to process HTTP requests
    int n_cache_reuse       = 0;        // Min chunk size to reuse from cache via KV shifting
    int n_swa_checkpoints   = 3;        // Max number of SWA checkpoints per slot
    std::string hostname    = "127.0.0.1"; // Server hostname
    std::string public_path = "";       // Public path for server
    std::string api_prefix  = "";       // API prefix for server
    
    // SSL parameters
    std::string ssl_file_key  = "";     // SSL private key file
    std::string ssl_file_cert = "";     // SSL certificate file
    
    // Advanced server features
    bool webui              = true;     // Enable web UI
    bool endpoint_slots     = false;    // Enable slots endpoint
    bool endpoint_props     = false;    // Enable properties endpoint
    bool endpoint_metrics   = false;    // Enable metrics endpoint
    bool log_json           = false;    // Log in JSON format
    std::string slot_save_path = "";    // Slot save path
    float slot_prompt_similarity = 0.5f; // Slot prompt similarity threshold
    
    // Reasoning parameters
    int reasoning_format    = 1;        // COMMON_REASONING_FORMAT_AUTO = 1
    int reasoning_budget    = -1;       // Reasoning budget
    bool prefill_assistant  = true;     // Prefill assistant message
    
    // Batch benchmark parameters
    bool is_pp_shared       = false;    // Shared prompt processing
    std::vector<int> n_pp;              // Prompt processing batch sizes
    std::vector<int> n_tg;              // Text generation batch sizes
    std::vector<int> n_pl;              // Parallel batch sizes
    
    // Context and retrieval parameters
    std::vector<std::string> context_files; // Context files to embed
    int chunk_size          = 64;       // Chunk size for context embedding
    std::string chunk_separator = "\n"; // Chunk separator for context embedding
    
    // Passkey parameters
    int n_junk              = 250;      // Number of times to repeat junk text
    int i_pos               = -1;       // Position of passkey in junk text
    
    // iMatrix parameters
    int n_out_freq          = 10;       // Output imatrix every n iterations
    int n_save_freq         = 0;        // Save imatrix every n iterations
    int i_chunk             = 0;        // Start processing from this chunk
    int imat_dat            = 0;        // Legacy imatrix.dat format output
    bool process_output     = false;    // Collect data for output tensor
    bool compute_ppl        = true;     // Whether to compute perplexity
    bool show_statistics    = false;    // Show imatrix statistics per tensor
    bool parse_special      = false;    // Parse special tokens during imatrix tokenization
    
    // Control vector generator parameters
    int n_pca_batch         = 100;      // PCA batch size
    int n_pca_iterations    = 1000;     // PCA iterations
    int cvector_dimre_method = 0;       // DIMRE_METHOD_PCA = 0
    std::string cvector_positive_file = "tools/cvector-generator/positive.txt";
    std::string cvector_negative_file = "tools/cvector-generator/negative.txt";
    
    // Infill parameters
    bool spm_infill         = false;    // Suffix/prefix/middle pattern for infill
    
    // Batch benchmark output
    bool batched_bench_output_jsonl = false; // Output batch benchmark in JSONL format
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
     * @brief Loads a model from the specified GGUF file path.
     * @param modelPath Path to the GGUF model file
     * @param lParams Loading parameters configuration
     * @param mainGpuId Primary GPU ID (-1 for auto-select)
     * @return true if model loaded successfully, false otherwise
     */
    virtual bool loadModel(const char* modelPath, 
                          const LoadingParameters lParams, 
                          const int mainGpuId = -1) = 0;

    /**
     * @brief Loads an embedding model from the specified GGUF file path.
     * @param modelPath Path to the GGUF embedding model file
     * @param lParams Loading parameters configuration
     * @param mainGpuId Primary GPU ID (-1 for auto-select)
     * @return true if embedding model loaded successfully, false otherwise
     */
    virtual bool loadEmbeddingModel(const char* modelPath, 
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

    /**
     * @brief Submits an embedding job.
     * @param params Embedding parameters
     * @return Job ID for tracking the submitted job
     */
    virtual int submitEmbeddingJob(const EmbeddingParameters& params) = 0;

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

    /**
     * @brief Retrieves the embedding result of a job.
     * @param job_id ID of the job
     * @return Embedding result
     * @note This function should only be called for embedding jobs
     */
    virtual EmbeddingResult getEmbeddingResult(int job_id) = 0;

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
    virtual std::string getJobError(int job_id) = 0;    /**
     * @brief Checks if there are any active jobs currently running.
     * @return True if there are active jobs, false otherwise
     */
    virtual bool hasActiveJobs() = 0;
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

// =============================================================================
// C-Style Factory Functions for Dynamic Loading
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Factory function to create an inference engine instance
 * @return Pointer to a new inference engine instance
 */
INFERENCE_API IInferenceEngine* createInferenceEngine();

/**
 * @brief Factory function to destroy an inference engine instance
 * @param engine Pointer to the inference engine instance to destroy
 */
INFERENCE_API void destroyInferenceEngine(IInferenceEngine* engine);

/**
 * @brief Get engine information (optional, for plugin metadata)
 * @return Engine name (e.g., "cpu", "cuda", "vulkan")
 */
INFERENCE_API const char* getEngineType();

/**
 * @brief Get engine version (optional, for plugin metadata)
 * @return Engine version string
 */
INFERENCE_API const char* getEngineVersion();

#ifdef __cplusplus
}
#endif