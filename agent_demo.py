#!/usr/bin/env python3
"""
Agent Response Demo for Kolosal Server

This script demonstrates how to interact with different agents configured in the Kolosal Server.
It uses the downloaded LLM model and agent configurations to showcase various agent capabilities.

Features:
- Automatic model loading from downloads folder
- Interactive agent selection
- Demonstration of different agent types (research, code, data analysis, content creation)
- Real-time streaming response display
- Non-streaming response mode
- Conversation history management
- Toggle streaming mode during runtime

Usage:
    python agent_demo.py [--host HOST] [--port PORT] [--config CONFIG_PATH] [--no-streaming]
"""

import requests
import json
import argparse
import sys
import time
import yaml
import os
import logging
from typing import Dict, Any, Optional, List, Iterator, Generator
from datetime import datetime

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('agent_demo.log'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

class AgentDemo:
    def __init__(self, host: str = "localhost", port: int = 8080, config_path: str = "config/agents.yaml"):
        self.base_url = f"http://{host}:{port}"
        self.config_path = config_path
        self.session = requests.Session()
        self.available_agents = []
        self.current_agent = None
        self.conversation_history = []
        self.streaming_enabled = True  # Enable streaming by default
        
        # Load agent configuration
        self.agent_config = self.load_agent_config()
        
    def load_agent_config(self) -> Dict[str, Any]:
        """Load agent configuration from YAML file"""
        try:
            with open(self.config_path, 'r', encoding='utf-8') as file:
                config = yaml.safe_load(file)
                print(f"✅ Loaded agent configuration from {self.config_path}")
                return config        
        except Exception as e:
            print(f"❌ Failed to load agent config: {e}")
            return {}
    
    def check_server_health(self) -> bool:
        """Check if the server is running and healthy"""
        try:
            response = self.session.get(f"{self.base_url}/v1/health", timeout=5)
            return response.status_code == 200
        except Exception as e:
            print(f"❌ Server health check failed: {e}")
            return False
    
    def setup_model_engine(self) -> bool:
        """Setup the model engine using the downloaded model"""
        try:
            model_path = "downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf"
            engine_id = "qwen-0.6b-demo"
            
            # Check if model file exists
            if not os.path.exists(model_path):
                print(f"❌ Model file not found: {model_path}")
                print("Please download the model first using the download script")
                return False
            
            print(f"✅ Found model file: {model_path}")
            
            # First, check if engine already exists by trying to get its details
            print(f"🔍 Checking if engine '{engine_id}' already exists...")
            engine_check_response = self.session.get(f"{self.base_url}/engines/{engine_id}")
            
            if engine_check_response.status_code == 200:
                print(f"✅ Engine '{engine_id}' already exists and is available")
                return True
            elif engine_check_response.status_code == 404:
                print(f"🔄 Engine '{engine_id}' not found, will create new engine")
            else:
                print(f"⚠️  Engine check returned status {engine_check_response.status_code}, will attempt to list all engines")
            
            # Fallback: Check engines list endpoint
            engines_response = self.session.get(f"{self.base_url}/engines")
            if engines_response.status_code == 200:
                engines = engines_response.json()
                
                # Handle different response formats
                engine_list = []
                if isinstance(engines, list):
                    engine_list = engines
                elif isinstance(engines, dict):
                    if 'data' in engines:
                        engine_list = engines['data']
                    elif 'engines' in engines:
                        engine_list = engines['engines']
                
                # Check if our engine exists in the list
                for engine in engine_list:
                    if isinstance(engine, dict) and engine.get('engine_id') == engine_id:
                        print(f"✅ Engine '{engine_id}' found in engines list")
                        return True
            
            # Engine doesn't exist, create it
            print(f"🔄 Creating engine '{engine_id}'...")
            engine_data = {
                "engine_id": engine_id,
                "model_path": model_path,
                "n_ctx": 4096,
                "n_gpu_layers": 0,
                "main_gpu_id": 0,
                "n_batch": 512,
                "n_threads": 4
            }
            
            create_response = self.session.post(f"{self.base_url}/engines", json=engine_data)
            
            if create_response.status_code in [200, 201]:
                print(f"✅ Engine '{engine_id}' created successfully")
                return True
            elif create_response.status_code == 400:
                # Check if error is due to existing engine ID
                try:
                    error_response = create_response.json()
                    if (isinstance(error_response, dict) and 
                        'error' in error_response and 
                        error_response['error'].get('code') == 'engine_id_exists'):
                        print(f"✅ Engine '{engine_id}' already exists (detected during creation), using existing engine")
                        return True
                except json.JSONDecodeError:
                    pass
                print(f"❌ Failed to create engine: {create_response.text}")
                return False
            else:
                print(f"❌ Failed to create engine: {create_response.text}")
                return False
                
        except Exception as e:
            print(f"❌ Error setting up model engine: {e}")
            return False
    
    def get_available_agents(self) -> List[Dict[str, Any]]:
        """Get list of available agents from the server"""
        try:
            response = self.session.get(f"{self.base_url}/v1/agents")
            if response.status_code == 200:
                agents_response = response.json()
                
                # Handle different response formats
                if isinstance(agents_response, list):
                    # Direct list of agents
                    self.available_agents = agents_response
                    return agents_response
                elif isinstance(agents_response, dict):
                    # Check for different possible keys
                    if 'data' in agents_response:
                        # Format: {"count": N, "data": [...], "success": true}
                        agents = agents_response['data']
                        self.available_agents = agents
                        return agents
                    elif 'agents' in agents_response:
                        # Format: {"agents": [...]}
                        agents = agents_response['agents']
                        self.available_agents = agents
                        return agents
                    elif 'results' in agents_response:
                        # Format: {"results": [...]}
                        agents = agents_response['results']
                        self.available_agents = agents
                        return agents
                    else:
                        print(f"❌ Unexpected response format: {agents_response}")
                        return []
            
            print(f"❌ Failed to get agents: {response.status_code} - {response.text}")
            return []
            
        except Exception as e:
            print(f"❌ Error getting agents: {e}")
            return []
    
    def display_available_agents(self):
        """Display available agents with their details"""
        if not self.available_agents:
            print("❌ No agents available")
            return
        
        print("\n🤖 Available Agents:")
        print("=" * 60)
        
        for i, agent in enumerate(self.available_agents, 1):
            if isinstance(agent, dict):
                name = agent.get('name', 'Unknown')
                agent_type = agent.get('type', 'Unknown')
                role = agent.get('role', 'No description')
                status = agent.get('status', 'Unknown')
                
                print(f"{i}. {name}")
                print(f"   Type: {agent_type}")
                print(f"   Role: {role}")
                print(f"   Status: {status}")
                print()
    
    def select_agent(self, agent_index: int) -> bool:
        """Select an agent by index"""
        if 0 <= agent_index < len(self.available_agents):
            self.current_agent = self.available_agents[agent_index]
            print(f"✅ Selected agent: {self.current_agent.get('name', 'Unknown')}")
            return True
        else:
            print("❌ Invalid agent selection")
            return False
    
    def send_message_to_agent(self, message: str) -> str:
        """Send a message to the currently selected agent"""
        if not self.current_agent:
            return "❌ No agent selected"
        
        try:
            agent_name = self.current_agent.get('name')
            agent_id = self.current_agent.get('id', agent_name)
            
            # Try different endpoint patterns
            endpoints = [
                f"{self.base_url}/v1/agents/{agent_id}/execute",
                f"{self.base_url}/v1/agents/{agent_name}/execute",
                f"{self.base_url}/agents/{agent_id}/execute",
                f"{self.base_url}/agents/{agent_name}/execute"
            ]
            
            request_data = {
                "message": message,
                "conversation_id": f"demo-{int(time.time())}",
                "parameters": {
                    "temperature": 0.7,
                    "max_tokens": 512
                }
            }
            
            for endpoint in endpoints:
                try:
                    response = self.session.post(
                        endpoint,
                        json=request_data,
                        headers={"Content-Type": "application/json"},
                        timeout=30
                    )
                    
                    if response.status_code == 200:
                        result = response.json()
                        if isinstance(result, dict):
                            if 'response' in result:
                                return result['response']
                            elif 'message' in result:
                                return result['message']
                            elif 'content' in result:
                                return result['content']
                            else:
                                return json.dumps(result, indent=2)
                        else:
                            return str(result)
                    elif response.status_code == 404:
                        continue  # Try next endpoint
                    else:
                        print(f"⚠️  Agent response error ({response.status_code}): {response.text}")
                        continue
                        
                except requests.exceptions.RequestException as e:
                    continue  # Try next endpoint
              # If all endpoints fail, try direct chat completion
            return self.fallback_to_chat_completion(message)
            
        except Exception as e:
            return f"❌ Error sending message: {e}"
    
    def fallback_to_chat_completion(self, message: str) -> str:
        """Fallback to direct chat completion API"""
        try:
            # Get system prompt from agent config
            system_prompt = self.current_agent.get('system_prompt', 'You are a helpful AI assistant.')
            
            # Prepare messages for chat completion
            messages = [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": message}
            ]
            
            request_data = {
                "model": "qwen-0.6b-demo",
                "messages": messages,
                "temperature": 0.7,
                "max_tokens": 512,
                "stream": False
            }
            
            response = self.session.post(
                f"{self.base_url}/v1/chat/completions",
                json=request_data,
                headers={"Content-Type": "application/json"},
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                if "choices" in result and len(result["choices"]) > 0:
                    return result["choices"][0]["message"]["content"]
            
            return f"❌ Fallback failed: {response.status_code} - {response.text}"
            
        except Exception as e:
            return f"❌ Fallback error: {e}"
    
    def send_message_to_agent_streaming(self, message: str) -> Generator[str, None, None]:
        """Send a message to the currently selected agent with streaming response"""
        if not self.current_agent:
            yield "❌ No agent selected"
            return
        
        try:
            agent_name = self.current_agent.get('name')
            agent_id = self.current_agent.get('id', agent_name)
            
            # Try different endpoint patterns
            endpoints = [
                f"{self.base_url}/v1/agents/{agent_id}/execute",
                f"{self.base_url}/v1/agents/{agent_name}/execute",
                f"{self.base_url}/agents/{agent_id}/execute",
                f"{self.base_url}/agents/{agent_name}/execute"
            ]
            
            request_data = {
                "message": message,
                "conversation_id": f"demo-{int(time.time())}",
                "parameters": {
                    "temperature": 0.7,
                    "max_tokens": 512,
                    "stream": True
                }
            }
            
            for endpoint in endpoints:
                try:
                    response = self.session.post(
                        endpoint,
                        json=request_data,
                        headers={
                            "Content-Type": "application/json",
                            "Accept": "text/event-stream"
                        },
                        timeout=30,
                        stream=True
                    )
                    
                    if response.status_code == 200:
                        yield from self._parse_streaming_response(response)
                        return
                    elif response.status_code == 404:
                        continue  # Try next endpoint
                    else:
                        continue
                        
                except requests.exceptions.RequestException as e:
                    continue  # Try next endpoint
            
            # If all endpoints fail, try direct chat completion with streaming
            yield from self.fallback_to_chat_completion_streaming(message)
            
        except Exception as e:
            yield f"❌ Error sending message: {e}"
    
    def fallback_to_chat_completion_streaming(self, message: str) -> Generator[str, None, None]:
        """Fallback to direct chat completion API with streaming"""
        try:
            # Get system prompt from agent config
            system_prompt = self.current_agent.get('system_prompt', 'You are a helpful AI assistant.')
            
            # Prepare messages for chat completion
            messages = [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": message}
            ]
            
            request_data = {
                "model": "qwen-0.6b-demo",
                "messages": messages,
                "temperature": 0.7,
                "max_tokens": 512,
                "stream": True
            }
            
            response = self.session.post(
                f"{self.base_url}/v1/chat/completions",
                json=request_data,
                headers={
                    "Content-Type": "application/json",
                    "Accept": "text/event-stream"
                },
                timeout=30,
                stream=True
            )
            
            if response.status_code == 200:
                yield from self._parse_streaming_response(response)
            else:
                yield f"❌ Fallback failed: {response.status_code} - {response.text}"
                
        except Exception as e:
            yield f"❌ Fallback error: {e}"
    
    def _parse_streaming_response(self, response: requests.Response) -> Generator[str, None, None]:
        """Parse Server-Sent Events streaming response"""
        try:
            buffer = ""
            for chunk in response.iter_content(chunk_size=1024, decode_unicode=True):
                if chunk:
                    buffer += chunk
                    
                    # Process complete lines
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.strip()
                        
                        if not line:
                            continue
                        
                        # Handle SSE format
                        if line.startswith('data: '):
                            data_content = line[6:]  # Remove 'data: ' prefix
                            
                            # Check for end of stream
                            if data_content == '[DONE]':
                                return
                            
                            try:
                                # Parse JSON data
                                data = json.loads(data_content)
                                
                                # Handle different response formats
                                if isinstance(data, dict):
                                    # OpenAI-style streaming format
                                    if 'choices' in data and len(data['choices']) > 0:
                                        choice = data['choices'][0]
                                        if 'delta' in choice and 'content' in choice['delta']:
                                            content = choice['delta']['content']
                                            if content:
                                                yield content
                                    # Custom agent response format
                                    elif 'content' in data:
                                        yield data['content']
                                    elif 'response' in data:
                                        yield data['response']
                                    elif 'message' in data:
                                        yield data['message']
                                    elif 'token' in data:
                                        yield data['token']
                                else:
                                    # Plain text response
                                    yield str(data)
                                    
                            except json.JSONDecodeError:
                                # If not JSON, treat as plain text
                                if data_content and data_content != '[DONE]':
                                    yield data_content
                        elif line.startswith('event: '):
                            # Handle event types if needed
                            continue
                        else:
                            # Handle non-SSE streaming (plain text chunks)
                            try:
                                data = json.loads(line)
                                if isinstance(data, dict):
                                    if 'choices' in data and len(data['choices']) > 0:
                                        choice = data['choices'][0]
                                        if 'delta' in choice and 'content' in choice['delta']:
                                            content = choice['delta']['content']
                                            if content:
                                                yield content
                                    elif 'content' in data:
                                        yield data['content']
                                    elif 'response' in data:
                                        yield data['response']
                            except json.JSONDecodeError:
                                # If not JSON, treat as plain text
                                if line:
                                    yield line
                                    
        except Exception as e:
            yield f"❌ Streaming parse error: {e}"
    
    def send_message_with_streaming_choice(self, message: str, use_streaming: bool = None) -> str:
        """Send message with option to use streaming or non-streaming"""
        if use_streaming is None:
            use_streaming = self.streaming_enabled
            
        if use_streaming:
            # Collect all streaming chunks into a single response
            full_response = ""
            for chunk in self.send_message_to_agent_streaming(message):
                full_response += chunk
            return full_response
        else:
            return self.send_message_to_agent(message)
    
    def display_streaming_response(self, message: str) -> str:
        """Display streaming response in real-time"""
        if not self.streaming_enabled:
            return self.send_message_to_agent(message)
        
        full_response = ""
        for chunk in self.send_message_to_agent_streaming(message):
            if chunk:
                print(chunk, end="", flush=True)
                full_response += chunk
        
        return full_response
    
    def run_demo_scenarios(self):
        """Run predefined demo scenarios"""
        scenarios = [
            {
                "agent_type": "research_assistant",
                "message": "What are the key benefits of using large language models in software development?",
                "description": "Research Assistant - Technical Research"
            },
            {
                "agent_type": "code_assistant", 
                "message": "Write a Python function to calculate the factorial of a number using recursion",
                "description": "Code Assistant - Programming Task"
            },
            {
                "agent_type": "data_analyst",
                "message": "Explain the difference between mean, median, and mode in statistics",
                "description": "Data Analyst - Statistical Explanation"
            },
            {
                "agent_type": "content_creator",
                "message": "Write a brief introduction for a blog post about artificial intelligence",
                "description": "Content Creator - Creative Writing"
            },
            {
                "agent_type": "response_test_agent",
                "message": "Hello! Can you help me understand how you work?",
                "description": "Test Agent - General Interaction"            }
        ]
        
        print("\n🎯 Running Demo Scenarios")
        print("=" * 60)
        
        for scenario in scenarios:
            # Find agent by type
            target_agent = None
            for i, agent in enumerate(self.available_agents):
                if agent.get('type') == scenario['agent_type'] or agent.get('name') == scenario['agent_type']:
                    target_agent = i
                    break
            
            if target_agent is not None:
                print(f"\n📋 {scenario['description']}")
                print(f"🤖 Agent: {self.available_agents[target_agent].get('name', 'Unknown')}")
                print(f"💬 Message: {scenario['message']}")
                print("📝 Response:")
                print("   ", end="", flush=True)
                
                self.select_agent(target_agent)
                
                if self.streaming_enabled:
                    response = self.display_streaming_response(scenario['message'])
                    print()  # Add newline after streaming
                else:
                    response = self.send_message_to_agent(scenario['message'])
                    print(response)
                
                print("-" * 40)
                time.sleep(1)  # Brief pause between scenarios
            else:
                print(f"⚠️  Agent type '{scenario['agent_type']}' not found")
    
    def interactive_mode(self):
        """Run interactive chat mode with selected agent"""
        if not self.current_agent:
            print("❌ No agent selected for interactive mode")
            return
        
        agent_name = self.current_agent.get('name', 'Unknown')
        print(f"\n💬 Interactive Chat with {agent_name}")
        print("=" * 50)
        print("Type 'quit', 'exit', or 'back' to return to main menu")
        print("Type 'clear' to clear conversation history")
        print("Type 'stream on/off' to toggle streaming mode")
        print(f"Streaming is currently: {'✅ ON' if self.streaming_enabled else '❌ OFF'}")
        print("-" * 50)
        
        while True:
            try:
                user_input = input(f"\nYou: ").strip()
                
                if not user_input:
                    continue
                
                if user_input.lower() in ['quit', 'exit', 'back']:
                    print("👋 Returning to main menu...")
                    break
                
                if user_input.lower() == 'clear':
                    self.conversation_history.clear()
                    print("🧹 Conversation history cleared!")
                    continue
                
                if user_input.lower() in ['stream on', 'streaming on']:
                    self.streaming_enabled = True
                    print("✅ Streaming mode enabled")
                    continue
                
                if user_input.lower() in ['stream off', 'streaming off']:
                    self.streaming_enabled = False
                    print("❌ Streaming mode disabled")
                    continue
                
                print(f"\n🤖 {agent_name}: ", end="", flush=True)
                
                if self.streaming_enabled:
                    response = self.display_streaming_response(user_input)
                    print()  # Add newline after streaming response
                else:
                    response = self.send_message_to_agent(user_input)
                    print(response)
                
                # Store in conversation history
                self.conversation_history.append({
                    "timestamp": datetime.now().isoformat(),
                    "user": user_input,
                    "assistant": response
                })
                
            except KeyboardInterrupt:
                print("\n👋 Returning to main menu...")
                break
            except EOFError:
                print("\n👋 Returning to main menu...")
                break
    
    def run(self):
        """Main demo execution"""
        print("🚀 Kolosal Server Agent Demo")
        print("=" * 40)
        
        # Check server health
        print("🔍 Checking server connection...")
        if not self.check_server_health():
            print("❌ Cannot connect to Kolosal Server")
            print(f"Make sure the server is running at {self.base_url}")
            return
        
        print("✅ Server is running")
        
        # Setup model engine
        print("\n🔧 Setting up model engine...")
        if not self.setup_model_engine():
            print("❌ Failed to setup model engine")
            return
        
        # Get available agents
        print("\n📋 Getting available agents...")
        agents = self.get_available_agents()
        if not agents:
            print("❌ No agents available")
            return
        
        print(f"✅ Found {len(agents)} agents")
          # Main menu loop
        while True:
            streaming_status = "✅ ON" if self.streaming_enabled else "❌ OFF"
            print(f"\n🎯 Demo Options (Streaming: {streaming_status}):")
            print("1. View available agents")
            print("2. Run demo scenarios")
            print("3. Interactive chat with agent")
            print("4. Show agent configuration")
            print("5. Toggle streaming mode")
            print("6. Exit")
            
            try:
                choice = input("\nSelect option (1-6): ").strip()
                
                if choice == '1':
                    self.display_available_agents()
                
                elif choice == '2':
                    self.run_demo_scenarios()
                
                elif choice == '3':
                    self.display_available_agents()
                    try:
                        agent_num = int(input("Select agent number for chat: ")) - 1
                        if self.select_agent(agent_num):
                            self.interactive_mode()
                    except ValueError:
                        print("❌ Invalid agent number")
                
                elif choice == '4':
                    print("\n📄 Agent Configuration Summary:")
                    print("=" * 50)
                    if self.agent_config:
                        if 'agents' in self.agent_config:
                            for agent in self.agent_config['agents']:
                                name = agent.get('name', 'Unknown')
                                agent_type = agent.get('type', 'Unknown')
                                role = agent.get('role', 'No description')
                                print(f"\n• {name} ({agent_type})")
                                print(f"  Role: {role}")
                                if 'llm_config' in agent:
                                    llm_config = agent['llm_config']
                                    print(f"  Model: {llm_config.get('model_name', 'Unknown')}")
                                    print(f"  Temperature: {llm_config.get('temperature', 'Unknown')}")
                                    print(f"  Max Tokens: {llm_config.get('max_tokens', 'Unknown')}")
                    else:
                        print("❌ No agent configuration loaded")
                
                elif choice == '5':
                    self.streaming_enabled = not self.streaming_enabled
                    status = "✅ enabled" if self.streaming_enabled else "❌ disabled"
                    print(f"🔄 Streaming mode {status}")
                
                elif choice == '6':
                    print("\n👋 Goodbye!")
                    break
                
                else:
                    print("❌ Invalid option. Please select 1-6.")
                    
            except KeyboardInterrupt:
                print("\n\n👋 Demo interrupted. Goodbye!")
                break
            except EOFError:
                print("\n\n👋 Goodbye!")
                break

def main():
    parser = argparse.ArgumentParser(description="Agent Demo for Kolosal Server")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--config", default="config/agents.yaml", help="Agent config file path")
    parser.add_argument("--no-streaming", action="store_true", help="Disable streaming mode (default: streaming enabled)")
    
    args = parser.parse_args()
    
    demo = AgentDemo(args.host, args.port, args.config)
    if args.no_streaming:
        demo.streaming_enabled = False
    demo.run()

if __name__ == "__main__":
    main()
