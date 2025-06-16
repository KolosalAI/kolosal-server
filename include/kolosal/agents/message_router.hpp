

// File: include/kolosal/agents/message_router.hpp
#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include <queue>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace kolosal::agents {

// Forward declaration
class Logger;

/**
 * @brief Routes messages between agents
 */
class KOLOSAL_SERVER_API MessageRouter {
private:
    std::queue<AgentMessage> message_queue;
    std::unordered_map<std::string, std::function<void(const AgentMessage&)>> message_handlers;
    mutable std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> running{false};
    std::thread router_thread;
    std::shared_ptr<Logger> logger;

public:
    MessageRouter(std::shared_ptr<Logger> log);
    ~MessageRouter();

    void start();
    void stop();

    void register_agent_handler(const std::string& agent_id, 
                               std::function<void(const AgentMessage&)> handler);
    void unregister_agent_handler(const std::string& agent_id);
    void route_message(const AgentMessage& message);
    void broadcast_message(const AgentMessage& message);

private:
    void routing_loop();
};

} // namespace kolosal::agents