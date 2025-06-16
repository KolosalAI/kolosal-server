// File: include/kolosal/agents/job_manager.hpp
#pragma once

#include "../export.hpp"
#include "function_manager.hpp"
#include "agent_data.hpp"
#include <queue>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace kolosal::agents {

enum class JobStatus {
    PENDING, RUNNING, COMPLETED, FAILED, CANCELLED
};

struct KOLOSAL_SERVER_API Job {
    std::string id;
    std::string function_name;
    AgentData parameters;
    JobStatus status = JobStatus::PENDING;
    FunctionResult result;
    std::string requester;
    int priority = 0;
    
    Job(const std::string& func_name, const AgentData& params);
};

/**
 * @brief Manages job queue and execution for agents
 */
class KOLOSAL_SERVER_API JobManager {
private:
    std::queue<std::shared_ptr<Job>> job_queue;
    std::unordered_map<std::string, std::shared_ptr<Job>> all_jobs;
    std::shared_ptr<FunctionManager> function_manager;
    std::shared_ptr<Logger> logger;
    mutable std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> running{false};
    std::thread worker_thread;

public:
    JobManager(std::shared_ptr<FunctionManager> func_mgr, std::shared_ptr<Logger> log);
    ~JobManager();

    void start();
    void stop();

    std::string submit_job(const std::string& function_name, const AgentData& parameters, 
                          int priority = 0, const std::string& requester = "");
    JobStatus get_job_status(const std::string& job_id) const;
    FunctionResult get_job_result(const std::string& job_id) const;
    bool cancel_job(const std::string& job_id);
    std::map<std::string, int> get_stats() const;

private:
    void worker_loop();
};

} // namespace kolosal::agents