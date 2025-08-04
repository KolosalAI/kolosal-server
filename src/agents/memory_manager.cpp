// File: src/agents/memory_manager.cpp
#include "kolosal/agents/memory_manager.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <fstream>
#include <random>

namespace kolosal::agents {

// Bridge Logger interface to ServerLogger
class Logger {
public:
    void info(const std::string& message) { ServerLogger::logInfo("%s", message.c_str()); }
    void debug(const std::string& message) { ServerLogger::logDebug("%s", message.c_str()); }
    void warn(const std::string& message) { ServerLogger::logWarning("%s", message.c_str()); }
    void error(const std::string& message) { ServerLogger::logError("%s", message.c_str()); }
};

// ConversationMemory implementation
ConversationMemory::ConversationMemory(size_t max_messages) : max_size(max_messages) {}

void ConversationMemory::add_message(const std::string& role, const std::string& content, 
                                   const std::unordered_map<std::string, std::string>& metadata) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    std::string id = std::to_string(messages.size());
    MemoryEntry entry(id, content, "conversation");
    entry.metadata = metadata;
    entry.metadata["role"] = role;
    
    messages.push_back(std::move(entry));
    
    // Trim if necessary
    if (messages.size() > max_size) {
        messages.erase(messages.begin(), messages.begin() + (messages.size() - max_size));
    }
}

std::vector<MemoryEntry> ConversationMemory::get_recent_messages(size_t count) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (count >= messages.size()) {
        return messages;
    }
    
    return std::vector<MemoryEntry>(messages.end() - count, messages.end());
}

std::vector<MemoryEntry> ConversationMemory::get_all_messages() const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return messages;
}

void ConversationMemory::clear() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    messages.clear();
}

size_t ConversationMemory::size() const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return messages.size();
}

std::string ConversationMemory::get_context_window(size_t max_tokens) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    std::ostringstream context;
    size_t token_count = 0;
    
    // Start from the most recent messages and work backwards
    for (auto it = messages.rbegin(); it != messages.rend() && token_count < max_tokens; ++it) {
        const auto& entry = *it;
        std::string role = entry.metadata.count("role") ? entry.metadata.at("role") : "unknown";
        
        std::string message_text = role + ": " + entry.content + "\n";
        token_count += message_text.length() / 4; // Rough token estimation
        
        if (token_count <= max_tokens) {
            context.str(message_text + context.str());
        }
    }
    
    return context.str();
}

void ConversationMemory::trim_to_size(size_t target_size) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (messages.size() > target_size) {
        messages.erase(messages.begin(), messages.begin() + (messages.size() - target_size));
    }
}

// VectorMemory implementation
VectorMemory::VectorMemory(std::shared_ptr<Logger> log) : logger(log) {
    if (!logger) {
        logger = std::make_shared<Logger>();
    }
}

bool VectorMemory::store(const MemoryEntry& entry) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    if (entries.find(entry.id) != entries.end()) {
        logger->warn("Memory entry already exists: " + entry.id);
        return false;
    }
    
    MemoryEntry stored_entry = entry;
    stored_entry.embedding = generate_embedding(entry.content);
    
    entries[entry.id] = std::move(stored_entry);
    update_type_index(entry.id, entry.type);
    
    logger->debug("Stored memory entry: " + entry.id);
    return true;
}

bool VectorMemory::update(const std::string& id, const MemoryEntry& entry) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    auto it = entries.find(id);
    if (it == entries.end()) {
        return false;
    }
    
    std::string old_type = it->second.type;
    MemoryEntry updated_entry = entry;
    updated_entry.id = id;
    updated_entry.updated_at = std::chrono::system_clock::now();
    updated_entry.embedding = generate_embedding(entry.content);
    
    entries[id] = std::move(updated_entry);
    
    if (old_type != entry.type) {
        remove_from_type_index(id, old_type);
        update_type_index(id, entry.type);
    }
    
    logger->debug("Updated memory entry: " + id);
    return true;
}

bool VectorMemory::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    auto it = entries.find(id);
    if (it == entries.end()) {
        return false;
    }
    
    remove_from_type_index(id, it->second.type);
    entries.erase(it);
    
    logger->debug("Removed memory entry: " + id);
    return true;
}

std::vector<MemoryEntry> VectorMemory::search(const MemoryQuery& query) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    std::vector<MemoryEntry> results;
    
    for (const auto& pair : entries) {
        const auto& entry = pair.second;
        bool matches = true;
        
        // Filter by type
        if (!query.types.empty()) {
            matches = std::find(query.types.begin(), query.types.end(), entry.type) != query.types.end();
        }
        
        // Filter by time range
        if (matches && entry.created_at < query.after_time) matches = false;
        if (matches && entry.created_at > query.before_time) matches = false;
        
        // Filter by metadata
        if (matches && !query.metadata_filters.empty()) {
            for (const auto& filter : query.metadata_filters) {
                auto meta_it = entry.metadata.find(filter.first);
                if (meta_it == entry.metadata.end() || meta_it->second != filter.second) {
                    matches = false;
                    break;
                }
            }
        }
        
        // Text search (simple contains for now)
        if (matches && !query.query_text.empty()) {
            std::string content_lower = entry.content;
            std::string query_lower = query.query_text;
            std::transform(content_lower.begin(), content_lower.end(), content_lower.begin(), ::tolower);
            std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
            matches = content_lower.find(query_lower) != std::string::npos;
        }
        
        if (matches) {
            results.push_back(entry);
        }
    }
    
    // Sort by relevance (access count and recency for now)
    std::sort(results.begin(), results.end(), [](const MemoryEntry& a, const MemoryEntry& b) {
        return a.access_count > b.access_count || 
               (a.access_count == b.access_count && a.updated_at > b.updated_at);
    });
    
    // Limit results
    if (results.size() > static_cast<size_t>(query.max_results)) {
        results.resize(query.max_results);
    }
    
    return results;
}

MemoryEntry* VectorMemory::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    auto it = entries.find(id);
    if (it != entries.end()) {
        // Update access info (const_cast is safe here as we're just updating metadata)
        auto& entry = const_cast<MemoryEntry&>(it->second);
        entry.access_count++;
        entry.accessed_at = std::chrono::system_clock::now();
        return &entry;
    }
    return nullptr;
}

std::vector<MemoryEntry> VectorMemory::get_by_type(const std::string& type) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    std::vector<MemoryEntry> results;
    auto it = type_index.find(type);
    if (it != type_index.end()) {
        for (const std::string& id : it->second) {
            auto entry_it = entries.find(id);
            if (entry_it != entries.end()) {
                results.push_back(entry_it->second);
            }
        }
    }
    
    return results;
}

std::vector<MemoryEntry> VectorMemory::semantic_search(const std::string& query, int max_results) const {
    std::vector<float> query_embedding = generate_embedding(query);
    return similarity_search(query_embedding, max_results);
}

std::vector<MemoryEntry> VectorMemory::similarity_search(const std::vector<float>& query_embedding, 
                                                        int max_results) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    std::vector<std::pair<double, MemoryEntry>> scored_entries;
    
    for (const auto& pair : entries) {
        const auto& entry = pair.second;
        if (!entry.embedding.empty()) {
            double similarity = calculate_similarity(query_embedding, entry.embedding);
            scored_entries.emplace_back(similarity, entry);
        }
    }
    
    // Sort by similarity (descending)
    std::sort(scored_entries.begin(), scored_entries.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    std::vector<MemoryEntry> results;
    int count = std::min(max_results, static_cast<int>(scored_entries.size()));
    for (int i = 0; i < count; ++i) {
        results.push_back(scored_entries[i].second);
    }
    
    return results;
}

void VectorMemory::cleanup_old_entries(std::chrono::hours max_age) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    auto cutoff_time = std::chrono::system_clock::now() - max_age;
    std::vector<std::string> to_remove;
    
    for (const auto& pair : entries) {
        if (pair.second.created_at < cutoff_time && pair.second.access_count < 5) {
            to_remove.push_back(pair.first);
        }
    }
    
    for (const std::string& id : to_remove) {
        auto it = entries.find(id);
        if (it != entries.end()) {
            remove_from_type_index(id, it->second.type);
            entries.erase(it);
        }
    }
    
    logger->info("Cleaned up " + std::to_string(to_remove.size()) + " old memory entries");
}

void VectorMemory::optimize_memory() {
    // Placeholder for memory optimization (e.g., compression, deduplication)
    logger->debug("Memory optimization completed");
}

size_t VectorMemory::size() const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return entries.size();
}

double VectorMemory::calculate_similarity(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size() || a.empty()) return 0.0;
    
    // Cosine similarity
    double dot_product = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;
    
    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    if (norm_a == 0.0 || norm_b == 0.0) return 0.0;
    
    return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

std::vector<float> VectorMemory::generate_embedding(const std::string& text) const {
    // Placeholder: In a real implementation, this would use an embedding model
    // For now, return a simple hash-based pseudo-embedding
    std::hash<std::string> hasher;
    size_t hash_value = hasher(text);
    
    std::vector<float> embedding(128);  // 128-dimensional embedding
    std::mt19937 gen(hash_value);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (float& val : embedding) {
        val = dist(gen);
    }
    
    // Normalize
    float norm = 0.0f;
    for (float val : embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    
    if (norm > 0.0f) {
        for (float& val : embedding) {
            val /= norm;
        }
    }
    
    return embedding;
}

void VectorMemory::update_type_index(const std::string& id, const std::string& type) {
    type_index[type].push_back(id);
}

void VectorMemory::remove_from_type_index(const std::string& id, const std::string& type) {
    auto it = type_index.find(type);
    if (it != type_index.end()) {
        auto& ids = it->second;
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
        if (ids.empty()) {
            type_index.erase(it);
        }
    }
}

// WorkingMemory implementation
WorkingMemory::WorkingMemory() = default;

void WorkingMemory::set_context(const std::string& key, const AgentData& data) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    current_context[key] = data;
}

AgentData WorkingMemory::get_context(const std::string& key) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    auto it = current_context.find(key);
    return (it != current_context.end()) ? it->second : AgentData();
}

bool WorkingMemory::has_context(const std::string& key) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return current_context.find(key) != current_context.end();
}

void WorkingMemory::clear_context() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    current_context.clear();
}

void WorkingMemory::push_goal(const std::string& goal) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    goal_stack.push_back(goal);
}

std::string WorkingMemory::pop_goal() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    if (goal_stack.empty()) return "";
    
    std::string goal = goal_stack.back();
    goal_stack.pop_back();
    return goal;
}

std::string WorkingMemory::current_goal() const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return goal_stack.empty() ? "" : goal_stack.back();
}

std::vector<std::string> WorkingMemory::get_goal_stack() const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return goal_stack;
}

void WorkingMemory::clear_goals() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    goal_stack.clear();
}

void WorkingMemory::set_variable(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    variables[name] = value;
}

std::string WorkingMemory::get_variable(const std::string& name) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    auto it = variables.find(name);
    return (it != variables.end()) ? it->second : "";
}

bool WorkingMemory::has_variable(const std::string& name) const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return variables.find(name) != variables.end();
}

void WorkingMemory::clear_variables() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    variables.clear();
}

void WorkingMemory::set_current_task(const std::string& task) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    current_task = task;
}

std::string WorkingMemory::get_current_task() const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return current_task;
}

AgentData WorkingMemory::serialize() const {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    AgentData data;
    data.set("current_task", current_task);
    
    // Serialize goal stack
    std::string goals_str;
    for (size_t i = 0; i < goal_stack.size(); ++i) {
        if (i > 0) goals_str += "|";
        goals_str += goal_stack[i];
    }
    data.set("goal_stack", goals_str);
    
    // Serialize variables
    for (const auto& pair : variables) {
        data.set("var_" + pair.first, pair.second);
    }
    
    return data;
}

void WorkingMemory::deserialize(const AgentData& data) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    
    current_task = data.get_string("current_task");
    
    // Deserialize goal stack
    std::string goals_str = data.get_string("goal_stack");
    goal_stack.clear();
    if (!goals_str.empty()) {
        std::istringstream iss(goals_str);
        std::string goal;
        while (std::getline(iss, goal, '|')) {
            goal_stack.push_back(goal);
        }
    }
    
    // Deserialize variables
    variables.clear();
    auto keys = data.get_keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        if (key.substr(0, 4) == "var_") {
            std::string var_name = key.substr(4);
            variables[var_name] = data.get_string(key);
        }
    }
}

// MemoryManager implementation
MemoryManager::MemoryManager(const std::string& aid, std::shared_ptr<Logger> log) 
    : agent_id(aid), logger(log) {
    if (!logger) {
        logger = std::make_shared<Logger>();
    }
    
    conversation_memory = std::make_unique<ConversationMemory>();
    vector_memory = std::make_unique<VectorMemory>(logger);
    working_memory = std::make_unique<WorkingMemory>();
    
    logger->info("Memory manager initialized for agent: " + agent_id);
}

MemoryManager::~MemoryManager() = default;

void MemoryManager::store_conversation(const std::string& role, const std::string& content) {
    conversation_memory->add_message(role, content);
    
    // Also store important conversations in long-term memory
    if (content.length() > 50) {  // Only store substantial messages
        std::string id = "conv_" + std::to_string(std::time(nullptr));
        MemoryEntry entry(id, content, "conversation");
        entry.metadata["role"] = role;
        entry.metadata["agent_id"] = agent_id;
        vector_memory->store(entry);
    }
}

void MemoryManager::store_fact(const std::string& fact, const std::unordered_map<std::string, std::string>& metadata) {
    std::string id = "fact_" + std::to_string(std::time(nullptr));
    MemoryEntry entry(id, fact, "fact");
    entry.metadata = metadata;
    entry.metadata["agent_id"] = agent_id;
    vector_memory->store(entry);
}

void MemoryManager::store_procedure(const std::string& procedure, const std::string& name) {
    std::string id = "proc_" + name;
    MemoryEntry entry(id, procedure, "procedure");
    entry.metadata["name"] = name;
    entry.metadata["agent_id"] = agent_id;
    vector_memory->store(entry);
}

std::vector<MemoryEntry> MemoryManager::retrieve_relevant_memories(const std::string& query, int max_results) {
    return vector_memory->semantic_search(query, max_results);
}

std::string MemoryManager::get_context_for_query(const std::string& query, size_t max_tokens) {
    std::ostringstream context;
    
    // Add recent conversation context
    std::string conv_context = conversation_memory->get_context_window(max_tokens / 2);
    if (!conv_context.empty()) {
        context << "Recent conversation:\n" << conv_context << "\n";
    }
    
    // Add relevant memories
    auto relevant_memories = retrieve_relevant_memories(query, 3);
    if (!relevant_memories.empty()) {
        context << "Relevant memories:\n";
        for (const auto& memory : relevant_memories) {
            context << "- " << memory.content << "\n";
        }
    }
    
    return context.str();
}

void MemoryManager::consolidate_memories() {
    // Placeholder for memory consolidation logic
    logger->info("Memory consolidation completed for agent: " + agent_id);
}

void MemoryManager::summarize_conversation_history() {
    // Placeholder for conversation summarization
    logger->info("Conversation history summarized for agent: " + agent_id);
}

bool MemoryManager::save_to_file(const std::string& filepath) {
    // Placeholder for persistence
    logger->info("Memory saved to file: " + filepath);
    return true;
}

bool MemoryManager::load_from_file(const std::string& filepath) {
    // Placeholder for persistence
    logger->info("Memory loaded from file: " + filepath);
    return true;
}

MemoryManager::MemoryStats MemoryManager::get_statistics() const {
    MemoryStats stats;
    stats.conversation_count = conversation_memory->size();
    stats.vector_memory_count = vector_memory->size();
    stats.working_memory_items = working_memory->has_context("main") ? 1 : 0;
    stats.total_memory_size_mb = 0.0;  // Placeholder
    
    return stats;
}

} // namespace kolosal::agents
