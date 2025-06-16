// File: src/agents/message_router.cpp
#include "kolosal/agents/message_router.hpp"
#include "kolosal/agents/logger.hpp"

namespace kolosal::agents {

MessageRouter::MessageRouter(std::shared_ptr<Logger> log) : logger(log) {}

MessageRouter::~MessageRouter() {
    stop();
}

void MessageRouter::start() {
    if (running.load()) return;
    running.store(true);
    router_thread = std::thread(&MessageRouter::routing_loop, this);
    logger->info("Message router started");
}

void MessageRouter::stop() {
    if (!running.load()) return;
    running.store(false);
    queue_cv.notify_one();
    if (router_thread.joinable()) {
        router_thread.join();
    }
    logger->info("Message router stopped");
}

void MessageRouter::register_agent_handler(const std::string& agent_id, 
                                          std::function<void(const AgentMessage&)> handler) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    message_handlers[agent_id] = handler;
    logger->debug("Registered message handler for agent: " + agent_id);
}

void MessageRouter::unregister_agent_handler(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    message_handlers.erase(agent_id);
    logger->debug("Unregistered message handler for agent: " + agent_id);
}

void MessageRouter::route_message(const AgentMessage& message) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        message_queue.push(message);
    }
    queue_cv.notify_one();
    logger->debug("Message queued: " + message.id + " from " + message.from_agent + " to " + message.to_agent);
}

void MessageRouter::broadcast_message(const AgentMessage& message) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    for (const auto& handler : message_handlers) {
        if (handler.first != message.from_agent) {
            AgentMessage broadcast_msg = message;
            broadcast_msg.to_agent = handler.first;
            message_queue.push(broadcast_msg);
        }
    }
    queue_cv.notify_one();
    logger->debug("Broadcast message queued: " + message.id + " from " + message.from_agent);
}

void MessageRouter::routing_loop() {
    while (running.load()) {
        AgentMessage message("", "", "");
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] {
                return !message_queue.empty() || !running.load();
            });

            if (!running.load()) break;
            if (message_queue.empty()) continue;

            message = std::move(message_queue.front());
            message_queue.pop();
        }

        auto it = message_handlers.find(message.to_agent);
        if (it != message_handlers.end()) {
            try {
                it->second(message);
                logger->debug("Message delivered: " + message.id + " to " + message.to_agent);
            } catch (const std::exception& e) {
                logger->error("Message delivery failed: " + std::string(e.what()));
            }
        } else {
            logger->warn("No handler for agent: " + message.to_agent + " (message: " + message.id + ")");
        }
    }
}

} // namespace kolosal::agents