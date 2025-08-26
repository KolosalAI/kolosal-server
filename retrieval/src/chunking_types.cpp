#include "kolosal/retrieval/chunking_types.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "inference_interface.h"
#include <stdexcept>
#include <future>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <regex>

namespace kolosal
{
namespace retrieval
{

ChunkingService::ChunkingService()
{
    ServerLogger::logInfo("ChunkingService initialized");
}

ChunkingService::~ChunkingService() = default;

std::vector<std::string> ChunkingService::generateBaseChunks(
    const std::string& text,
    const std::vector<std::string>& tokens,
    int chunk_size,
    int overlap
) const
{
    validateChunkingParameters(chunk_size, overlap, chunk_size * 2, 0.0f);
    
    if (tokens.empty())
    {
        ServerLogger::logWarning("Empty tokens vector provided to generateBaseChunks");
        return {};
    }
    
    std::vector<std::string> chunks;
    int step = chunk_size - overlap;
    
    if (step <= 0)
    {
        throw std::runtime_error("Invalid chunking parameters: step size must be positive");
    }
    
    for (int start = 0; start < static_cast<int>(tokens.size()); start += step)
    {
        int end = std::min(start + chunk_size, static_cast<int>(tokens.size()));
        
        if (start >= end)
        {
            break;
        }
        
        auto chunk_tokens = extractTokenSubset(tokens, start, end);
        if (!chunk_tokens.empty())
        {
            std::string chunk_text = reconstructText(chunk_tokens);
            chunks.push_back(chunk_text);
        }
        
        if (end >= static_cast<int>(tokens.size()))
        {
            break;
        }
    }
    
    ServerLogger::logDebug("Generated %zu base chunks from %zu tokens", chunks.size(), tokens.size());
    return chunks;
}

std::future<std::vector<std::string>> ChunkingService::semanticChunk(
    const std::string& text,
    const std::string& model_name,
    int chunk_size,
    int overlap,
    int max_tokens,
    float similarity_threshold
)
{
    return std::async(std::launch::async, [=]() -> std::vector<std::string> {
        try
        {
            validateChunkingParameters(chunk_size, overlap, max_tokens, similarity_threshold);
            
            // Step 1: Tokenize the text
            auto tokens_future = tokenizeText(text, model_name);
            auto tokens = tokens_future.get();
            
            // Step 2: Generate base chunks
            auto base_chunks = generateBaseChunks(text, tokens, chunk_size, overlap);
            
            if (base_chunks.empty())
            {
                return {};
            }
            
            if (base_chunks.size() == 1)
            {
                return base_chunks;
            }
            
            // Step 3: Compute embeddings for all base chunks sequentially
            std::vector<std::vector<float>> embeddings;
            embeddings.reserve(base_chunks.size());
            
            for (const auto& chunk : base_chunks)
            {
                auto embedding_future = computeEmbedding(chunk, model_name);
                embeddings.push_back(embedding_future.get());
            }
            
            // Step 4: Merge chunks based on semantic similarity
            std::vector<std::string> merged_chunks;
            std::string current_chunk = base_chunks[0];
            std::vector<float> current_embedding = embeddings[0];
            int current_token_count = estimateTokenCount(current_chunk);
            
            for (size_t i = 1; i < base_chunks.size(); ++i)
            {
                const auto& next_chunk = base_chunks[i];
                const auto& next_embedding = embeddings[i];
                int next_token_count = estimateTokenCount(next_chunk);
                
                float similarity = cosineSimilarity(current_embedding, next_embedding);
                
                bool can_merge = (
                    similarity >= similarity_threshold &&
                    (current_token_count + next_token_count <= max_tokens)
                );
                
                if (can_merge)
                {
                    // Merge chunks
                    current_chunk = current_chunk + " " + next_chunk;
                    current_token_count += next_token_count;
                    // Note: We don't update the embedding to save computation cost
                }
                else
                {
                    // Finalize current chunk and start new one
                    merged_chunks.push_back(current_chunk);
                    current_chunk = next_chunk;
                    current_embedding = next_embedding;
                    current_token_count = next_token_count;
                }
            }
            
            // Add the last chunk
            merged_chunks.push_back(current_chunk);
            
            ServerLogger::logDebug("Semantic chunking: %zu base chunks merged into %zu chunks", 
                                   base_chunks.size(), merged_chunks.size());
            
            return merged_chunks;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error in semantic chunking: %s", ex.what());
            throw;
        }
    });
}

std::future<std::vector<std::string>> ChunkingService::tokenizeText(
    const std::string& text,
    const std::string& model_name
)
{
    return std::async(std::launch::async, [=]() -> std::vector<std::string> {
        try
        {
            // For now, use simple space-based tokenization as a fallback
            // In a real implementation, we would use the model's tokenizer
            std::vector<std::string> tokens;
            
            // Simple regex-based tokenization
            std::regex word_regex(R"(\S+)");
            std::sregex_iterator iter(text.begin(), text.end(), word_regex);
            std::sregex_iterator end;
            
            for (; iter != end; ++iter)
            {
                tokens.push_back(iter->str());
            }
            
            ServerLogger::logDebug("Tokenized text into %zu tokens", tokens.size());
            return tokens;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error in tokenization: %s", ex.what());
            throw;
        }
    });
}

std::future<std::vector<float>> ChunkingService::computeEmbedding(
    const std::string& text,
    const std::string& model_name
)
{
    return std::async(std::launch::async, [=]() -> std::vector<float> {
        try
        {
            // Serialize embedding requests to avoid concurrency issues
            std::lock_guard<std::mutex> lock(embedding_mutex_);
            
            // Get the NodeManager and build candidate engine list (requested first)
            auto& nodeManager = ServerAPI::instance().getNodeManager();
            std::vector<std::string> candidates;
            if (!model_name.empty()) candidates.push_back(model_name);
            for (const auto& id : nodeManager.listEngineIds())
            {
                if (id != model_name) candidates.push_back(id);
            }

            std::string usedModel;
            for (const auto& id : candidates)
            {
                auto engine = nodeManager.getEngine(id);
                if (!engine)
                {
                    continue;
                }

                // Create embedding parameters
                EmbeddingParameters params;
                params.input = text;
                params.normalize = true;
                params.seqId = static_cast<int>(std::hash<std::string>{}(text + id) % 10000);

                if (!params.isValid())
                {
                    ServerLogger::logWarning("Invalid embedding parameters; skipping engine '%s'", id.c_str());
                    continue;
                }

                int jobId = engine->submitEmbeddingJob(params);
                if (jobId < 0)
                {
                    ServerLogger::logWarning("Engine '%s' rejected embedding job; trying next engine", id.c_str());
                    continue;
                }

                ServerLogger::logDebug("Submitted embedding job %d for model '%s', text length: %zu", jobId, id.c_str(), text.length());

                engine->waitForJob(jobId);

                if (engine->hasJobError(jobId))
                {
                    std::string error = engine->getJobError(jobId);
                    ServerLogger::logWarning("Embedding job %d on model '%s' failed: %s", jobId, id.c_str(), error.c_str());
                    continue;
                }

                EmbeddingResult result = engine->getEmbeddingResult(jobId);
                if (result.embedding.empty())
                {
                    ServerLogger::logWarning("Model '%s' returned empty embedding; trying next engine", id.c_str());
                    continue;
                }

                usedModel = id;
                ServerLogger::logInfo("Completed embedding: using model '%s' with %zu dimensions", usedModel.c_str(), result.embedding.size());
                return result.embedding;
            }

            throw std::runtime_error("No available engine could compute embeddings");
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error computing embedding: %s", ex.what());
            throw;
        }
    });
}

float ChunkingService::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.empty() || b.empty() || a.size() != b.size())
    {
        return 0.0f;
    }
    
    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    
    for (size_t i = 0; i < a.size(); ++i)
    {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    if (norm_a == 0.0f || norm_b == 0.0f)
    {
        return 0.0f;
    }
    
    return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

int ChunkingService::estimateTokenCount(const std::string& text)
{
    // Simple estimation: roughly 4 characters per token for English text
    return std::max(1, static_cast<int>(text.length() / 4));
}

std::string ChunkingService::reconstructText(const std::vector<std::string>& tokens)
{
    if (tokens.empty())
    {
        return "";
    }
    
    std::string result;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (i > 0)
        {
            result += " ";
        }
        result += tokens[i];
    }
    
    return result;
}

void ChunkingService::validateChunkingParameters(
    int chunk_size,
    int overlap,
    int max_tokens,
    float similarity_threshold
) const
{
    if (chunk_size <= 0)
    {
        throw std::runtime_error("chunk_size must be positive");
    }
    
    if (overlap < 0 || overlap >= chunk_size)
    {
        throw std::runtime_error("overlap must be >= 0 and < chunk_size");
    }
    
    if (max_tokens < chunk_size)
    {
        throw std::runtime_error("max_tokens must be >= chunk_size");
    }
    
    if (similarity_threshold < 0.0f || similarity_threshold > 1.0f)
    {
        throw std::runtime_error("similarity_threshold must be between 0.0 and 1.0");
    }
}

std::vector<std::string> ChunkingService::extractTokenSubset(
    const std::vector<std::string>& tokens,
    int start,
    int end
) const
{
    if (start < 0 || end > static_cast<int>(tokens.size()) || start >= end)
    {
        return {};
    }
    
    return std::vector<std::string>(tokens.begin() + start, tokens.begin() + end);
}

} // namespace retrieval
} // namespace kolosal
