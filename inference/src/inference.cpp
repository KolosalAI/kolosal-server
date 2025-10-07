#define INFERENCE_EXPORTS

#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <queue>
#include <set>
#include <cstdint>
#include <thread>
#include <condition_variable>
#include <functional>
#include <type_traits>
#include <fstream>
#include <cstring>
#include <chrono>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <initializer_list>
#ifdef USE_VULKAN
#include <vulkan/vulkan.h>
#endif

#include "llama.h"
#include "common.h"
#include "sampling.h"
#include "inference.h"
#include "chat.h"
#include <nlohmann/json.hpp>
#include "json-schema-to-grammar.h"

// Avoid Windows macro conflicts with std::min/std::max
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

class ThreadPool {
public:
	explicit ThreadPool(const int maxBatch = 1);
	~ThreadPool();

	// Submit a task to the thread pool
	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		-> std::future<typename std::invoke_result<F, Args...>::type>;

	// Shutdown the thread pool
	void shutdown();

	// Return the number of active threads in the pool
	size_t size() const { return workers.size(); }

private:
	// Worker function for each thread
	void worker();

	// Members
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;

	// Synchronization
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
};

//-------------------------------------------------------------------------------------------------
// Thread Pool function definitions
//-------------------------------------------------------------------------------------------------

inline ThreadPool::ThreadPool(const int maxBatch) : stop(false)
{
	size_t num_threads = maxBatch;

#ifdef DEBUG
	std::cout << "[INFERENCE] Creating thread pool with " << num_threads << " threads" << std::endl;
#endif

	for (size_t i = 0; i < num_threads; ++i)
	{
		workers.emplace_back(&ThreadPool::worker, this);
	}
}

inline ThreadPool::~ThreadPool()
{
	shutdown();
}

inline void ThreadPool::shutdown()
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		if (stop) return; // Already shutdown
		stop = true;
	}
	condition.notify_all();
	for (std::thread& worker : workers)
	{
		if (worker.joinable())
		{
			worker.join();
		}
	}
	workers.clear();
}

inline void ThreadPool::worker()
{
	while (true)
	{
		std::function<void()> task;

		{
			std::unique_lock<std::mutex> lock(this->queue_mutex);

			this->condition.wait(lock, [this] {
				return this->stop || !this->tasks.empty();
				});

			if (this->stop && this->tasks.empty())
			{
				return;
			}

			if (this->tasks.empty()) {
				continue; // Spurious wakeup protection
			}

			task = std::move(this->tasks.front());
			this->tasks.pop();
		}

		// Execute task outside of lock to prevent deadlocks
		try {
			task();
		} catch (const std::exception& e) {
			// Log error but continue processing
			std::cerr << "[ThreadPool] Task execution failed: " << e.what() << std::endl;
		} catch (...) {
			std::cerr << "[ThreadPool] Task execution failed with unknown exception" << std::endl;
		}
	}
}

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::invoke_result<F, Args...>::type>
{
#ifdef DEBUG
	std::cout << "[INFERENCE] Enqueueing task to thread pool" << std::endl;
#endif

	using return_type = typename std::invoke_result<F, Args...>::type;

	auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
	);

	std::future<return_type> res = task_ptr->get_future();

	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		// Don't allow enqueueing after stopping the pool
		if (stop)
		{
			std::cerr << "[INFERENCE] [ERROR] Enqueue on stopped ThreadPool" << std::endl;
			return res;
		}

		tasks.emplace([task_ptr]() { (*task_ptr)(); });
	}

	condition.notify_one();
	return res;
}

bool CompletionParameters::isValid() const
{
	if (prompt.empty())
	{
		std::cerr << "[INFERENCE] [ERROR] prompt is empty" << std::endl;
		return false;
	}

	if (randomSeed < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] randomSeed is negative: " << randomSeed << std::endl;
		return false;
	}

	if (maxNewTokens < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] maxNewTokens is out of range: " << maxNewTokens << std::endl;
		return false;
	}

	if (minLength < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] minLength is out of range: " << minLength << std::endl;
		return false;
	}

	if (temperature < 0.0f)
	{
		std::cerr << "[INFERENCE] [ERROR] temperature is negative: " << temperature << std::endl;
		return false;
	}

	if (topP < 0.0f || topP > 1.0f)
	{
		std::cerr << "[INFERENCE] [ERROR] topP is out of range: " << topP << std::endl;
		return false;
	}

	if (!kvCacheFilePath.empty() && seqId < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] seqId needs to be set when kvCacheFilePath is provided" << std::endl;
		return false;
	}

	// Grammar / JSON Schema validation
	if (!grammar.empty() && !jsonSchema.empty())
	{
		std::cerr << "[INFERENCE] [ERROR] Provide either 'grammar' or 'jsonSchema', not both" << std::endl;
		return false;
	}

	if (!jsonSchema.empty())
	{
		try {
			// Validate that it's valid JSON. Use ordered_json to match converter signature
			auto schema = nlohmann::ordered_json::parse(jsonSchema);
			(void)schema; // only validating here
		} catch (const std::exception &e) {
			std::cerr << "[INFERENCE] [ERROR] Invalid JSON schema: " << e.what() << std::endl;
			return false;
		}
	}

	return true;
}

bool ChatCompletionParameters::isValid() const
{
	if (messages.empty())
	{
		std::cerr << "[INFERENCE] [ERROR] messages is empty; size: " << messages.size() << std::endl;
		return false;
	}

	if (randomSeed < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] randomSeed is negative: " << randomSeed << std::endl;
		return false;
	}

	if (maxNewTokens < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] maxNewTokens is out of range: " << maxNewTokens << std::endl;
		return false;
	}

	if (minLength < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] minLength is out of range: " << minLength << std::endl;
		return false;
	}

	if (temperature < 0.0f)
	{
		std::cerr << "[INFERENCE] [ERROR] temperature is negative: " << temperature << std::endl;
		return false;
	}

	if (topP < 0.0f || topP > 1.0f)
	{
		std::cerr << "[INFERENCE] [ERROR] topP is out of range: " << topP << std::endl;
		return false;
	}

	if (!kvCacheFilePath.empty() && seqId < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] seqId needs to be set when kvCacheFilePath is provided" << std::endl;
		return false;
	}

	// Grammar / JSON Schema validation
	if (!grammar.empty() && !jsonSchema.empty())
	{
		std::cerr << "[INFERENCE] [ERROR] Provide either 'grammar' or 'jsonSchema', not both" << std::endl;
		return false;
	}

	if (!jsonSchema.empty())
	{
		try {
			auto schema = nlohmann::ordered_json::parse(jsonSchema);
			(void)schema;
		} catch (const std::exception &e) {
			std::cerr << "[INFERENCE] [ERROR] Invalid JSON schema: " << e.what() << std::endl;
			return false;
		}
	}

	return true;
}

bool EmbeddingParameters::isValid() const
{
	if (input.empty())
	{
		std::cerr << "[INFERENCE] [ERROR] input is empty" << std::endl;
		return false;
	}

	// Input should not be too long (reasonable limit)
	if (input.length() > 100000) // 100KB limit
	{
		std::cerr << "[INFERENCE] [ERROR] input is too long: " << input.length() << " characters" << std::endl;
		return false;
	}

	return true;
}

// Anonymous namespace to encapsulate internal classes
namespace
{
	// Manages KV sequence IDs (slots) for llama context
	class SlotManager {
	public:
		SlotManager(llama_context * ctx, int n_parallel)
			: ctx(ctx), max_slots(n_parallel) {
			for (int i = 0; i < max_slots; ++i) free_slots.push(i);
		}
		int allocate() {
			std::unique_lock<std::mutex> lock(mtx);
			cv.wait(lock, [&]{ return !free_slots.empty() || terminated; });
			if (terminated) return -1;
			int id = free_slots.front(); free_slots.pop(); in_use.insert(id); return id;
		}
		void release(int id) {
			if (id < 0) return;
			if (ctx) {
				// remove all KV entries for this sequence id
				auto * mem = llama_get_memory(ctx);
				// use range [0, -1) to ensure full wipe across backends (CUDA may no-op on [-1, -1])
				llama_memory_seq_rm(mem, id, /*p0=*/0, /*p1=*/-1);
			}
			std::lock_guard<std::mutex> lock(mtx);
			if (in_use.erase(id)) { free_slots.push(id); cv.notify_one(); }
		}
		void shutdown() { std::lock_guard<std::mutex> lock(mtx); terminated = true; cv.notify_all(); }
		int capacity() const { return max_slots; }
	private:
		llama_context * ctx;
		int max_slots;
		std::queue<int> free_slots;
		std::set<int>   in_use;
		std::mutex mtx; std::condition_variable cv; bool terminated=false;
	};

	static void llama_log_callback_null(ggml_log_level level, const char* text, void* user_data)
	{
		(void)level;
		(void)text;
		(void)user_data;
	}

	class Tokenizer
	{
	public:
		// New constructor takes shared pointers to model and context
		Tokenizer(llama_model* shared_model, llama_context* shared_context, common_params& params);
		~Tokenizer();

		std::vector<int32_t>	tokenize(const std::string& text, bool add_bos = true);
		std::string				detokenize(const std::vector<int32_t>& tokens);
		std::string				decode(const int32_t& token);
		std::string				applyTemplate(std::vector<common_chat_msg>& messages);

		const	llama_vocab		*getVocab()		const { return vocab; }
				llama_model		*getModel()		const { return tokenizer_model; }
				llama_context	*getContext()	const { return tokenizer_context; }

		bool shouldAddBos() const { return add_bos; }

	private:
		const	llama_vocab		*vocab;
				llama_model		*tokenizer_model;
				llama_context	*tokenizer_context;

		common_chat_templates_ptr chat_templates;
		bool					  add_bos;
	};

	Tokenizer::Tokenizer(llama_model* shared_model, llama_context* shared_context, common_params& params)
		: tokenizer_model(shared_model)
		, tokenizer_context(shared_context)
		, add_bos(false)
	{
#ifdef DEBUG
		std::cout << "[INFERENCE] Initializing Tokenizer with shared model and context." << std::endl;
#endif
		vocab = llama_model_get_vocab(tokenizer_model);
		add_bos = llama_vocab_get_add_bos(vocab);

		chat_templates = common_chat_templates_init(tokenizer_model, params.chat_template);
		try {
			const std::map<std::string, std::string> empty_kwargs{};
			common_chat_format_example(chat_templates.get(), params.use_jinja, empty_kwargs);
		}
		catch (const std::exception& e) {
			std::cerr << "[INFERENCE] [ERROR] Failed to format chat templates: " << e.what() << std::endl;
			chat_templates = common_chat_templates_init(tokenizer_model, "chatml");
		}
	}

	Tokenizer::~Tokenizer()
	{
	}

	std::vector<int32_t> Tokenizer::tokenize(const std::string& text, bool /*add_bos_token*/)
	{
		std::vector<llama_token> tokens = common_tokenize(tokenizer_context, text.c_str(), true, true);
		return std::vector<int32_t>(tokens.begin(), tokens.end());
	}

	std::string Tokenizer::detokenize(const std::vector<int32_t>& tokens)
	{
		std::ostringstream tokensStream;
		for (const auto& token : tokens)
		{
			tokensStream << decode(token);
		}
		return tokensStream.str();
	}

	std::string Tokenizer::decode(const int32_t& token)
	{
		return common_token_to_piece(tokenizer_context, token);
	}

	std::string Tokenizer::applyTemplate(std::vector<common_chat_msg>& messages)
	{
		common_chat_templates_inputs inputs;
		inputs.messages = messages;

		return common_chat_templates_apply(chat_templates.get(), inputs).prompt;
	}

	// InferenceService Interface (Internal Use Only)
	class InferenceService
	{
	public:
		virtual ~InferenceService() {}
		virtual void start() = 0;
		virtual void stop() = 0;
		virtual void submitJob(const CompletionParameters &params, std::shared_ptr<Job> job) = 0;
		virtual void complete(const CompletionParameters &params, std::shared_ptr<Job> job) = 0;
		virtual void embed(const EmbeddingParameters &params, std::shared_ptr<Job> job) = 0;
		virtual CompletionParameters formatChat(const ChatCompletionParameters &params) = 0;
	};
	// LlamaInferenceService (CPU Implementation)
	class LlamaInferenceService : public InferenceService
	{
	private:
		// Data members moved before constructor to satisfy MSVC name lookup in mem-initializers
		std::shared_ptr<Tokenizer>			tokenizer;
		struct llama_model*					model;
		struct llama_context*				context;
		std::mutex							mtx;
		std::condition_variable				cv;
		common_params						g_params;
		ggml_threadpool*					threadpool;
		llama_batch							batch;
		std::vector<std::shared_ptr<Job>>	jobs;
		std::atomic<bool>					should_terminate{ false };
		std::thread							inferenceThread;

		const int n_batch;
		const int n_keep;
		const int n_ctx;
		SlotManager slotManager;

	public:
		LlamaInferenceService(std::shared_ptr<Tokenizer> tokenizer, llama_model* model, llama_context* context, 
			common_params params, ggml_threadpool* threadpool)
			: tokenizer(std::move(tokenizer)), model(model), context(context), g_params(params), threadpool(threadpool),
			n_batch(params.n_batch), n_keep(params.n_keep), n_ctx(llama_n_ctx(context)), slotManager(context, params.n_parallel)
		{
#ifdef DEBUG
			std::cout << "Initializing batch with size of: " << g_params.n_batch << std::endl;
#endif
			// initialize for multi-sequence decoding
			batch = llama_batch_init(params.n_ctx, 0, params.n_parallel);

			inferenceThread = std::thread(&LlamaInferenceService::start, this);
		}

		~LlamaInferenceService()
		{
			stop();
			slotManager.shutdown();

			// Wait for the inference thread to finish before cleaning up resources
			if (inferenceThread.joinable()) {
				inferenceThread.join();
			}

			// Clean up all remaining jobs to prevent accessing freed resources
			{
				std::lock_guard<std::mutex> lock(mtx);
				for (auto& job : jobs) {
					if (job) { // Null check for safety
						std::lock_guard<std::mutex> jobLock(job->mtx);
						if (!job->isFinished) {
							job->hasError = true;
							job->errorMessage = "Service is shutting down";
							job->isFinished = true;
							job->cv.notify_all();
						}
						if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
						if (job->smpl) {
							common_sampler_free(job->smpl);
							job->smpl = nullptr;
						}
					}
				}
				jobs.clear();
			}

			// Safe cleanup of llama resources
			if (context) {
				llama_free(context);
				context = nullptr;
			}
			if (model) {
				llama_model_free(model);
				model = nullptr;
			}
			llama_batch_free(batch);

			if (threadpool) {
				ggml_threadpool_free(threadpool);
				threadpool = nullptr;
			}
		}

		void stop() override
		{
			should_terminate = true;
			// Wake up the inference thread if it's waiting
			cv.notify_all();
		}

		void start() override
		{
			while (!should_terminate)
			{
				std::vector<std::shared_ptr<Job>> current_jobs;
				{
					std::unique_lock<std::mutex> lock(mtx);
					if (jobs.empty()) {
						cv.wait(lock, [this] { return !jobs.empty() || should_terminate; });
					}
					if (should_terminate) break;
					current_jobs = jobs; // Copy jobs to process without holding the lock
				}

				// Check for shutdown again after acquiring jobs
				if (should_terminate) break;

				bool batch_has_tokens = false;

				// determine fair per-job prompt quota for this decode step
				int active_jobs = 0;
				for (auto &job : current_jobs) {
					std::lock_guard<std::mutex> jl(job->mtx);
					if (!job->isFinished && !job->hasError) {
						active_jobs++;
					}
				}
				// avoid divide-by-zero; ensure at least 1 token per job
				const int per_job_quota = std::max(1, g_params.n_batch / std::max(1, active_jobs));

				for (auto job : current_jobs)
				{
					// Check for shutdown in the job processing loop
					if (should_terminate) break;

					std::lock_guard<std::mutex> jobLock(job->mtx);

					if (job->isFinished || job->hasError)
						continue;

					if (checkCancellation(job) || (job->n_remain <= 0 && job->params.maxNewTokens != 0)) {
						saveSession(job);
						if (job->smpl) {
							common_sampler_free(job->smpl);
							job->smpl = nullptr;
						}
						if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
						job->isFinished = true;
						job->cv.notify_all();
						continue;
					}

					if (!ensureContextCapacity(job))
					{
						if (job->smpl) {
							common_sampler_free(job->smpl);
							job->smpl = nullptr;
						}
						if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
						job->hasError = true;
						job->isFinished = true;
						job->errorMessage = "Context overflow even after trimming.";
						job->cv.notify_all();
						continue;
					}

					if (!job->isDecodingPrompt) {
						// respect overall batch limit and internal capacity
						int capacity_remaining = n_ctx - batch.n_tokens;
						if (capacity_remaining <= 0 || batch.n_tokens >= g_params.n_batch) {
							break;
						}

						if (!sampleNextToken(job)) {
							saveSession(job);
							if (job->smpl) {
								common_sampler_free(job->smpl);
								job->smpl = nullptr;
							}
							if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
							job->isFinished = true;
							job->cv.notify_all();
							continue;
						}

						job->n_past += 1;
						batch_has_tokens = true;
					}
					else {
						if (!loadSession(job)) {
							if (job->smpl) {
								common_sampler_free(job->smpl);
								job->smpl = nullptr;
							}
							if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
							job->hasError = true;
							job->isFinished = true;
							job->errorMessage = "Failed to load sessions";
							job->cv.notify_all();
							continue;
						}

						if (!getInputTokens(job)) {
							if (job->smpl) {
								common_sampler_free(job->smpl);
								job->smpl = nullptr;
							}
							if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
							job->hasError = true;
							job->isFinished = true;
							job->errorMessage = "Failed to tokenize input";
							job->cv.notify_all();
							continue;
						}

						if (!ensureNonEmptyInput(job)) {
							if (job->smpl) {
								common_sampler_free(job->smpl);
								job->smpl = nullptr;
							}
							if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
							job->hasError = true;
							job->isFinished = true;
							job->errorMessage = "Failed to ensure input content";
							job->cv.notify_all();
							continue;
						}
						
						job->n_matching_session_tokens = matchSessionTokens(job);
						job->n_past = static_cast<int>(job->n_matching_session_tokens);
						job->i_prompt = static_cast<int>(job->n_matching_session_tokens);
						job->n_prompt = static_cast<int>(job->embd_inp.size());

						int remaining_prompt_tokens = job->n_prompt - job->i_prompt;
						// respect overall batch limit and internal capacity
						int capacity_remaining = n_ctx - batch.n_tokens;
						int available_batch_space = std::min(g_params.n_batch - batch.n_tokens, capacity_remaining);

						if (available_batch_space <= 0) {
							break;
						}

						// fair-share: each job contributes up to per_job_quota tokens per decode step
						int tokens_to_process = std::min(remaining_prompt_tokens, std::min(available_batch_space, per_job_quota));

						for (int i = 0; i < tokens_to_process; ++i) {
							llama_token token = job->embd_inp[job->i_prompt];
							common_batch_add(batch, token, job->i_prompt, { job->seqId }, false);
							if (job->i_prompt == job->n_prompt - 1)
							{
								batch.logits[batch.n_tokens - 1] = true;
								job->batch_pos = batch.n_tokens - 1;
							}

							common_sampler_accept(job->smpl, token, false);
							job->session_tokens.push_back(token);
							++(job->i_prompt);
							++(job->n_past);
							batch_has_tokens = true;
						}

						if (job->i_prompt >= job->n_prompt) {
							job->isDecodingPrompt = false;
						}

						// ensure capacity even during prompt ingestion
						if (!ensureContextCapacity(job)) {
							if (job->smpl) {
								common_sampler_free(job->smpl);
								job->smpl = nullptr;
							}
							if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
							job->hasError = true;
							job->isFinished = true;
							job->errorMessage = "Context overflow even after trimming.";
							job->cv.notify_all();
							continue;
						}
					}
				}

				if (batch_has_tokens && !should_terminate && context)
				{
					if (llama_decode(context, batch))
					{
						for (auto job : current_jobs)
						{
							std::lock_guard<std::mutex> jobLock(job->mtx);
							if (!job->isFinished) {
								job->hasError = true;
								job->errorMessage = "Could not decode next token";
								job->isFinished = true;
								if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
								job->cv.notify_all();
							}
						}
					}

					common_batch_clear(batch);
				}
			}
			
			// Ensure all remaining jobs are properly cleaned up when exiting
			{
				std::lock_guard<std::mutex> lock(mtx);
				for (auto& job : jobs) {
					std::lock_guard<std::mutex> jobLock(job->mtx);
					if (!job->isFinished) {
						job->hasError = true;
						job->errorMessage = "Service is shutting down";
						job->isFinished = true;
						if (job->seqId >= 0) { slotManager.release(job->seqId); job->seqId = -1; }
						job->cv.notify_all();
					}
				}
			}
		}

		void submitJob(const CompletionParameters& params, std::shared_ptr<Job> job) override
		{
#ifdef DEBUG
			std::cout << "[INFERENCE] Submitting job with prompt: \n" << params.prompt << std::endl;
#endif

			if (!validateParameters(params, job)) {
				return;
			}

			job->params = params;

			job->smpl = initializeSampler(params, job);
			if (!job->smpl) {
				return;
			}

			{
				std::lock_guard<std::mutex> jobLock(job->mtx);
				job->generatedTokens.clear();
				job->generatedText.clear();
			}

			// Acquire a managed seq slot; block if busy
			const int slot_id = slotManager.allocate();
			if (slot_id < 0) {
				std::lock_guard<std::mutex> jobLock(job->mtx);
				job->hasError = true;
				job->errorMessage = "Service is shutting down";
				job->cv.notify_all();
				return;
			}
			job->seqId = slot_id;
			// defensive: ensure the slot's KV is empty before use (CUDA can be strict)
			if (context) {
				auto * mem = llama_get_memory(context);
				llama_memory_seq_rm(mem, job->seqId, /*p0=*/0, /*p1=*/-1);
			}
			job->path_session				= params.kvCacheFilePath;
			job->n_remain					= params.maxNewTokens;
			job->isDecodingPrompt			= true;
			job->isFinished					= false;

			{
				std::lock_guard<std::mutex> lock(mtx);
				jobs.push_back(job);
			}

			cv.notify_one();
		}

		void complete(const CompletionParameters& params, std::shared_ptr<Job> job) override
		{
#ifdef DEBUG
			std::cout << "[INFERENCE] Submitting job with sequence ID: " << params.seqId << std::endl;
#endif

			submitJob(params, job);

			{
				std::unique_lock<std::mutex> lock(job->mtx);
				job->cv.wait(lock, [&job] { return job->isFinished; });
			}
		}

		void embed(const EmbeddingParameters & /*params*/, std::shared_ptr<Job> job) override
		{
			// This service handles only text/chat completion; embeddings are delegated to EmbeddingInferenceService
			std::lock_guard<std::mutex> jobLock(job->mtx);
			job->hasError = true;
			job->errorMessage = "Embedding is not supported by this model/service";
			job->isFinished = true;
			job->cv.notify_all();
		}

		CompletionParameters formatChat(const ChatCompletionParameters& params) override
		{
			if (!params.isValid())
			{
				throw std::runtime_error("[INFERENCE] [CHATCOMPLETE] [ERROR] Invalid chat completion parameters\n");
			}

			// Format the chat messages into a single prompt
			std::vector<common_chat_msg> messages;
			for (const auto& msg : params.messages)
			{
				messages.push_back(common_chat_msg{ msg.role, msg.content });
			}

			std::string formatted;

			formatted = tokenizer->applyTemplate(messages);
			
			CompletionParameters completionParams{
				formatted.c_str(),
				params.randomSeed,
				params.maxNewTokens,
				params.minLength,
				params.temperature,
				params.topP,
				/*grammar*/ "",
				/*jsonSchema*/ "",
				params.streaming,
				params.kvCacheFilePath,
				params.seqId
			};

			// Carry over grammar or jsonSchema
			completionParams.grammar = params.grammar;
			completionParams.jsonSchema = params.jsonSchema;

			return completionParams;
		}

	private:


#ifdef DEBUG
		void printLogits(llama_context* ctx, size_t maxLogits = 10) {
			// Get the logits after decoding
			float* logits = llama_get_logits_ith(ctx, -1);

			// Get vocabulary size (number of logits)
			const int n_vocab = llama_vocab_n_tokens(tokenizer->getVocab());

			std::cout << "\n----- Logits after decoding -----\n";

			// Calculate some statistics
			float min_logit = std::numeric_limits<float>::max();
			float max_logit = std::numeric_limits<float>::lowest();
			float sum_logit = 0.0f;

			for (int i = 0; i < n_vocab; i++) {
				min_logit = std::min(min_logit, logits[i]);
				max_logit = std::max(max_logit, logits[i]);
				sum_logit += logits[i];
			}

			float mean_logit = sum_logit / n_vocab;

			std::cout << "Vocab size: " << n_vocab << std::endl;
			std::cout << "Min logit: " << min_logit << std::endl;
			std::cout << "Max logit: " << max_logit << std::endl;
			std::cout << "Mean logit: " << mean_logit << std::endl;

			// Create vector of (token, logit) pairs for sorting
			std::vector<std::pair<int, float>> token_logits;
			for (int i = 0; i < n_vocab; i++) {
				token_logits.push_back({ i, logits[i] });
			}

			// Sort by logit value in descending order
			std::sort(token_logits.begin(), token_logits.end(),
				[](const auto& a, const auto& b) { return a.second > b.second; });

			// Print top N tokens with highest logits
			std::cout << "\nTop " << maxLogits << " tokens:\n";
			std::cout << "|--------|------------|---------------------------------|\n";
			std::cout << "| Token  â Logit      â Text                            â\n";
			std::cout << "|--------|------------|---------------------------------|\n";

			for (size_t i = 0; i < std::min(maxLogits, token_logits.size()); i++) {
				int token_id = token_logits[i].first;
				float token_logit = token_logits[i].second;

				// Get the token text (you'll need to adapt this based on your tokenizer implementation)
				std::string token_text = tokenizer->decode(token_id);

				// Escape special characters for display
				std::string escaped_text = "";
				for (char c : token_text) {
					if (c == '\n') escaped_text += "\\n";
					else if (c == '\r') escaped_text += "\\r";
					else if (c == '\t') escaped_text += "\\t";
					else if (c < 32 || c > 126) escaped_text += "Â·";
					else escaped_text += c;
				}

				// Truncate if too long
				if (escaped_text.length() > 31) {
					escaped_text = escaped_text.substr(0, 28) + "...";
				}

				std::cout << "| " << std::setw(6) << token_id << " | "
					<< std::setw(10) << std::fixed << std::setprecision(6) << token_logit << " | "
					<< std::left << std::setw(31) << escaped_text << " |\n";
			}

			std::cout << "|--------|------------|---------------------------------|\n";
		}
#endif

		bool validateParameters(const CompletionParameters& params, std::shared_ptr<Job> job) {
			if (!params.isValid()) {
				std::lock_guard<std::mutex> jobLock(job->mtx);
				job->hasError = true;
				job->errorMessage = "Invalid completion parameters";
				job->cv.notify_all();
				return false;
			}
			return true;
		}

		common_sampler* initializeSampler(const CompletionParameters& params, std::shared_ptr<Job> job) {
			auto sparams = g_params.sampling;
			sparams.top_p = params.topP;
			sparams.temp = params.temperature;
			sparams.seed = params.randomSeed;
			// sparams.top_k = params.topK;
			sparams.no_perf = false;

			// Handle grammar or JSON schema -> grammar
			try {
				if (!params.jsonSchema.empty()) {
					auto schema = nlohmann::ordered_json::parse(params.jsonSchema);
					std::string gbnf = json_schema_to_grammar(schema, /*force_gbnf=*/true);
					sparams.grammar = gbnf; // copies string into sampling params
				} else if (!params.grammar.empty()) {
					sparams.grammar = params.grammar;
				}
			} catch (const std::exception &e) {
				std::lock_guard<std::mutex> jobLock(job->mtx);
				job->hasError = true;
				job->errorMessage = std::string("Failed to prepare grammar: ") + e.what();
				job->cv.notify_all();
				return nullptr;
			}

			common_sampler* sampler = common_sampler_init(model, sparams);
			if (!sampler) {
				std::lock_guard<std::mutex> jobLock(job->mtx);
				job->hasError = true;
				job->errorMessage = "Could not initialize sampler";
				job->cv.notify_all();
				return nullptr;
			}
			return sampler;
		}

		bool loadSession(std::shared_ptr<Job> job) {
			if (!job->path_session.empty()) {
				// Use the logical session id from params for file-backed sessions
				const int session_seq = job->params.seqId;
				if (!load_kv_cache(job->path_session, job->session_tokens, session_seq)) {
					return false;
				}
			}
			return true;
		}

		bool getInputTokens(std::shared_ptr<Job> job) {
			if (job->session_tokens.empty() || !job->params.prompt.empty()) {
				job->embd_inp = tokenizer->tokenize(job->params.prompt, tokenizer->shouldAddBos());
			}
			else {
				job->embd_inp = job->session_tokens;
			}
			return true;
		}

		bool ensureNonEmptyInput(std::shared_ptr<Job> job) {
			if (job->embd_inp.empty()) {
				if (tokenizer->shouldAddBos()) {
					job->embd_inp.push_back(llama_vocab_bos(llama_model_get_vocab(tokenizer->getModel())));
				}
				else {
					return false;
				}
			}
			return true;
		}

		bool checkCancellation(std::shared_ptr<Job> job) {
			return job->cancelRequested.load();
		}

		bool ensureContextCapacity(std::shared_ptr<Job> job) {
			if (job->n_past + 1 > n_ctx) {
				kv_cache_seq_ltrim(context, n_keep, job->session_tokens, job->n_past, job->seqId);
				if (job->n_past + 1 > n_ctx) {
					return false;
				}
			}
			return true;
		}

		void saveSession(std::shared_ptr<Job> job) {
			if (!job->path_session.empty()) {
				// Use the logical session id from params for file-backed sessions
				const int session_seq = job->params.seqId;
				llama_state_seq_save_file(context, job->path_session.c_str(), session_seq, job->session_tokens.data(), job->session_tokens.size());
			}
		}

		bool sampleNextToken(std::shared_ptr<Job> job) {
			llama_token id = common_sampler_sample(job->smpl, context, job->batch_pos);
			common_sampler_accept(job->smpl, id, false);

			if (llama_vocab_is_eog(tokenizer->getVocab(), id) || id == llama_vocab_eos(tokenizer->getVocab())) {
				return false; // Stop generation
			}

			common_batch_add(batch, id, job->n_past, { job->seqId }, true);

			const auto data = llama_perf_context(context);
			const std::string token_str = tokenizer->decode(id);
			{
				// Record first token timing if this is the first generated token
				if (job->generatedTokens.empty()) {
					job->first_token_time = std::chrono::steady_clock::now();
					job->first_token_generated = true;
					
					// Calculate TTFT in milliseconds
					auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
						job->first_token_time - job->start_time
					);
					job->ttft = static_cast<float>(duration.count()) / 1000.0f;
					
#ifdef DEBUG
					std::cout << "[INFERENCE] First token generated. TTFT: " << job->ttft << " ms" << std::endl;
#endif
				}
				job->generatedTokens.push_back(id);
				job->generatedText += token_str;
				job->tps = static_cast<float>(1e3 / data.t_eval_ms * data.n_eval);
				job->cv.notify_all();
			}

			if (!job->path_session.empty()) {
				job->session_tokens.push_back(id);
			}

			job->n_remain -= 1;
			job->batch_pos = batch.n_tokens - 1;

			return true;
		}

		bool load_kv_cache(const std::string& path, std::vector<llama_token>& session_tokens, const int jobId)
		{
			if (!path.empty())
			{
				// Attempt to load
				if (!std::filesystem::exists(path))
				{
					// file doesn't exist => no old cache
					printf("[KV] session file does not exist, will create.\n");
					return true;
				}
				else if (std::filesystem::is_empty(path))
				{
					// file is empty => treat as brand-new
					printf("[KV] session file is empty, new session.\n");
					return true;
				}
				else
				{
					// The file exists and is not empty
					session_tokens.resize(g_params.n_ctx);  // allocate buffer
					size_t n_token_count_out = 0;

					bool load_success = llama_state_seq_load_file(
						context,
						path.c_str(),
						jobId,
						session_tokens.data(),
						session_tokens.capacity(),
						&n_token_count_out
					);

					if (!load_success)
					{
						// Loading failed - delete the corrupt file and create a new one
						printf("[KV] ERROR: Failed to load session file, deleting corrupt file and creating a new one.\n");
						try {
							std::filesystem::remove(path);
						}
						catch (const std::filesystem::filesystem_error& e) {
							printf("[KV] WARNING: Could not delete corrupt session file: %s\n", e.what());
						}

						// Clear any partial data
						session_tokens.clear();

						// Return true to continue with a new cache instead of failing
						return true;
					}

					// The llama_state_load_file call gives us how many tokens were in that old session
					session_tokens.resize(n_token_count_out);

#ifdef DEBUG
					printf("[INFERENCE] [KV] loaded session with prompt size: %d tokens\n", (int)session_tokens.size());
					printf("[INFERENCE] [KV] tokens decoded: %s\n", tokenizer->detokenize(session_tokens).c_str());
#endif
					return true;
				}
			}
			return true;
		}

		size_t matchSessionTokens(std::shared_ptr<Job> job)
		{
			size_t n_matching_session_tokens = 0;

			if (!job->session_tokens.empty()) {
				const size_t n_preserve = g_params.n_keep;

				if (job->embd_inp.size() < n_preserve) {
					for (size_t i = 0; i < job->embd_inp.size() && i < job->session_tokens.size(); i++) {
						if (job->embd_inp[i] != job->session_tokens[i])
							break;
						n_matching_session_tokens++;
					}
				}
				else {
					// First, check the preserved prefix.
					bool prefix_matches = true;
					for (size_t i = 0; i < n_preserve; i++) {
						if (job->embd_inp[i] != job->session_tokens[i]) {
							prefix_matches = false;
							break;
						}
					}
					if (!prefix_matches) {
						// Fallback to simple matching from the beginning.
						for (size_t i = 0; i < job->embd_inp.size() && i < job->session_tokens.size(); i++) {
							if (job->embd_inp[i] != job->session_tokens[i])
								break;
							n_matching_session_tokens++;
						}
					}
					else {
						// The preserved prefix matches.
						// Compute the expected gap (i.e. the number of tokens that were dropped during shifting).
						size_t gap = (job->embd_inp.size() > job->session_tokens.size())
							? job->embd_inp.size() - job->session_tokens.size()
							: 0;
						// Check the shifted suffix.
						bool shifted_matches = true;
						size_t shifted_tokens = job->session_tokens.size() > n_preserve ? job->session_tokens.size() - n_preserve : 0;
						for (size_t i = 0; i < shifted_tokens; i++) {
							size_t prompt_index = n_preserve + gap + i;
							if (prompt_index >= job->embd_inp.size() || job->embd_inp[prompt_index] != job->session_tokens[n_preserve + i]) {
								shifted_matches = false;
								break;
							}
						}
						if (shifted_matches) {
							// We can reuse the whole session_tokens.
							n_matching_session_tokens = job->session_tokens.size();
#ifdef DEBUG
							std::cout << "[INFERENCE] [KV] Matched preserved prefix and shifted suffix." << std::endl;
#endif
						}
						else {
							// If shifted part doesn't match, fall back to matching as much as possible.
							for (size_t i = 0; i < job->embd_inp.size() && i < job->session_tokens.size(); i++) {
								if (job->embd_inp[i] != job->session_tokens[i])
									break;
								n_matching_session_tokens++;
							}
						}
					}
				}

#ifdef DEBUG
				if (n_matching_session_tokens == job->embd_inp.size()) {
					std::cout << "[INFERENCE] Session file has an exact match for the prompt." << std::endl;
				}
				else if (n_matching_session_tokens < (job->embd_inp.size() / 2)) {
					std::cout << "[INFERENCE] Session file has low similarity to prompt ("
						<< n_matching_session_tokens << " / " << job->embd_inp.size()
						<< "); will mostly be re-evaluated." << std::endl;
				}
				else {
					std::cout << "[INFERENCE] Session file matches "
						<< n_matching_session_tokens << " / "
						<< job->embd_inp.size() << " tokens of prompt." << std::endl;
				}
#endif

				if (job->session_tokens.size() > job->embd_inp.size() && n_matching_session_tokens > 0)
				{
#ifdef DEBUG
					std::cout << "[INFERENCE] [KV] Reevalueating the last token" << std::endl;
#endif
					--n_matching_session_tokens; // always force to re-evaluate the last token
				}

#ifdef DEBUG
				printf("[INFERENCE] [KV] removed %d tokens from the cache\n", (int)(job->session_tokens.size() - n_matching_session_tokens));
#endif

				// Remove any "future" tokens that don’t match
				// i.e. we only keep the portion that matched
				auto * mem = llama_get_memory(context);
				llama_memory_seq_rm(mem, job->seqId, static_cast<llama_pos>(n_matching_session_tokens), -1 /*up to end*/);
				job->session_tokens.resize(n_matching_session_tokens);

#ifdef DEBUG
				printf("[INFERENCE] [KV] tokens decoded: %s\n", tokenizer->detokenize(job->session_tokens).c_str());
#endif
			}

			return n_matching_session_tokens;
		}

		void kv_cache_seq_ltrim(llama_context* context, int n_keep, std::vector<llama_token>& session_tokens, int& n_past, const int id) {
			if (n_past <= n_keep) {
				return;
			}

			int n_left = n_past - n_keep;
			int n_discard = n_left / 2;
			if (n_discard <= 0) {
				return;
			}

#if DEBUG
			std::cout << "\n\nContext full, shifting: n_past = " << n_past
				<< ", n_left = " << n_left
				<< ", n_keep = " << n_keep
				<< ", n_discard = " << n_discard << std::endl << std::endl;
#endif

			auto * mem = llama_get_memory(context);
			llama_memory_seq_rm(mem, id, n_keep, n_keep + n_discard);
			llama_memory_seq_add(mem, id, n_keep + n_discard, n_past, -n_discard);

			n_past -= n_discard;

			if (!session_tokens.empty() && session_tokens.size() >= (size_t)(n_keep + n_discard)) {
				session_tokens.erase(session_tokens.begin() + n_keep,
					session_tokens.begin() + n_keep + n_discard);
			}
		}
	};
	// EmbeddingInferenceService (Optimized for Embedding Models)
	class EmbeddingInferenceService : public InferenceService
	{
	public:
		EmbeddingInferenceService(std::shared_ptr<Tokenizer> tokenizer, llama_model *model, llama_context *context,
								 common_params params, ggml_threadpool *threadpool)
			: tokenizer(std::move(tokenizer)), model(model), context(context), g_params(params), threadpool(threadpool),
			  n_batch(params.n_batch), n_ctx(llama_n_ctx(context)), embeddings_enabled(true)
		{
#ifdef DEBUG
			std::cout << "[INFERENCE] [EMBEDDING] Initializing EmbeddingInferenceService with batch size: " << g_params.n_batch << std::endl;
#endif

			// Always enable embeddings for this service
			llama_set_embeddings(context, true);

			batch = llama_batch_init(params.n_ctx, 0, params.n_parallel);

			std::thread inferenceThread(&EmbeddingInferenceService::start, this);
			inferenceThread.detach();
		}

		~EmbeddingInferenceService()
		{
			stop();

			// Wait for any remaining jobs to complete
			auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
			while (std::chrono::steady_clock::now() < timeout)
			{
				{
					std::lock_guard<std::mutex> lock(mtx);
					if (jobs.empty())
						break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			// Clean up in proper order
			llama_detach_threadpool(context);
			llama_batch_free(batch);
			llama_free(context);
			llama_model_free(model);
			ggml_threadpool_free(threadpool);
		}

		void stop() override
		{
			should_terminate = true;
			cv.notify_all();
		}

		void start() override
		{
			while (!should_terminate)
			{
				std::vector<std::shared_ptr<Job>> current_jobs;
				{
					std::unique_lock<std::mutex> lock(mtx);
					if (jobs.empty())
					{
						cv.wait(lock, [this] { return !jobs.empty() || should_terminate; });
					}
					if (should_terminate)
						break;
					current_jobs = jobs;
				}

				// Process embedding jobs in batches for efficiency
				processBatchEmbeddings(current_jobs);
			}
		}

		void submitJob(const CompletionParameters &params, std::shared_ptr<Job> job) override
		{
			// Not supported for embedding service
			std::lock_guard<std::mutex> jobLock(job->mtx);
			job->hasError = true;
			job->errorMessage = "Text completion not supported by embedding service";
			job->cv.notify_all();
		}

		void complete(const CompletionParameters &params, std::shared_ptr<Job> job) override
		{
			// Not supported for embedding service
			submitJob(params, job);
		}

		void embed(const EmbeddingParameters &params, std::shared_ptr<Job> job) override
		{
#ifdef DEBUG
			std::cout << "[INFERENCE] [EMBEDDING] Submitting embedding job" << std::endl;
#endif

			if (!params.isValid())
			{
				std::lock_guard<std::mutex> jobLock(job->mtx);
				job->hasError = true;
				job->errorMessage = "Invalid embedding parameters";
				job->cv.notify_all();
				return;
			}

			job->params_embedding = params;

			{
				std::lock_guard<std::mutex> lock(mtx);
				jobs.push_back(job);
			}

			cv.notify_one();
		}

		CompletionParameters formatChat(const ChatCompletionParameters &params) override
		{
			// Not supported for embedding service
			throw std::runtime_error("Chat completion not supported by embedding service");
		}

	private:
		std::shared_ptr<Tokenizer> tokenizer;
		struct llama_model *model;
		struct llama_context *context;
		std::mutex mtx;
		std::condition_variable cv;
		common_params g_params;
		ggml_threadpool *threadpool;
		llama_batch batch;
		std::vector<std::shared_ptr<Job>> jobs;
		std::atomic<bool> should_terminate{false};
		std::atomic<bool> embeddings_enabled{true};

		const int n_batch;
		const int n_ctx;

		void processBatchEmbeddings(std::vector<std::shared_ptr<Job>>& current_jobs)
		{
			std::vector<std::shared_ptr<Job>> embedding_jobs;
			
			// Filter embedding jobs
			for (auto& job : current_jobs)
			{
				std::lock_guard<std::mutex> jobLock(job->mtx);
				if (!job->isFinished && !job->hasError)
				{
					embedding_jobs.push_back(job);
				}
			}

			if (embedding_jobs.empty())
			{
				return;
			}

			try
			{
				// Process embeddings in batch for efficiency
				processBatch(embedding_jobs);
			}
			catch (const std::exception& e)
			{
				// Mark all jobs as failed
				for (auto& job : embedding_jobs)
				{
					std::lock_guard<std::mutex> jobLock(job->mtx);
					job->hasError = true;
					job->errorMessage = std::string("Batch embedding failed: ") + e.what();
					job->isFinished = true;
					job->cv.notify_all();
				}
			}

			// Remove completed jobs
			{
				std::lock_guard<std::mutex> lock(mtx);
				jobs.erase(std::remove_if(jobs.begin(), jobs.end(),
					[](const std::shared_ptr<Job>& job) {
						std::lock_guard<std::mutex> jobLock(job->mtx);
						return job->isFinished || job->hasError;
					}), jobs.end());
			}
		}

		void processBatch(std::vector<std::shared_ptr<Job>>& embedding_jobs)
		{
			// Clear batch and KV cache
			common_batch_clear(batch);
			auto * mem = llama_get_memory(context);
			llama_memory_clear(mem, /*data=*/true);

			std::vector<std::vector<llama_token>> all_tokens;
			std::vector<int> job_indices;

			// Tokenize all inputs
			for (size_t i = 0; i < embedding_jobs.size(); ++i)
			{
				auto& job = embedding_jobs[i];
				
				try
				{
					std::string input;
					bool normalize = true;
					{
						std::lock_guard<std::mutex> jobLock(job->mtx);
						input = job->params_embedding.input;
						normalize = job->params_embedding.normalize;
					}

					// Tokenize input
					std::vector<llama_token> tokens = common_tokenize(context, input, 
						llama_vocab_get_add_bos(llama_model_get_vocab(model)), false);
					
					if (tokens.empty())
					{
						std::lock_guard<std::mutex> jobLock(job->mtx);
						job->hasError = true;
						job->errorMessage = "Input text resulted in empty tokens";
						job->isFinished = true;
						job->cv.notify_all();
						continue;
					}

					// Check token limit
					const int max_tokens = std::min(n_ctx / static_cast<int>(embedding_jobs.size()) - 4, 512);
					if (static_cast<int>(tokens.size()) > max_tokens)
					{
						tokens.resize(max_tokens);
					}

					{
						// store token count for result usage
						std::lock_guard<std::mutex> jobLock(job->mtx);
						job->embedding_token_count = static_cast<int>(tokens.size());
					}
					all_tokens.push_back(tokens);
					job_indices.push_back(static_cast<int>(i));

				}
				catch (const std::exception& e)
				{
					std::lock_guard<std::mutex> jobLock(job->mtx);
					job->hasError = true;
					job->errorMessage = std::string("Tokenization failed: ") + e.what();
					job->isFinished = true;
					job->cv.notify_all();
				}
			}

			if (all_tokens.empty())
			{
				return;
			}

			// Add tokens to batch with different sequence IDs
			for (size_t seq = 0; seq < all_tokens.size(); ++seq)
			{
				const auto& tokens = all_tokens[seq];
				for (size_t i = 0; i < tokens.size(); ++i)
				{
					if (batch.n_tokens >= n_batch)
					{
						break; // Batch is full
					}

					batch.token[batch.n_tokens] = tokens[i];
					batch.pos[batch.n_tokens] = static_cast<llama_pos>(i);
					batch.n_seq_id[batch.n_tokens] = 1;
					batch.seq_id[batch.n_tokens][0] = static_cast<llama_seq_id>(seq);
					// Set logits for the last token of each sequence
					batch.logits[batch.n_tokens] = (i == tokens.size() - 1) ? true : false;
					batch.n_tokens++;
				}
			}

			// Decode the batch
			if (llama_decode(context, batch) != 0)
			{
				throw std::runtime_error("Failed to decode embedding batch");
			}

			// Extract embeddings for each sequence
			const int n_embd = llama_model_n_embd(model);
			const enum llama_pooling_type pooling_type = llama_pooling_type(context);

			for (size_t seq = 0; seq < job_indices.size(); ++seq)
			{
				auto& job = embedding_jobs[job_indices[seq]];
				
				try
				{
					bool normalize;
					{
						std::lock_guard<std::mutex> jobLock(job->mtx);
						normalize = job->params_embedding.normalize;
					}

					float* embd = nullptr;
					
					if (pooling_type == LLAMA_POOLING_TYPE_NONE)
					{
						// Find the last token position for this sequence
						int last_token_pos = -1;
						for (int i = 0; i < batch.n_tokens; ++i)
						{
							if (batch.seq_id[i][0] == static_cast<llama_seq_id>(seq) && batch.logits[i])
							{
								last_token_pos = i;
								break;
							}
						}
						
						if (last_token_pos >= 0)
						{
							embd = llama_get_embeddings_ith(context, last_token_pos);
						}
					}
					else
					{
						embd = llama_get_embeddings_seq(context, static_cast<llama_seq_id>(seq));
					}

					if (!embd)
					{
						std::lock_guard<std::mutex> jobLock(job->mtx);
						job->hasError = true;
						job->errorMessage = "Failed to get embeddings from model";
						job->isFinished = true;
						job->cv.notify_all();
						continue;
					}

					// Copy and normalize embedding
					std::vector<float> embedding(embd, embd + n_embd);
					
					if (normalize)
					{
						float norm = 0.0f;
						for (float val : embedding)
						{
							norm += val * val;
						}
						norm = std::sqrt(norm);
						
						if (norm > 1e-8f)
						{
							for (float& val : embedding)
							{
								val /= norm;
							}
						}
					}

					{
						std::lock_guard<std::mutex> jobLock(job->mtx);
						job->embedding = embedding;
						job->isFinished = true;
						job->cv.notify_all();
					}

#ifdef DEBUG
					std::cout << "[INFERENCE] [EMBEDDING] Generated " << n_embd << "-dimensional embedding for sequence " << seq << std::endl;
#endif

				}
				catch (const std::exception& e)
				{
					std::lock_guard<std::mutex> jobLock(job->mtx);
					job->hasError = true;
					job->errorMessage = std::string("Embedding extraction failed: ") + e.what();
					job->isFinished = true;
					job->cv.notify_all();
				}
			}
		}
	};
} // namespace

// Define the Impl struct for the PIMPL pattern
struct InferenceEngine::Impl
{
	std::unique_ptr<InferenceService> inferenceService;

	// Job management members
	std::atomic<int> nextJobId{ 0 };
	std::unordered_map<int, std::shared_ptr<Job>> jobs;
	std::mutex jobsMutex;

	ThreadPool threadPool;

	Impl(const char *modelPath, LoadingParameters lParams, const int mainGpuId = 0, bool isEmbeddingModel = false);
	~Impl();

	// Inline method implementations to avoid scope resolution issues
	int submitCompletionsJob(const CompletionParameters &params) {
		int jobId = nextJobId++;

		auto job = std::make_shared<Job>();
		job->jobId = jobId;
		job->seqId = params.seqId;
		job->start_time = std::chrono::steady_clock::now();

		try {
			threadPool.enqueue([this, params, job]() {
				try {
					this->inferenceService->complete(params, job);
				}
				catch (const std::exception& e) {
					std::lock_guard<std::mutex> lock(job->mtx);
					job->hasError = true;
					job->errorMessage = e.what();
				}
			});
		}
		catch (const std::exception& e) {
			std::cerr << "[INFERENCE] [ERROR] " << e.what() << std::endl;
			return -1;
		}

		{
			std::lock_guard<std::mutex> lock(jobsMutex);
			jobs.emplace(jobId, job);
		}

		return jobId;
	}

	int submitChatCompletionsJob(const ChatCompletionParameters &params) {
		int jobId = nextJobId++;

		auto job = std::make_shared<Job>();
		job->jobId = jobId;
		job->seqId = params.seqId;
		job->start_time = std::chrono::steady_clock::now();

#ifdef DEBUG
		std::cout << "[INFERENCE] Submitting chat completions job to queue" << std::endl;
#endif

		try {
			threadPool.enqueue([this, params, job]() {
				try {
#ifdef DEBUG
					std::cout << "[INFERENCE] Processing completion task to engine" << std::endl;
#endif
					this->inferenceService->complete(this->inferenceService->formatChat(params), job);
				}
				catch (const std::exception& e) {
					std::lock_guard<std::mutex> lock(job->mtx);
					job->hasError = true;
					job->errorMessage = e.what();
					std::cerr << "[INFERENCE] [ERROR] [submitChatCompletionsJob] " << e.what() << "\n" << std::endl;
				}
			});
		}
		catch (const std::exception& e) {
			std::cerr << "[INFERENCE] [ERROR] [submitChatCompletionsJob] " << e.what() << std::endl;
			return -1;
		}

		{
			std::lock_guard<std::mutex> lock(jobsMutex);
			jobs.emplace(jobId, job);
		}

		return jobId;
	}

	int submitEmbeddingJob(const EmbeddingParameters &params) {
		int jobId = nextJobId++;

		auto job = std::make_shared<Job>();
		job->jobId = jobId;
		job->seqId = params.seqId;

#ifdef DEBUG
		std::cout << "[INFERENCE] Submitting embedding job to queue" << std::endl;
#endif

		try {
			threadPool.enqueue([this, params, job]() {
				try {
#ifdef DEBUG
					std::cout << "[INFERENCE] Processing embedding task to engine" << std::endl;
#endif
					this->inferenceService->embed(params, job);
				}
				catch (const std::exception& e) {
					std::lock_guard<std::mutex> lock(job->mtx);
					job->hasError = true;
					job->errorMessage = e.what();
					std::cerr << "[INFERENCE] [ERROR] [submitEmbeddingJob] " << e.what() << "\n" << std::endl;
				} 
			});
		}
		catch (const std::exception& e) {
			std::cerr << "[INFERENCE] [ERROR] [submitEmbeddingJob] " << e.what() << std::endl;
			return -1;
		}

		{
			std::lock_guard<std::mutex> lock(jobsMutex);
			jobs.emplace(jobId, job);
		}

		return jobId;
	}

	void stopJob(int job_id);
	bool isJobFinished(int job_id);
	CompletionResult getJobResult(int job_id);
	EmbeddingResult getEmbeddingResult(int job_id);
	void waitForJob(int job_id);
	bool hasJobError(int job_id);
	std::string getJobError(int job_id);
	bool hasActiveJobs();
};

InferenceEngine::Impl::Impl(const char *modelPath, const LoadingParameters lParams, const int mainGpuId, bool isEmbeddingModel)
	: threadPool(lParams.n_parallel)
{
#ifndef DEBUG
	llama_log_set(llama_log_callback_null, NULL);
#endif

	std::filesystem::path tokenizer_model_path(modelPath);
	
	// Verify the file exists and has .gguf extension
	if (!std::filesystem::exists(tokenizer_model_path))
	{
		throw std::runtime_error("[INFERENCE] [ERROR] Model file not found: " + tokenizer_model_path.string());
	}
	
	if (tokenizer_model_path.extension() != ".gguf")
	{
		throw std::runtime_error("[INFERENCE] [ERROR] Invalid model file extension. Expected .gguf, got: " + tokenizer_model_path.extension().string());
	}

	unsigned int inferenceThreads = 16;

#ifdef DEBUG
	std::cout << "[INFERENCE] Inference threads: " << inferenceThreads << std::endl;
#endif

	common_params_model params_model;
	params_model.path = tokenizer_model_path.string().c_str();

	common_params params;
	params.model						= params_model;
	
	// Core model parameters
	params.n_ctx						= lParams.n_ctx;
	params.n_batch						= lParams.n_batch;
	params.n_ubatch						= lParams.n_ubatch;
	params.n_keep						= lParams.n_keep;
	params.n_chunks						= lParams.n_chunks;
	params.n_parallel					= lParams.n_parallel;
	params.n_sequences					= lParams.n_sequences;
	params.grp_attn_n					= lParams.grp_attn_n;
	params.grp_attn_w					= lParams.grp_attn_w;
	params.n_print						= lParams.n_print;
	
	// RoPE parameters
	params.rope_freq_base				= lParams.rope_freq_base;
	params.rope_freq_scale				= lParams.rope_freq_scale;
	params.yarn_ext_factor				= lParams.yarn_ext_factor;
	params.yarn_attn_factor				= lParams.yarn_attn_factor;
	params.yarn_beta_fast				= lParams.yarn_beta_fast;
	params.yarn_beta_slow				= lParams.yarn_beta_slow;
	params.yarn_orig_ctx				= lParams.yarn_orig_ctx;
	params.defrag_thold					= lParams.defrag_thold;
	
	// Hardware acceleration and GPU parameters
	params.n_gpu_layers					= lParams.n_gpu_layers;
	params.main_gpu						= lParams.main_gpu;
	// Copy tensor_split array
	for (int i = 0; i < 128; i++) {
		params.tensor_split[i] = lParams.tensor_split[i];
	}
	params.split_mode					= static_cast<llama_split_mode>(lParams.split_mode);
	
	// Memory management
	params.use_mmap						= lParams.use_mmap;
	params.use_mlock					= lParams.use_mlock;
	params.no_kv_offload				= lParams.no_kv_offload;
	params.no_op_offload				= lParams.no_op_offload;
	params.no_extra_bufts				= lParams.no_extra_bufts;
	
	// Processing and performance settings
	params.cont_batching				= lParams.cont_batching;
	params.flash_attn					= lParams.flash_attn;
	params.warmup						= lParams.warmup;
	params.check_tensors				= lParams.check_tensors;
	params.swa_full						= lParams.swa_full;
	params.kv_unified					= lParams.kv_unified;
	params.ctx_shift					= lParams.ctx_shift;
	
	// Cache data types
	params.cache_type_k					= static_cast<ggml_type>(lParams.cache_type_k);
	params.cache_type_v					= static_cast<ggml_type>(lParams.cache_type_v);
	
	// Rope scaling and pooling types
	params.rope_scaling_type			= static_cast<llama_rope_scaling_type>(lParams.rope_scaling_type);
	params.pooling_type					= static_cast<enum llama_pooling_type>(lParams.pooling_type);
	params.attention_type				= static_cast<llama_attention_type>(lParams.attention_type);
	
	// NUMA strategy
	params.numa							= static_cast<ggml_numa_strategy>(lParams.numa);
	
	// CPU parameters
	params.cpuparams.n_threads			= lParams.cpuparams.n_threads == -1 ? inferenceThreads : lParams.cpuparams.n_threads;
	for (int i = 0; i < 128; i++) {
		params.cpuparams.cpumask[i] = lParams.cpuparams.cpumask[i];
	}
	params.cpuparams.mask_valid			= lParams.cpuparams.mask_valid;
	params.cpuparams.priority			= static_cast<ggml_sched_priority>(lParams.cpuparams.priority);
	params.cpuparams.strict_cpu			= lParams.cpuparams.strict_cpu;
	params.cpuparams.poll				= lParams.cpuparams.poll;
	
	// Copy batch CPU parameters
	params.cpuparams_batch				= params.cpuparams; // Use same as main CPU params
	
	// Embedding parameters
	params.embedding					= lParams.embedding;
	params.embd_normalize				= lParams.embd_normalize;
	
	// Model and adapter parameters
	params.model_alias					= lParams.model_alias;
	params.hf_token						= lParams.hf_token;
	params.lora_init_without_apply		= lParams.lora_init_without_apply;
	
	// Control vectors
	params.control_vector_layer_start	= lParams.control_vector_layer_start;
	params.control_vector_layer_end		= lParams.control_vector_layer_end;
	
	// Server/API parameters
	params.verbosity					= lParams.verbosity;
	params.offline						= lParams.offline;
	
	// Multimodal parameters
	if (!lParams.mmproj_path.empty()) {
		params.mmproj.path = lParams.mmproj_path;
	}
	params.mmproj_use_gpu				= lParams.mmproj_use_gpu;
	params.no_mmproj					= lParams.no_mmproj;
	
	// Chat template parameters
	params.chat_template				= lParams.chat_template;
	params.use_jinja					= lParams.use_jinja;
	params.enable_chat_template			= lParams.enable_chat_template;
	
	// Input/output formatting
	params.input_prefix_bos				= lParams.input_prefix_bos;
	params.escape						= lParams.escape;
	params.special						= lParams.special;
	
	// Performance and debugging
	params.no_perf						= lParams.no_perf;
	params.verbose_prompt				= lParams.verbose_prompt;
	params.display_prompt				= lParams.display_prompt;
	
	// Conversation mode
	params.conversation_mode			= static_cast<common_conversation_mode>(lParams.conversation_mode);
	
	// Advanced parameters
	params.lookup_cache_static			= lParams.lookup_cache_static;
	params.lookup_cache_dynamic			= lParams.lookup_cache_dynamic;
	params.logits_file					= lParams.logits_file;
	
	// Perplexity calculation parameters
	params.ppl_stride					= lParams.ppl_stride;
	params.ppl_output_type				= lParams.ppl_output_type;
	
	// Evaluation parameters
	params.hellaswag					= lParams.hellaswag;
	params.hellaswag_tasks				= lParams.hellaswag_tasks;
	params.winogrande					= lParams.winogrande;
	params.winogrande_tasks				= lParams.winogrande_tasks;
	params.multiple_choice				= lParams.multiple_choice;
	params.multiple_choice_tasks		= lParams.multiple_choice_tasks;
	params.kl_divergence				= lParams.kl_divergence;
	
	// Server-specific parameters
	params.port							= lParams.port;
	params.timeout_read					= lParams.timeout_read;
	params.timeout_write				= lParams.timeout_write;
	params.n_threads_http				= lParams.n_threads_http;
	params.n_cache_reuse				= lParams.n_cache_reuse;
	params.n_swa_checkpoints			= lParams.n_swa_checkpoints;
	params.hostname						= lParams.hostname;
	params.public_path					= lParams.public_path;
	params.api_prefix					= lParams.api_prefix;
	
	// SSL parameters
	params.ssl_file_key					= lParams.ssl_file_key;
	params.ssl_file_cert				= lParams.ssl_file_cert;
	
	// Advanced server features
	params.webui						= lParams.webui;
	params.endpoint_slots				= lParams.endpoint_slots;
	params.endpoint_props				= lParams.endpoint_props;
	params.endpoint_metrics				= lParams.endpoint_metrics;
	params.log_json						= lParams.log_json;
	params.slot_save_path				= lParams.slot_save_path;
	params.slot_prompt_similarity		= lParams.slot_prompt_similarity;
	
	// Reasoning parameters
	params.reasoning_format				= static_cast<common_reasoning_format>(lParams.reasoning_format);
	params.reasoning_budget				= lParams.reasoning_budget;
	params.prefill_assistant			= lParams.prefill_assistant;
	
	// Batch benchmark parameters
	params.is_pp_shared					= lParams.is_pp_shared;
	params.n_pp							= lParams.n_pp;
	params.n_tg							= lParams.n_tg;
	params.n_pl							= lParams.n_pl;
	
	// Context and retrieval parameters
	params.context_files				= lParams.context_files;
	params.chunk_size					= lParams.chunk_size;
	params.chunk_separator				= lParams.chunk_separator;
	
	// Passkey parameters
	params.n_junk						= lParams.n_junk;
	params.i_pos						= lParams.i_pos;
	
	// iMatrix parameters
	params.n_out_freq					= lParams.n_out_freq;
	params.n_save_freq					= lParams.n_save_freq;
	params.i_chunk						= lParams.i_chunk;
	params.imat_dat						= lParams.imat_dat;
	params.process_output				= lParams.process_output;
	params.compute_ppl					= lParams.compute_ppl;
	params.show_statistics				= lParams.show_statistics;
	params.parse_special				= lParams.parse_special;
	
	// Control vector generator parameters
	params.n_pca_batch					= lParams.n_pca_batch;
	params.n_pca_iterations				= lParams.n_pca_iterations;
	params.cvector_dimre_method			= static_cast<dimre_method>(lParams.cvector_dimre_method);
	params.cvector_positive_file		= lParams.cvector_positive_file;
	params.cvector_negative_file		= lParams.cvector_negative_file;
	
	// Infill parameters
	params.spm_infill					= lParams.spm_infill;
	
	// Batch benchmark output
	params.batched_bench_output_jsonl	= lParams.batched_bench_output_jsonl;
	
	// Override some defaults for server usage
	params.single_turn					= true;
#if defined(USE_CUDA) || defined(USE_VULKAN)
	std::cout << "[INFERENCE] Using CUDA or Vulkan" << std::endl;
	// GPU parameters are already set above from lParams
#endif

#ifndef USE_VULKAN
	// Only enable flash attention if not using Vulkan
	if (!lParams.flash_attn) {
		params.flash_attn = true; // Enable by default for non-Vulkan builds
	}
#endif

#ifdef DEBUG
	std::cout << "[INFERENCE] Using main GPU ID: " << params.main_gpu << std::endl;
#endif

	llama_backend_init();
	llama_numa_init(params.numa);

#ifdef DEBUG
	std::cout << "[INFERENCE] Loading model from " << tokenizer_model_path << std::endl;
#endif

	common_init_result	llama_init	= common_init_from_params(params);
	llama_model			*model		= llama_init.model.release();
	llama_context		*ctx		= llama_init.context.release();

	// Validate model and context initialization
	if (!model) {
		throw std::runtime_error("[INFERENCE] [ERROR] Failed to load model - model is null");
	}
	
	if (!ctx) {
		llama_model_free(model);
		throw std::runtime_error("[INFERENCE] [ERROR] Failed to create context - context is null");
	}
	
	// Validate model dimensions to prevent tensor assertion failures
	const struct llama_vocab * vocab = llama_model_get_vocab(model);
	int n_vocab = llama_vocab_n_tokens(vocab);
	int n_embd = llama_model_n_embd(model);
	int n_ctx_train = llama_model_n_ctx_train(model);
	
	if (n_vocab <= 0 || n_embd <= 0 || n_ctx_train <= 0) {
		llama_free(ctx);
		llama_model_free(model);
		throw std::runtime_error("[INFERENCE] [ERROR] Invalid model dimensions - vocab:" + 
			std::to_string(n_vocab) + " embd:" + std::to_string(n_embd) + 
			" ctx_train:" + std::to_string(n_ctx_train));
	}
	
	// Ensure context size doesn't exceed model training context
	if (params.n_ctx > n_ctx_train) {
		std::cout << "[INFERENCE] [WARNING] Requested context size (" << params.n_ctx 
			<< ") exceeds model training context (" << n_ctx_train 
			<< "). Adjusting to model maximum." << std::endl;
		// Note: This might require recreating the context with adjusted parameters
	}

#ifdef DEBUG
	std::cout << "[INFERENCE] Model validation successful - vocab:" << n_vocab 
		<< " embd:" << n_embd << " ctx_train:" << n_ctx_train << std::endl;
#endif

	struct ggml_threadpool_params threadpool_params;
	ggml_threadpool_params_init(&threadpool_params, inferenceThreads);
	threadpool_params.prio = GGML_SCHED_PRIO_NORMAL;
	set_process_priority(GGML_SCHED_PRIO_NORMAL);
	struct ggml_threadpool* threadpool = ggml_threadpool_new(&threadpool_params);
	llama_attach_threadpool(ctx, threadpool, nullptr);

	// Create the tokenizer
	auto tokenizer = std::make_shared<Tokenizer>(model, ctx, params);
	if (!tokenizer)
	{
		ggml_threadpool_free(threadpool);
		llama_free(ctx);
		llama_model_free(model);
		throw std::runtime_error("[INFERENCE] [ERROR] Failed to create tokenizer.");
	}
	// Create the inference service
	try
	{
		if (isEmbeddingModel)
		{
			// For embedding models, use the specialized embedding service
			params.embedding = true;
			inferenceService = std::make_unique<EmbeddingInferenceService>(tokenizer, model, ctx, params, threadpool);
		}
		else
		{
			// For LLM models, use the regular inference service
			inferenceService = std::make_unique<LlamaInferenceService>(tokenizer, model, ctx, params, threadpool);
		}
	}
	catch (const std::exception &e)
	{
		ggml_threadpool_free(threadpool);
		llama_free(ctx);
		llama_model_free(model);
		throw std::runtime_error("[INFERENCE] [ERROR] Failed to create inference service: " + std::string(e.what()));
	}
}

void InferenceEngine::Impl::stopJob(int job_id)
{
	std::shared_ptr<Job> jobToStop;

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		auto it = jobs.find(job_id);
		if (it == jobs.end()) {
			return;  // Job not found
		}
		jobToStop = it->second;
	}

	jobToStop->cancelRequested.store(true);

	{
		std::lock_guard<std::mutex> jobLock(jobToStop->mtx);
		jobToStop->cv.notify_all();
	}
}

bool InferenceEngine::Impl::isJobFinished(int job_id)
{
	std::shared_ptr<Job> job;

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		auto it = jobs.find(job_id);
		if (it == jobs.end())
		{
			std::cerr << "[INFERENCE] [ERROR] [isJobFinished] Invalid job ID: " << job_id << "\n" << std::endl;
			return true;
		}
		job = it->second;
	}

	std::lock_guard<std::mutex> jobLock(job->mtx);
	bool isFinished = job->isFinished;
	
	// Don't remove the job here - let the caller get the results first
	// Jobs will be cleaned up when they are accessed for results or errors
	
	return isFinished;
}

CompletionResult InferenceEngine::Impl::getJobResult(int job_id)
{
	std::shared_ptr<Job> job;

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		auto it = jobs.find(job_id);
		if (it == jobs.end()) 
		{
			std::cerr << "[INFERENCE] [ERROR] [getJobResult] Invalid job ID " << job_id << "\n" << std::endl;
			CompletionResult result;
			result.tokens = {};
			result.text = "";
			result.tps = 0.0F;
			result.ttft = 0.0F;
			return result;
		}
		job = it->second;
	}

	std::lock_guard<std::mutex> jobLock(job->mtx);
	CompletionResult result;
	result.tokens = job->generatedTokens;
	result.text = job->generatedText;
	result.tps = job->tps;
	result.ttft = job->ttft;
	result.prompt_token_count = job->n_prompt;
	return result;
}

EmbeddingResult InferenceEngine::Impl::getEmbeddingResult(int job_id)
{
	std::shared_ptr<Job> job;

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		auto it = jobs.find(job_id);
		if (it == jobs.end())
		{
			std::cerr << "[INFERENCE] [ERROR] [getEmbeddingResult] Invalid job ID " << job_id << "\n"
					  << std::endl;
			EmbeddingResult result;
			result.embedding = {};
			return result;
		}
		job = it->second;
	}

	std::lock_guard<std::mutex> jobLock(job->mtx);
	EmbeddingResult result;
	result.embedding = job->embedding;
	result.tokens_count = job->embedding_token_count;
	
	// Clean up the job after getting the result
	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		jobs.erase(job_id);
	}
	
	return result;
}

void InferenceEngine::Impl::waitForJob(int job_id)
{
	std::shared_ptr<Job> job;

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		auto it = jobs.find(job_id);
		if (it == jobs.end()) 
		{
			std::cerr << "[INFERENCE] [ERROR] [waitForJob] Invalid job ID " << job_id << "\n";
			return;
		}
		job = it->second;
	}

	std::unique_lock<std::mutex> jobLock(job->mtx);
	job->cv.wait(jobLock, [&job]() { return job->isFinished || job->hasError; });
}

bool InferenceEngine::Impl::hasJobError(int job_id)
{
	std::shared_ptr<Job> job;

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		auto it = jobs.find(job_id);
		if (it == jobs.end()) 
		{
			std::cerr << "[INFERENCE] [ERROR] [hasJobError] Invalid job ID " << job_id << "\n" << std::endl;
			return false;
		}
		job = it->second;
	}

	std::lock_guard<std::mutex> jobLock(job->mtx);
	return job->hasError;
}

std::string InferenceEngine::Impl::getJobError(int job_id)
{
	std::shared_ptr<Job> job;

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		auto it = jobs.find(job_id);
		if (it == jobs.end()) 
		{
			std::cerr << "[INFERENCE] [ERROR] [getJobError] Invalid job ID " << job_id << "\n" << std::endl;
			return "";
		}
		job = it->second;
	}

	std::lock_guard<std::mutex> jobLock(job->mtx);
	return job->errorMessage;
}

bool InferenceEngine::Impl::hasActiveJobs()
{
	std::lock_guard<std::mutex> lock(jobsMutex);
	
	for (const auto& [jobId, job] : jobs) {
		std::lock_guard<std::mutex> jobLock(job->mtx);
		if (!job->isFinished && !job->hasError) {
			return true;
		}
	}
	
	return false;
}

InferenceEngine::Impl::~Impl()
{
	threadPool.shutdown();
	jobs.clear();
	llama_backend_free();

	inferenceService.reset();
}

INFERENCE_API InferenceEngine::InferenceEngine()
	: pimpl(nullptr)
{
}

INFERENCE_API bool InferenceEngine::loadModel(const char* modelPath, const LoadingParameters lParams, const int mainGpuId)
{
#ifdef DEBUG
	std::cout << "[INFERENCE] Loading LLM model from " << modelPath << std::endl;
#endif
	this->pimpl.reset();

	try
	{
		this->pimpl = std::make_unique<Impl>(modelPath, lParams, mainGpuId, false);
	}
	catch (const std::exception& e) {
		std::cerr << "[INFERENCE] [ERROR] Could not load model from: " << modelPath << "\nError: " << e.what() << "\n" << std::endl;
		return false;
	}
	return true;
}

INFERENCE_API bool InferenceEngine::loadEmbeddingModel(const char *modelPath, const LoadingParameters lParams, const int mainGpuId)
{
#ifdef DEBUG
	std::cout << "[INFERENCE] Loading embedding model from " << modelPath << std::endl;
#endif
	this->pimpl.reset();

	try
	{
		this->pimpl = std::make_unique<Impl>(modelPath, lParams, mainGpuId, true);
	}
	catch (const std::exception &e)
	{
		std::cerr << "[INFERENCE] [ERROR] Could not load embedding model from: " << modelPath << "\nError: " << e.what() << "\n"
				  << std::endl;
		return false;
	}
	return true;
}

INFERENCE_API bool InferenceEngine::unloadModel()
{
	if (!this->pimpl)
	{
		std::cerr << "[INFERENCE] [ERROR] Model not loaded\n" << std::endl;
		return false;
	}

	this->pimpl.reset();
	return true;
}

INFERENCE_API int InferenceEngine::submitCompletionsJob(const CompletionParameters& params)
{
#ifdef DEBUG
	std::cout << "[INFERENCE] Submitting completions job" << std::endl;
#endif

	return pimpl->submitCompletionsJob(params);
}

INFERENCE_API int InferenceEngine::submitChatCompletionsJob(const ChatCompletionParameters& params)
{
#ifdef DEBUG
	std::cout << "[INFERENCE] Submitting chat completions job" << std::endl;
#endif

	return pimpl->submitChatCompletionsJob(params);
}

INFERENCE_API int InferenceEngine::submitEmbeddingJob(const EmbeddingParameters &params)
{
#ifdef DEBUG
	std::cout << "[INFERENCE] Submitting embedding job" << std::endl;
#endif

	return pimpl->submitEmbeddingJob(params);
}

INFERENCE_API void InferenceEngine::stopJob(int job_id)
{
	pimpl->stopJob(job_id);
}

INFERENCE_API bool InferenceEngine::isJobFinished(int job_id)
{
	return pimpl->isJobFinished(job_id);
}

INFERENCE_API CompletionResult InferenceEngine::getJobResult(int job_id)
{
	return pimpl->getJobResult(job_id);
}

INFERENCE_API EmbeddingResult InferenceEngine::getEmbeddingResult(int job_id)
{
	return pimpl->getEmbeddingResult(job_id);
}

INFERENCE_API void InferenceEngine::waitForJob(int job_id)
{
	pimpl->waitForJob(job_id);
}

INFERENCE_API bool InferenceEngine::hasJobError(int job_id)
{
	return pimpl->hasJobError(job_id);
}

INFERENCE_API std::string InferenceEngine::getJobError(int job_id)
{
	return pimpl->getJobError(job_id);
}

INFERENCE_API bool InferenceEngine::hasActiveJobs()
{
	return pimpl->hasActiveJobs();
}

INFERENCE_API InferenceEngine::~InferenceEngine() = default;

extern "C" INFERENCE_API IInferenceEngine* createInferenceEngine()
{
	return new InferenceEngine();
}

extern "C" INFERENCE_API void destroyInferenceEngine(IInferenceEngine* engine)
{
	delete static_cast<InferenceEngine*>(engine);
}