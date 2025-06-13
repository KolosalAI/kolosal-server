#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <queue>
#include <cstdint>
#include <thread>
#include <condition_variable>
#include <functional>
#include <type_traits>
#ifdef USE_VULKAN
#include <vulkan/vulkan.h>
#endif

#include "llama.h"
#include "common.h"
#include "sampling.h"
#include "inference.h"
#include "chat.h"
#include "json.hpp"
#include "toolcall-client.h"

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

			task = std::move(this->tasks.front());
			this->tasks.pop();
		}

		task();
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
	if (sizeof(prompt) <= 0)
	{
		std::cerr << "[INFERENCE] [ERROR] prompt is empty: " << prompt << std::endl;
		return false;
	}

	if (randomSeed < 0)
	{
		std::cerr << "[INFERENCE] [ERROR] randomSeed is negative: " << randomSeed << std::endl;
		return false;
	}

	if (maxNewTokens < 0 || maxNewTokens > 4096)
	{
		std::cerr << "[INFERENCE] [ERROR] maxNewTokens is out of range: " << maxNewTokens << std::endl;
		return false;
	}

	if (minLength < 0 || minLength > 4096)
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

	if (maxNewTokens < 0 || maxNewTokens > 4096)
	{
		std::cerr << "[INFERENCE] [ERROR] maxNewTokens is out of range: " << maxNewTokens << std::endl;
		return false;
	}

	if (minLength < 0 || minLength > 4096)
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

	return true;
}

// Anonymous namespace to encapsulate internal classes
namespace
{

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
		std::string				applyTemplate(std::vector<common_chat_msg>& messages, toolcall::client::ptr tc_client = nullptr);

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
		add_bos = llama_add_bos_token(vocab);

		chat_templates = common_chat_templates_init(tokenizer_model, params.chat_template);
		try {
			common_chat_format_example(chat_templates.get(), params.use_jinja);
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

	std::string Tokenizer::applyTemplate(std::vector<common_chat_msg>& messages, toolcall::client::ptr tc_client)
	{
		common_chat_templates_inputs inputs;
		inputs.messages = messages;

		if (tc_client)
		{
			inputs.tool_choice = common_chat_tool_choice_parse_oaicompat(tc_client->tool_choice());
			inputs.tools	   = common_chat_tools_parse_oaicompat(tc_client->tool_list());
		}

		return common_chat_templates_apply(chat_templates.get(), inputs).prompt;
	}

	// InferenceService Interface (Internal Use Only)
	class InferenceService
	{
	public:
		virtual ~InferenceService() {}
		virtual void start() = 0;
		virtual void stop() = 0;
		virtual void submitJob(const CompletionParameters& params, std::shared_ptr<Job> job) = 0;
		virtual void complete(const CompletionParameters& params, std::shared_ptr<Job> job) = 0;
		virtual CompletionParameters formatChat(const ChatCompletionParameters& params) = 0;
	};

	// LlamaInferenceService (CPU Implementation)
	class LlamaInferenceService : public InferenceService
	{
	public:
		LlamaInferenceService(std::shared_ptr<Tokenizer> tokenizer, llama_model* model, llama_context* context, 
			common_params params, ggml_threadpool* threadpool)
			: tokenizer(std::move(tokenizer)), model(model), context(context), g_params(params), threadpool(threadpool),
			n_batch(params.n_batch), n_keep(params.n_keep), n_ctx(llama_n_ctx(context)), tc_client(nullptr)
		{
#ifdef DEBUG
			std::cout << "Initializing batch with size of: " << g_params.n_batch << std::endl;
#endif

			batch = llama_batch_init(params.n_ctx, 0, 1);

			std::thread inferenceThread(&LlamaInferenceService::start, this);
			inferenceThread.detach();
		}

		~LlamaInferenceService()
		{
			stop();

			llama_free(context);
			llama_free_model(model);
			llama_batch_free(batch);

			ggml_threadpool_free(threadpool);
		}

		void stop() override
		{
			should_terminate = true;
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

				bool batch_has_tokens = false;

				for (auto job : current_jobs)
				{
					std::lock_guard<std::mutex> jobLock(job->mtx);

					if (job->isFinished || job->hasError)
						continue;

					if (checkCancellation(job) || (job->n_remain <= 0 && job->params.maxNewTokens != 0)) {
						saveSession(job);
						common_sampler_free(job->smpl);
						llama_kv_self_seq_rm(context, job->seqId, -1, -1);
						llama_kv_self_update(context);
						job->isFinished = true;
						job->cv.notify_all();
						continue;
					}

					if (!ensureContextCapacity(job))
					{
						common_sampler_free(job->smpl);
						llama_kv_self_seq_rm(context, job->seqId, -1, -1);
						llama_kv_self_update(context);
						job->hasError = true;
						job->isFinished = true;
						job->errorMessage = "Context overflow even after trimming.";
						job->cv.notify_all();
						continue;
					}

					if (!job->isDecodingPrompt) {
						if (batch.n_tokens >= g_params.n_batch) {
							break;
						}

						if (!sampleNextToken(job)) {
							saveSession(job);
							common_sampler_free(job->smpl);
							llama_kv_self_seq_rm(context, job->seqId, -1, -1);
							llama_kv_self_update(context);
							job->isFinished = true;
							job->cv.notify_all();
							continue;
						}

						job->n_past += 1;
						batch_has_tokens = true;
					}
					else {
						if (!loadSession(job)) {
							common_sampler_free(job->smpl);
							llama_kv_self_seq_rm(context, job->seqId, -1, -1);
							llama_kv_self_update(context);
							job->hasError = true;
							job->isFinished = true;
							job->errorMessage = "Failed to load sessions";
							job->cv.notify_all();
							continue;
						}

						if (!getInputTokens(job)) {
							common_sampler_free(job->smpl);
							llama_kv_self_seq_rm(context, job->seqId, -1, -1);
							llama_kv_self_update(context);
							job->hasError = true;
							job->isFinished = true;
							job->errorMessage = "Failed to tokenize input";
							job->cv.notify_all();
							continue;
						}

						if (!ensureNonEmptyInput(job)) {
							common_sampler_free(job->smpl);
							llama_kv_self_seq_rm(context, job->seqId, -1, -1);
							llama_kv_self_update(context);
							job->hasError = true;
							job->isFinished = true;
							job->errorMessage = "Failed to ensure input content";
							job->cv.notify_all();
							continue;
						}

						job->n_matching_session_tokens = matchSessionTokens(job);
						job->n_past = static_cast<int>(job->n_matching_session_tokens);
						job->i_prompt = static_cast<int>(job->n_matching_session_tokens);
						job->n_prompt = job->embd_inp.size();

						int remaining_prompt_tokens = job->n_prompt - job->i_prompt;
						int available_batch_space = g_params.n_batch - batch.n_tokens;

						if (available_batch_space <= 0) {
							break;
						}

						int tokens_to_process = std::min(remaining_prompt_tokens, available_batch_space);

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

						if (job->isDecodingPrompt) {
							break;
						}

						if (!ensureContextCapacity(job)) {
							common_sampler_free(job->smpl);
							llama_kv_self_seq_rm(context, job->seqId, -1, -1);
							llama_kv_self_update(context);
							job->hasError = true;
							job->isFinished = true;
							job->errorMessage = "Context overflow even after trimming.";
							job->cv.notify_all();
							continue;
						}
					}
				}

				if (batch_has_tokens) {
					if (llama_decode(context, batch)) {
						for (auto job : current_jobs)
						{
							std::lock_guard<std::mutex> jobLock(job->mtx);
							if (!job->isFinished) {
								job->hasError = true;
								job->errorMessage = "Could not decode next token";
								job->isFinished = true;
								job->cv.notify_all();
							}
						}
					}

					common_batch_clear(batch);
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

			if (!params.tools.empty())
			{
				if (!tc_client || (tc_client && params.tools.compare(tc_client->tool_list()) != 0))
				{
#ifdef DEBUG
					std::cout << "[INFERENCE] initializing tool call client" << std::endl;
					if (tc_client) std::cout << "[INFERENCE] current tools: " << tc_client->tool_list() << std::endl;
					std::cout << "[INFERENCE] new tools: " << params.tools << std::endl;
#endif
					toolcall::params tc_params(params.tools, params.toolChoice);
					tc_client = toolcall::create_client(tc_params);
					if (!tc_client)
					{
						throw std::runtime_error("[INFERENCE] [CHATCOMPLETE] [ERROR] Failed to create tool call client\n");
					}

					tc_client->initialize();
				}

				formatted = tokenizer->applyTemplate(messages, tc_client);
			}
			else
			{
				formatted = tokenizer->applyTemplate(messages);
			}
			
			CompletionParameters completionParams{
				formatted.c_str(),
				params.randomSeed,
				params.maxNewTokens,
				params.minLength,
				params.temperature,
				params.topP,
				params.streaming,
				params.kvCacheFilePath,
				params.seqId
			};

			return completionParams;
		}

	private:
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
		toolcall::client::ptr               tc_client;

		const int n_batch;
		const int n_keep;
		const int n_ctx;

#ifdef DEBUG
		void printLogits(llama_context* ctx, size_t maxLogits = 10) {
			// Get the logits after decoding
			float* logits = llama_get_logits_ith(ctx, -1);

			// Get vocabulary size (number of logits)
			const int n_vocab = llama_n_vocab(tokenizer->getVocab());

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
				if (!load_kv_cache(job->path_session, job->session_tokens, job->seqId)) {
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
					job->embd_inp.push_back(llama_token_bos(tokenizer->getVocab()));
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

		bool sampleNextToken(common_sampler* sampler, int& n_past, int& n_remain, std::vector<llama_token>& session_tokens, std::shared_ptr<Job> job, const std::string& path_session) {
			llama_token id = common_sampler_sample(sampler, context, -1);
			common_sampler_accept(sampler, id, true);
			common_batch_add(batch, id, n_past, { 0 }, true);

			if (llama_token_is_eog(tokenizer->getVocab(), id) || id == llama_token_eos(tokenizer->getVocab())) {
				return false; // Stop generation
			}

			const auto data = llama_perf_context(context);
			const std::string token_str = tokenizer->decode(id);
			{
				std::lock_guard<std::mutex> jobLock(job->mtx);
				job->generatedTokens.push_back(id);
				job->generatedText += token_str;
				job->tps = 1e3 / data.t_eval_ms * data.n_eval;
				job->cv.notify_all();
			}

			if (!path_session.empty()) {
				session_tokens.push_back(id);
			}

			n_remain -= 1;
		}

		bool sampleNextToken(std::shared_ptr<Job> job) {
			llama_token id = common_sampler_sample(job->smpl, context, job->batch_pos);
			common_sampler_accept(job->smpl, id, false);

			if (llama_token_is_eog(tokenizer->getVocab(), id) || id == llama_token_eos(tokenizer->getVocab())) {
				return false; // Stop generation
			}

			common_batch_add(batch, id, job->n_past, { job->seqId }, true);

			const auto data = llama_perf_context(context);
			const std::string token_str = tokenizer->decode(id);
			{
				job->generatedTokens.push_back(id);
				job->generatedText += token_str;
				job->tps = 1e3 / data.t_eval_ms * data.n_eval;
				job->cv.notify_all();
			}

			if (!job->path_session.empty()) {
				job->session_tokens.push_back(id);
			}

			job->n_remain -= 1;
			job->batch_pos = batch.n_tokens - 1;

			return true;
		}

		void saveSession(std::shared_ptr<Job> job) {
			if (!job->path_session.empty()) {
				llama_state_seq_save_file(context, job->path_session.c_str(), job->seqId, job->session_tokens.data(), job->session_tokens.size());
			}
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

				// Remove any "future" tokens that donât match
				// i.e. we only keep the portion that matched
				llama_kv_self_seq_rm(context, job->seqId, n_matching_session_tokens, -1 /*up to end*/);
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

			llama_kv_self_seq_rm(context, id, n_keep, n_keep + n_discard);
			llama_kv_self_seq_add(context, id, n_keep + n_discard, n_past, -n_discard);

			n_past -= n_discard;

			if (!session_tokens.empty() && session_tokens.size() >= (size_t)(n_keep + n_discard)) {
				session_tokens.erase(session_tokens.begin() + n_keep,
					session_tokens.begin() + n_keep + n_discard);
			}
		}
	};
} // namespace

struct InferenceEngine::Impl
{
	std::unique_ptr<InferenceService> inferenceService;

	// Job management members
	std::atomic<int> nextJobId{ 0 };
	std::unordered_map<int, std::shared_ptr<Job>> jobs;
	std::mutex jobsMutex;

	ThreadPool threadPool;

	Impl(const char* engineDir, const LoadingParameters lParams, const int mainGpuId = 0);
	~Impl();

	int submitCompletionsJob(const CompletionParameters& params);
	int submitChatCompletionsJob(const ChatCompletionParameters& params);
	void stopJob(int job_id);
	bool isJobFinished(int job_id);
	CompletionResult getJobResult(int job_id);
	void waitForJob(int job_id);
	bool hasJobError(int job_id);
	std::string getJobError(int job_id);
};

InferenceEngine::Impl::Impl(const char* engineDir, const LoadingParameters lParams, const int mainGpuId)
	: threadPool(lParams.n_parallel)
{
#ifndef DEBUG
	llama_log_set(llama_log_callback_null, NULL);
#endif

	std::filesystem::path tokenizer_model_path;
	for (const auto& entry : std::filesystem::directory_iterator(engineDir))
	{
		if (entry.path().extension() == ".gguf")
		{
			tokenizer_model_path = entry.path();
			break;
		}
	}

	if (!std::filesystem::exists(tokenizer_model_path))
	{
		throw std::runtime_error("[INFERENCE] [ERROR] Tokenizer model not found from " + tokenizer_model_path.string());
	}

	unsigned int inferenceThreads = 4;

#ifdef DEBUG
	std::cout << "[INFERENCE] Inference threads: " << inferenceThreads << std::endl;
#endif

	common_params_model params_model;
	params_model.path = tokenizer_model_path.string().c_str();

	common_params params;
	params.model						= params_model;
	params.n_ctx						= lParams.n_ctx;
	params.n_keep						= lParams.n_keep;
	params.use_mlock					= lParams.use_mlock;
	params.use_mmap						= lParams.use_mmap;
	params.cont_batching				= lParams.cont_batching;
	params.warmup						= lParams.warmup;
	params.cpuparams.n_threads			= inferenceThreads;
	params.n_parallel					= lParams.n_parallel;
	params.n_batch						= lParams.n_batch;
	params.n_ubatch                     = lParams.n_ubatch;
	params.webui						= false;
	params.single_turn					= true;
	params.compute_ppl					= false;
	params.use_jinja					= true;
#if defined(USE_CUDA) || defined(USE_VULKAN)
	std::cout << "[INFERENCE] Using CUDA or Vulkan" << std::endl;

	params.n_gpu_layers = lParams.n_gpu_layers;
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

	struct ggml_threadpool_params threadpool_params;
	ggml_threadpool_params_init(&threadpool_params, inferenceThreads);
	threadpool_params.prio = GGML_SCHED_PRIO_NORMAL;
	set_process_priority(GGML_SCHED_PRIO_NORMAL);
	struct ggml_threadpool* threadpool = ggml_threadpool_new(&threadpool_params);
	llama_attach_threadpool(ctx, threadpool, nullptr);

	// Create the tokenizer
	auto tokenizer = std::make_shared<Tokenizer>(model, ctx, params);
	// Create the inference service
	inferenceService = std::make_unique<LlamaInferenceService>(tokenizer, model, ctx, params, threadpool);
}

int InferenceEngine::Impl::submitCompletionsJob(const CompletionParameters& params)
{
	int jobId = nextJobId++;

	auto job = std::make_shared<Job>();
	job->jobId = jobId;
	job->seqId = params.seqId;

	// Asynchronously execute the job using thread pool
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
	catch (const std::exception& e)
	{
		std::cerr << "[INFERENCE] [ERROR] " << e.what() << std::endl;
		return -1;
	}

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		jobs.emplace(jobId, job);
	}

	return jobId;
}

int InferenceEngine::Impl::submitChatCompletionsJob(const ChatCompletionParameters& params)
{
	int jobId = nextJobId++;

	auto job = std::make_shared<Job>();
	job->jobId = jobId;
	job->seqId = params.seqId;

#ifdef DEBUG
	std::cout << "[INFERENCE] Submitting chat completions job to queue" << std::endl;
#endif

	// Asynchronously execute the job using thread pool
	try
	{
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
	catch (const std::exception& e)
	{
		std::cerr << "[INFERENCE] [ERROR] [submitChatCompletionsJob] " << e.what() << std::endl;
		return -1;
	}

	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		jobs.emplace(jobId, job);
	}

	return jobId;
}

void InferenceEngine::Impl::stopJob(int job_id) {
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
	if (isFinished)
	{
		std::lock_guard<std::mutex> lock(jobsMutex);
		jobs.erase(job_id);
	}
	return isFinished;
}

CompletionResult InferenceEngine::Impl::getJobResult(int job_id)
{
	std::shared_ptr<Job> job;

	{		std::lock_guard<std::mutex> lock(jobsMutex);
		auto it = jobs.find(job_id);
		if (it == jobs.end()) 
		{
			std::cerr << "[INFERENCE] [ERROR] [getJobResult] Invalid job ID " << job_id << "\n" << std::endl;
			CompletionResult result;
			result.tokens = {};
			result.text = "";
			result.tps = 0.0f;
			return result;
		}
		job = it->second;
	}

	std::lock_guard<std::mutex> jobLock(job->mtx);
	CompletionResult result;
	result.tokens = job->generatedTokens;
	result.text = job->generatedText;
	result.tps = job->tps;
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

INFERENCE_API bool InferenceEngine::loadModel(const char* engineDir, const LoadingParameters lParams, const int mainGpuId)
{
#ifdef DEBUG
	std::cout << "[INFERENCE] Loading model from " << engineDir << std::endl;
#endif
	this->pimpl.reset();

	try {
		this->pimpl = std::make_unique<Impl>(engineDir, lParams, mainGpuId);
	}
	catch (const std::exception& e) {
		std::cerr << "[INFERENCE] [ERROR] Could not load model from: " << engineDir << "\nError: " << e.what() << "\n" << std::endl;
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

INFERENCE_API InferenceEngine::~InferenceEngine() = default;

extern "C" INFERENCE_API IInferenceEngine* createInferenceEngine()
{
	return new InferenceEngine();
}

extern "C" INFERENCE_API void destroyInferenceEngine(IInferenceEngine* engine)
{
	delete static_cast<InferenceEngine*>(engine);
}