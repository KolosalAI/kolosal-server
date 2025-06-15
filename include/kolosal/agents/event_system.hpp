// File: include/kolosal/agents/event_system.hpp
#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace kolosal::agents {

// Forward declaration
class Logger;

/**
 * @brief Manages events and event handlers in the agent system
 */
class KOLOSAL_SERVER_API EventSystem {
private:
    std::unordered_map<std::string, std::vector<std::shared_ptr<EventHandler>>> handlers;
    std::shared_ptr<Logger> logger;
    std::atomic<bool> running{false};
    mutable std::mutex handlers_mutex;

public:
    EventSystem(std::shared_ptr<Logger> log);

    void start();
    void stop();

    void emit(const std::string& event_type, const std::string& source, 
             const AgentData& data = AgentData());
    void subscribe(const std::string& event_type, std::shared_ptr<EventHandler> handler);
    void unsubscribe(const std::string& event_type, std::shared_ptr<EventHandler> handler);
};

} // namespace kolosal::agents