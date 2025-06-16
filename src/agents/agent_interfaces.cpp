// File: src/agents/agent_interfaces.cpp
#include "kolosal/agents/agent_interfaces.hpp"
#include "kolosal/agents/agent_data.hpp"

namespace kolosal::agents {

AgentMessage::AgentMessage(const std::string& from, const std::string& to, const std::string& msg_type)
    : id(UUIDGenerator::generate()), from_agent(from), to_agent(to), type(msg_type),
      timestamp(std::chrono::system_clock::now()) {}

} // namespace kolosal::agents