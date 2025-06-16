// File: src/agents/job_manager.cpp
#include "kolosal/agents/job_manager.hpp"
#include "kolosal/logger.hpp"

namespace kolosal::agents {

// Job implementation
Job::Job(const std::string& func_name, const AgentData& params)
    : id(UUIDGenerator::generate()), function_name(func_name), parameters(params) {}

// JobManager implementation
JobManager::JobManager(std::shared_ptr<FunctionManager> func_mgr, std::shared_ptr<Logger> log)
    : function_manager(func_mgr), logger(log) {}

JobManager::~JobManager() {
    stop();
}

void JobManager::start() {
    if (running.load()) return;
    running.store(true);
    worker_thread = std::thread(&JobManager::worker_loop, this);
    logger->info("Job manager started");
}

void JobManager::stop() {
    if (!running.load()) return;
    running.store(false);
    queue_cv.notify_one();
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    logger->info("Job manager stopped");
}

std::string JobManager::submit_job(const std::string& function_name, const AgentData& parameters, 
                                  int priority, const std::string& requester) {
    auto job = std::make_shared<Job>(function_name, parameters);
    job->priority = priority;
    job->requester = requester;
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        job_queue.push(job);
        all_jobs[job->id] = job;
    }
    queue_cv.notify_one();
    
    logger->debug("Job submitted: " + job->id + " (function: " + function_name + ")");
    return job->id;
}

JobStatus JobManager::get_job_status(const std::string& job_id) const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    auto it = all_jobs.find(job_id);
    return (it != all_jobs.end()) ? it->second->status : JobStatus::FAILED;
}

FunctionResult JobManager::get_job_result(const std::string& job_id) const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    auto it = all_jobs.find(job_id);
    if (it != all_jobs.end()) {
        return it->second->result;
    }
    return FunctionResult(false, "Job not found");
}

bool JobManager::cancel_job(const std::string& job_id) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    auto it = all_jobs.find(job_id);
    if (it != all_jobs.end() && it->second->status == JobStatus::PENDING) {
        it->second->status = JobStatus::CANCELLED;
        logger->info("Job cancelled: " + job_id);
        return true;
    }
    return false;
}

std::map<std::string, int> JobManager::get_stats() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return {
        {"total", static_cast<int>(all_jobs.size())}, 
        {"queue_size", static_cast<int>(job_queue.size())}
    };
}

void JobManager::worker_loop() {
    while (running.load()) {
        std::shared_ptr<Job> job_to_process;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] { return !job_queue.empty() || !running.load(); });
            
            if (!running.load()) break;
            if (job_queue.empty()) continue;
            
            job_to_process = job_queue.front();
            job_queue.pop();
            job_to_process->status = JobStatus::RUNNING;
        }

        logger->debug("Processing job: " + job_to_process->id);
        job_to_process->result = function_manager->execute_function(
            job_to_process->function_name, job_to_process->parameters);
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            job_to_process->status = job_to_process->result.success ? 
                JobStatus::COMPLETED : JobStatus::FAILED;
        }
        
        logger->debug("Job completed: " + job_to_process->id + 
                     " (status: " + (job_to_process->result.success ? "SUCCESS" : "FAILED") + ")");
    }
}

} // namespace kolosal::agents