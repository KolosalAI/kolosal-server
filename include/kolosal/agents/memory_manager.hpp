// File: include/kolosal/agents/memory_manager.hpp
#pragma once

#include "../export.hpp"
#include "agent_data.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <mutex>

namespace kolosal::agents {

/**
 * @brief Memory entry with metadata
 */
struct KOLOSAL_SERVER_API MemoryEntry {
    std::string id;
    std::string content;
    std::string type;  // "conversation", "fact", "procedure", "context"
    std::unordered_map<std::string, std::string> metadata;
    std::chrono::time_point<std::chrono::system_clock> created_at;
    std::chrono::time_point<std::chrono::system_clock> accessed_at;
    std::chrono::time_point<std::chrono::system_clock> updated_at;
    int access_count = 0;
    double relevance_score = 0.0;
    std::vector<float> embedding;  // For semantic search
    
    // Default constructor
    MemoryEntry() : type("general"), access_count(0), relevance_score(0.0) {
        auto now = std::chrono::system_clock::now();
        created_at = accessed_at = updated_at = now;
    }
    
    MemoryEntry(const std::string& mem_id, const std::string& mem_content, 
               const std::string& mem_type = "general")
        : id(mem_id), content(mem_content), type(mem_type), access_count(0), relevance_score(0.0) {
        auto now = std::chrono::system_clock::now();
        created_at = accessed_at = updated_at = now;
    }
};

/**
 * @brief Memory query for searching and filtering
 */
struct KOLOSAL_SERVER_API MemoryQuery {
    std::string query_text;
    std::vector<std::string> types;
    std::unordered_map<std::string, std::string> metadata_filters;
    std::chrono::time_point<std::chrono::system_clock> after_time;
    std::chrono::time_point<std::chrono::system_clock> before_time;
    int max_results = 10;
    double min_relevance = 0.0;
    bool semantic_search = true;
    
    MemoryQuery(const std::string& query = "") : query_text(query) {
        after_time = std::chrono::time_point<std::chrono::system_clock>::min();
        before_time = std::chrono::time_point<std::chrono::system_clock>::max();
    }
};

/**
 * @brief Short-term conversation memory
 */
class KOLOSAL_SERVER_API ConversationMemory {
private:
    std::vector<MemoryEntry> messages;
    size_t max_size;
    mutable std::mutex memory_mutex;
    
public:
    ConversationMemory(size_t max_messages = 100);
    
    void add_message(const std::string& role, const std::string& content, 
                    const std::unordered_map<std::string, std::string>& metadata = {});
    std::vector<MemoryEntry> get_recent_messages(size_t count = 10) const;
    std::vector<MemoryEntry> get_all_messages() const;
    void clear();
    size_t size() const;
    
    // Context window management
    std::string get_context_window(size_t max_tokens = 4000) const;
    void trim_to_size(size_t target_size);
};

/**
 * @brief Long-term vector-based memory
 */
class KOLOSAL_SERVER_API VectorMemory {
private:
    std::unordered_map<std::string, MemoryEntry> entries;
    std::unordered_map<std::string, std::vector<std::string>> type_index;
    mutable std::mutex memory_mutex;
    std::shared_ptr<class Logger> logger;
    
public:
    VectorMemory(std::shared_ptr<class Logger> log = nullptr);
    
    // Memory operations
    bool store(const MemoryEntry& entry);
    bool update(const std::string& id, const MemoryEntry& entry);
    bool remove(const std::string& id);
    
    // Retrieval
    std::vector<MemoryEntry> search(const MemoryQuery& query) const;
    MemoryEntry* get(const std::string& id) const;
    std::vector<MemoryEntry> get_by_type(const std::string& type) const;
    
    // Semantic search
    std::vector<MemoryEntry> semantic_search(const std::string& query, int max_results = 10) const;
    std::vector<MemoryEntry> similarity_search(const std::vector<float>& query_embedding, 
                                              int max_results = 10) const;
    
    // Memory management
    void cleanup_old_entries(std::chrono::hours max_age = std::chrono::hours(24 * 30)); // 30 days
    void optimize_memory();
    size_t size() const;
    
private:
    double calculate_similarity(const std::vector<float>& a, const std::vector<float>& b) const;
    std::vector<float> generate_embedding(const std::string& text) const;
    void update_type_index(const std::string& id, const std::string& type);
    void remove_from_type_index(const std::string& id, const std::string& type);
};

/**
 * @brief Working memory for current task context
 */
class KOLOSAL_SERVER_API WorkingMemory {
private:
    std::unordered_map<std::string, AgentData> current_context;
    std::vector<std::string> goal_stack;
    std::unordered_map<std::string, std::string> variables;
    std::string current_task;
    mutable std::mutex memory_mutex;
    
public:
    WorkingMemory();
    
    // Context management
    void set_context(const std::string& key, const AgentData& data);
    AgentData get_context(const std::string& key) const;
    bool has_context(const std::string& key) const;
    void clear_context();
    
    // Goal stack management
    void push_goal(const std::string& goal);
    std::string pop_goal();
    std::string current_goal() const;
    std::vector<std::string> get_goal_stack() const;
    void clear_goals();
    
    // Variables
    void set_variable(const std::string& name, const std::string& value);
    std::string get_variable(const std::string& name) const;
    bool has_variable(const std::string& name) const;
    void clear_variables();
    
    // Task management
    void set_current_task(const std::string& task);
    std::string get_current_task() const;
    
    // State serialization
    AgentData serialize() const;
    void deserialize(const AgentData& data);
};

/**
 * @brief Comprehensive memory manager
 */
class KOLOSAL_SERVER_API MemoryManager {
private:
    std::unique_ptr<ConversationMemory> conversation_memory;
    std::unique_ptr<VectorMemory> vector_memory;
    std::unique_ptr<WorkingMemory> working_memory;
    std::shared_ptr<class Logger> logger;
    std::string agent_id;
    
public:
    MemoryManager(const std::string& agent_id, std::shared_ptr<class Logger> log = nullptr);
    ~MemoryManager();
    
    // Memory access
    ConversationMemory* get_conversation_memory() { return conversation_memory.get(); }
    VectorMemory* get_vector_memory() { return vector_memory.get(); }
    WorkingMemory* get_working_memory() { return working_memory.get(); }
    
    // High-level operations
    void store_conversation(const std::string& role, const std::string& content);
    void store_fact(const std::string& fact, const std::unordered_map<std::string, std::string>& metadata = {});
    void store_procedure(const std::string& procedure, const std::string& name);
    
    // Intelligent retrieval
    std::vector<MemoryEntry> retrieve_relevant_memories(const std::string& query, int max_results = 5);
    std::string get_context_for_query(const std::string& query, size_t max_tokens = 2000);
    
    // Memory consolidation
    void consolidate_memories();
    void summarize_conversation_history();
    
    // Persistence
    bool save_to_file(const std::string& filepath);
    bool load_from_file(const std::string& filepath);
    
    // Statistics
    struct MemoryStats {
        size_t conversation_count;
        size_t vector_memory_count;
        size_t working_memory_items;
        double total_memory_size_mb;
    };
    
    MemoryStats get_statistics() const;
};

} // namespace kolosal::agents
