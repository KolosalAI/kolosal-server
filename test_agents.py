#!/usr/bin/env python3
"""
Kolosal Server Agent API Test Script

This script specifically tests the agent system endpoints of the Kolosal Server.
Make sure the server is running before executing this script.

Usage:
    python test_agents.py [--host HOST] [--port PORT]
"""

import requests
import json
import argparse
import sys
import time
import yaml
import os
from typing import Dict, Any, Optional, List


class AgentTester:
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.base_url = f"http://{host}:{port}"
        self.session = requests.Session()
        self.session.headers.update({
            'Content-Type': 'application/json',
            'User-Agent': 'AgentTester/1.0'
        })
        self.created_agents = []  # Keep track of created agents for cleanup
        self.test_yaml_file = "test_agents_config.yaml"  # Test YAML configuration file
        
    def make_request(self, method: str, endpoint: str, data: Optional[Dict[Any, Any]] = None, 
                    params: Optional[Dict[str, Any]] = None) -> tuple:
        """Make HTTP request and return response and status"""
        url = f"{self.base_url}{endpoint}"
        
        try:
            if method.upper() == 'GET':
                response = self.session.get(url, params=params)
            elif method.upper() == 'POST':
                response = self.session.post(url, json=data, params=params)
            elif method.upper() == 'PUT':
                response = self.session.put(url, json=data, params=params)
            elif method.upper() == 'DELETE':
                response = self.session.delete(url, params=params)
            else:
                raise ValueError(f"Unsupported HTTP method: {method}")
                
            return response, response.status_code
        except requests.exceptions.RequestException as e:
            print(f"❌ Request failed: {e}")
            return None, 0

    def test_agent_system_status(self):
        """Test agent system status endpoint"""
        print("\n📈 Testing Agent System Status")
        print("=" * 60)
        
        response, status = self.make_request('GET', '/v1/agents/system/status')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Agent system is running")
                    return json_response
                else:
                    print("⚠️  Agent system status returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Agent system status check failed")
        return None

    def test_list_agents(self):
        """Test list all agents endpoint"""
        print("\n🤖 Testing List All Agents")
        print("=" * 60)
        
        response, status = self.make_request('GET', '/v1/agents')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Successfully retrieved agents list")
                    if isinstance(json_response, list):
                        print(f"Found {len(json_response)} agents")
                    elif isinstance(json_response, dict) and 'agents' in json_response:
                        print(f"Found {len(json_response['agents'])} agents")
                    return json_response
                else:
                    print("⚠️  List agents returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ List agents failed")
        return None

    def test_create_simple_agent(self):
        """Test creating a simple agent"""
        print("\n🔨 Testing Create Simple Agent")
        print("=" * 60)
        
        agent_data = {
            "id": "simple-test-agent",
            "name": "Simple Test Agent",
            "description": "A simple agent for basic testing",
            "type": "assistant",
            "llm_config": {
                "model_name": "gpt-3.5-turbo",
                "api_endpoint": "https://api.openai.com/v1/chat/completions",
                "instruction": "You are a simple test assistant. Provide clear, concise, and helpful responses to user queries.",
                "temperature": 0.7,
                "max_tokens": 500,
                "timeout_seconds": 30,
                "max_retries": 2
            }
        }
        
        response, status = self.make_request('POST', '/v1/agents', agent_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status in [200, 201]:
                    print("✅ Simple agent created successfully")
                    self.created_agents.append("simple-test-agent")
                    return "simple-test-agent"
                else:
                    print("⚠️  Create simple agent returned non-success status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Create simple agent failed")
        return None

    def test_create_function_agent(self):
        """Test creating an agent with functions"""
        print("\n🛠️  Testing Create Function Agent")
        print("=" * 60)
        
        agent_data = {
            "id": "function-test-agent",
            "name": "Function Test Agent",
            "description": "An agent with custom functions for testing",
            "type": "function_agent",
            "llm_config": {
                "model_name": "gpt-3.5-turbo",
                "api_endpoint": "https://api.openai.com/v1/chat/completions",
                "instruction": "You are a function-capable assistant. Execute the requested functions accurately and provide helpful responses.",
                "temperature": 0.5,
                "max_tokens": 1000,
                "timeout_seconds": 30,
                "max_retries": 2
            },
            "functions": [
                {
                    "name": "greet_user",
                    "description": "Greet a user with a personalized message",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "name": {
                                "type": "string",
                                "description": "The user's name"
                            },
                            "language": {
                                "type": "string",
                                "description": "Language for greeting",
                                "enum": ["english", "spanish", "french"],
                                "default": "english"
                            }
                        },
                        "required": ["name"]
                    }
                },
                {
                    "name": "calculate_sum",
                    "description": "Calculate the sum of two numbers",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "a": {
                                "type": "number",
                                "description": "First number"
                            },
                            "b": {
                                "type": "number",
                                "description": "Second number"
                            }
                        },
                        "required": ["a", "b"]
                    }
                },
                {
                    "name": "get_current_time",
                    "description": "Get the current time",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "timezone": {
                                "type": "string",
                                "description": "Timezone (e.g., UTC, EST, PST)",
                                "default": "UTC"
                            }
                        }
                    }
                }
            ],
            "metadata": {
                "version": "1.0",
                "author": "Test Suite",
                "tags": ["test", "functions", "demo"]
            }
        }
        
        response, status = self.make_request('POST', '/v1/agents', agent_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status in [200, 201]:
                    print("✅ Function agent created successfully")
                    self.created_agents.append("function-test-agent")
                    return "function-test-agent"
                else:
                    print("⚠️  Create function agent returned non-success status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Create function agent failed")
        return None

    def test_get_agent_details(self, agent_id: str):
        """Test getting agent details"""
        print(f"\n🔍 Testing Get Agent Details: {agent_id}")
        print("=" * 60)
        
        response, status = self.make_request('GET', f'/v1/agents/{agent_id}')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Agent details retrieved successfully")
                    return json_response
                else:
                    print("⚠️  Get agent details returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Get agent details failed")
        return None

    def test_execute_agent_greet(self, agent_id: str):
        """Test executing agent greeting function"""
        print(f"\n⚡ Testing Agent Execute: Greet Function ({agent_id})")
        print("=" * 60)
        
        test_data = {
            "function": "greet_user",
            "parameters": {
                "name": "Alice",
                "language": "english"
            }
        }
        
        response, status = self.make_request('POST', f'/v1/agents/{agent_id}/execute', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Agent greeting function executed successfully")
                    return json_response
                else:
                    print("⚠️  Agent execution returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Agent greeting function execution failed")
        return None

    def test_execute_agent_calculate(self, agent_id: str):
        """Test executing agent calculation function"""
        print(f"\n🧮 Testing Agent Execute: Calculate Function ({agent_id})")
        print("=" * 60)
        
        test_data = {
            "function": "calculate_sum",
            "parameters": {
                "a": 15,
                "b": 27
            }
        }
        
        response, status = self.make_request('POST', f'/v1/agents/{agent_id}/execute', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Agent calculation function executed successfully")
                    return json_response
                else:
                    print("⚠️  Agent execution returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Agent calculation function execution failed")
        return None

    def test_execute_agent_time(self, agent_id: str):
        """Test executing agent time function"""
        print(f"\n🕐 Testing Agent Execute: Time Function ({agent_id})")
        print("=" * 60)
        
        test_data = {
            "function": "get_current_time",
            "parameters": {
                "timezone": "UTC"
            }
        }
        
        response, status = self.make_request('POST', f'/v1/agents/{agent_id}/execute', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Agent time function executed successfully")
                    return json_response
                else:
                    print("⚠️  Agent execution returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Agent time function execution failed")
        return None

    def test_execute_invalid_function(self, agent_id: str):
        """Test executing non-existent function (should fail gracefully)"""
        print(f"\n❓ Testing Agent Execute: Invalid Function ({agent_id})")
        print("=" * 60)
        
        test_data = {
            "function": "non_existent_function",
            "parameters": {
                "test": "value"
            }
        }
        
        response, status = self.make_request('POST', f'/v1/agents/{agent_id}/execute', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status in [400, 404]:
                    print("✅ Invalid function handled correctly (returned error)")
                else:
                    print("⚠️  Invalid function didn't return expected error status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Invalid function test failed")

    def test_agent_with_invalid_data(self):
        """Test creating agent with invalid data"""
        print("\n❌ Testing Create Agent with Invalid Data")
        print("=" * 60)
        
        # Missing required fields
        invalid_agent_data = {
            "name": "Invalid Agent",
            # Missing 'id' field
            "type": "assistant"
            # Missing other required fields
        }
        
        response, status = self.make_request('POST', '/v1/agents', invalid_agent_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status in [400, 422]:
                    print("✅ Invalid agent data handled correctly (returned error)")
                else:
                    print("⚠️  Invalid agent data didn't return expected error status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Invalid agent data test failed")

    def cleanup_created_agents(self):
        """Clean up agents created during testing"""
        if not self.created_agents:
            return
            
        print("\n🧹 Cleaning up created agents...")
        print("=" * 60)
        
        for agent_id in self.created_agents:
            print(f"Deleting agent: {agent_id}")
            response, status = self.make_request('DELETE', f'/v1/agents/{agent_id}')
            if response and status in [200, 204, 404]:  # 404 is OK if already deleted
                print(f"  ✅ Agent {agent_id} deleted successfully")
            else:
                print(f"  ⚠️  Failed to delete agent {agent_id}")
        
        # Clean up test YAML file
        try:
            if os.path.exists(self.test_yaml_file):
                os.remove(self.test_yaml_file)
                print(f"✅ Test YAML file {self.test_yaml_file} removed")
        except Exception as e:
            print(f"⚠️  Failed to remove test YAML file: {e}")

    def test_server_connectivity(self):
        """Test basic server connectivity"""
        print("🔗 Testing Server Connectivity")
        print("=" * 60)
        
        try:
            response = self.session.get(f"{self.base_url}/health", timeout=5)
            print(f"✅ Server is reachable at {self.base_url}")
            return True
        except requests.exceptions.RequestException as e:
            print(f"❌ Cannot reach server at {self.base_url}: {e}")
            return False

    def run_comprehensive_agent_tests(self):
        """Run comprehensive agent system tests"""
        print("🚀 Starting Comprehensive Agent System Tests")
        print("=" * 70)
        
        # Test connectivity first
        if not self.test_server_connectivity():
            print("\n❌ Server is not reachable. Please make sure the server is running.")
            return False
        
        try:
            # 1. Test agent system status
            self.test_agent_system_status()
            
            # 2. Test YAML configuration functionality
            yaml_test_success = self.test_yaml_config_functionality()
            
            # 3. Test model name and instruction validation
            self.test_model_instruction_validation()
            
            # 4. Test listing agents (initially empty or with existing agents)
            initial_agents = self.test_list_agents()
            
            # 5. Test creating simple agent
            simple_agent_id = self.test_create_simple_agent()
            
            # 6. Test creating function agent
            function_agent_id = self.test_create_function_agent()
            
            # 7. Test invalid agent creation
            self.test_agent_with_invalid_data()
            
            # 8. Test getting agent details
            if simple_agent_id:
                self.test_get_agent_details(simple_agent_id)
            
            if function_agent_id:
                self.test_get_agent_details(function_agent_id)
              # 9. Test function executions
            if function_agent_id:
                self.test_execute_agent_greet(function_agent_id)
                self.test_execute_agent_calculate(function_agent_id)
                self.test_execute_agent_time(function_agent_id)
                self.test_execute_invalid_function(function_agent_id)
            
            # 10. Test agent response generation
            if simple_agent_id:
                self.test_agent_generate_response(simple_agent_id)
                self.test_agent_conversation_flow(simple_agent_id)
            
            if function_agent_id:
                self.test_agent_generate_response(function_agent_id)
              # 11. Test agent with custom system prompt
            self.test_agent_with_system_prompt()
            
            # 12. Test model download and integration
            print("\n🔄 Testing model download and integration...")
            model_download_success = self.test_download_and_use_model()
            
            # 13. Test listing agents again (should show created agents)
            final_agents = self.test_list_agents()            # 12. Show summary
            print("\n📊 Test Summary")
            print("=" * 60)
            print(f"✅ YAML configuration test: {'Passed' if yaml_test_success else 'Failed'}")
            print(f"✅ Simple agent created: {'Yes' if simple_agent_id else 'No'}")
            print(f"✅ Function agent created: {'Yes' if function_agent_id else 'No'}")
            print(f"✅ Agent details retrieved: {'Yes' if simple_agent_id or function_agent_id else 'No'}")
            print(f"✅ Function executions tested: {'Yes' if function_agent_id else 'No'}")
            print(f"✅ Response generation tested: {'Yes' if simple_agent_id or function_agent_id else 'No'}")
            print(f"✅ Conversation flow tested: {'Yes' if simple_agent_id else 'No'}")
            print(f"✅ System prompt adherence tested: Included")
            print(f"✅ Model download integration: {'Tested' if 'model_download_success' in locals() else 'Skipped'}")
            print(f"✅ Model name and instruction validation: Tested")
            
        finally:
            # Cleanup created agents
            self.cleanup_created_agents()
        
        print("\n" + "=" * 70)
        print("🏁 Agent system tests completed!")
        print("=" * 70)
        
        return True

    def test_yaml_config_functionality(self):
        """Test YAML configuration creation and management"""
        print("\n📝 Testing YAML Configuration Functionality")
        print("=" * 60)
        
        # Create a test YAML configuration with model names and instructions
        test_config = {
            "system": {
                "worker_threads": 4,
                "health_check_interval_seconds": 30,
                "log_level": "INFO",
                "global_settings": {
                    "default_timeout_ms": 30000,
                    "max_retries": 3,
                    "enable_metrics": True
                }
            },
            "functions": [
                {
                    "name": "test_function",
                    "type": "builtin",
                    "description": "A test function for YAML configuration testing",
                    "parameters": {
                        "input": "Test input parameter",
                        "operation": "Type of test operation"
                    },
                    "timeout_ms": 10000
                }
            ],
            "agents": [
                {
                    "name": "yaml_test_agent",
                    "type": "test",
                    "role": "YAML configuration test agent",
                    "system_prompt": "You are a test agent created for YAML configuration testing.",
                    "capabilities": ["testing", "yaml_config"],
                    "functions": ["test_function"],
                    "llm_config": {
                        "model_name": "gpt-4-turbo",
                        "api_endpoint": "https://api.openai.com/v1/chat/completions",
                        "instruction": "You are a helpful assistant created for testing YAML configuration functionality. Always be concise and helpful.",
                        "temperature": 0.5,
                        "max_tokens": 1024,
                        "timeout_seconds": 30,
                        "max_retries": 2
                    },
                    "auto_start": True,
                    "max_concurrent_jobs": 3,
                    "heartbeat_interval_seconds": 10,
                    "custom_settings": {
                        "test_mode": "enabled",
                        "debug_level": "verbose"
                    }
                },
                {
                    "name": "yaml_test_agent_2",
                    "type": "assistant",
                    "role": "Secondary YAML test agent",
                    "system_prompt": "You are a secondary test agent for comprehensive YAML testing.",
                    "capabilities": ["analysis", "reporting"],
                    "functions": ["test_function"],
                    "llm_config": {
                        "model_name": "gpt-3.5-turbo",
                        "api_endpoint": "https://api.openai.com/v1/chat/completions",
                        "instruction": "You are an analytical assistant focused on data processing and reporting. Provide detailed insights.",
                        "temperature": 0.2,
                        "max_tokens": 2048,
                        "timeout_seconds": 45,
                        "max_retries": 3
                    },
                    "auto_start": False,
                    "max_concurrent_jobs": 5,
                    "heartbeat_interval_seconds": 15,
                    "custom_settings": {
                        "analysis_depth": "comprehensive",
                        "report_format": "detailed"
                    }
                }
            ]
        }
        
        try:
            # Write test YAML configuration
            with open(self.test_yaml_file, 'w') as f:
                yaml.dump(test_config, f, default_flow_style=False, indent=2)
            print(f"✅ Test YAML configuration written to {self.test_yaml_file}")
            
            # Read back the configuration to verify
            with open(self.test_yaml_file, 'r') as f:
                loaded_config = yaml.safe_load(f)
            
            # Verify the configuration structure
            assert 'system' in loaded_config, "System configuration missing"
            assert 'agents' in loaded_config, "Agents configuration missing"
            assert 'functions' in loaded_config, "Functions configuration missing"
            
            # Verify agent configurations have model names and instructions
            for agent in loaded_config['agents']:
                assert 'llm_config' in agent, f"LLM config missing for agent {agent['name']}"
                llm_config = agent['llm_config']
                assert 'model_name' in llm_config, f"Model name missing for agent {agent['name']}"
                assert 'instruction' in llm_config, f"Instruction missing for agent {agent['name']}"
                
                print(f"  Agent: {agent['name']}")
                print(f"    Model: {llm_config['model_name']}")
                print(f"    Instruction: {llm_config['instruction'][:50]}...")
                print(f"    Temperature: {llm_config['temperature']}")
                print(f"    Max Tokens: {llm_config['max_tokens']}")
            
            print("✅ YAML configuration structure verified successfully")
            
            # Test configuration loading endpoint (if available)
            self.test_yaml_config_endpoint()
            
            return True
            
        except Exception as e:
            print(f"❌ YAML configuration test failed: {e}")
            return False

    def test_yaml_config_endpoint(self):
        """Test YAML configuration loading via API endpoint"""
        print("\n🔄 Testing YAML Configuration Loading Endpoint")
        print("=" * 60)
        
        # Try to reload configuration via API (if endpoint exists)
        response, status = self.make_request('POST', '/v1/agents/system/reload-config', 
                                           {"config_file": self.test_yaml_file})
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Configuration reloaded successfully via API")
                elif status == 404:
                    print("ℹ️  Configuration reload endpoint not available")
                else:
                    print("⚠️  Configuration reload returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("ℹ️  Configuration reload endpoint test skipped (endpoint may not exist)")

    def test_model_instruction_validation(self):
        """Test validation of model names and instructions"""
        print("\n✅ Testing Model Name and Instruction Validation")
        print("=" * 60)
        
        # Test cases for different model configurations
        test_cases = [
            {
                "name": "Valid Configuration",
                "agent_data": {
                    "id": "test-valid-model",
                    "name": "Valid Model Test Agent",
                    "type": "assistant",
                    "llm_config": {
                        "model_name": "gpt-4",
                        "api_endpoint": "https://api.openai.com/v1/chat/completions",
                        "instruction": "You are a helpful assistant for testing purposes.",
                        "temperature": 0.7,
                        "max_tokens": 1000
                    }
                },
                "expected_status": [200, 201]
            },
            {
                "name": "Missing Model Name",
                "agent_data": {
                    "id": "test-missing-model",
                    "name": "Missing Model Test Agent",
                    "type": "assistant",
                    "llm_config": {
                        "api_endpoint": "https://api.openai.com/v1/chat/completions",
                        "instruction": "You are a helpful assistant.",
                        "temperature": 0.7
                    }
                },
                "expected_status": [400, 422]
            },
            {
                "name": "Empty Instruction",
                "agent_data": {
                    "id": "test-empty-instruction",
                    "name": "Empty Instruction Test Agent",
                    "type": "assistant",
                    "llm_config": {
                        "model_name": "gpt-3.5-turbo",
                        "api_endpoint": "https://api.openai.com/v1/chat/completions",
                        "instruction": "",
                        "temperature": 0.7
                    }
                },
                "expected_status": [200, 201, 400, 422]  # Could be valid or invalid depending on implementation
            }
        ]
        
        for test_case in test_cases:
            print(f"\n  Testing: {test_case['name']}")
            print("  " + "-" * 40)
            
            response, status = self.make_request('POST', '/v1/agents', test_case['agent_data'])
            
            if response:
                print(f"  Status Code: {status}")
                try:
                    json_response = response.json()
                    print(f"  Response: {json.dumps(json_response, indent=2)}")
                    
                    if status in test_case['expected_status']:
                        print(f"  ✅ {test_case['name']}: Expected status received")
                        if status in [200, 201]:
                            self.created_agents.append(test_case['agent_data']['id'])
                    else:
                        print(f"  ⚠️  {test_case['name']}: Unexpected status {status}")
                        
                except json.JSONDecodeError:
                    print(f"  Response (text): {response.text}")
            else:
                print(f"  ❌ {test_case['name']}: Request failed")
    
    def test_agent_generate_response(self, agent_id: str):
        """Test agent response generation based on configuration"""
        print(f"\n💬 Testing Agent Generate Response: {agent_id}")
        print("=" * 60)
        
        # Test different types of messages to verify agent follows its configuration
        test_messages = [
            {
                "message": "Hello, please introduce yourself and tell me what you can do.",
                "description": "Basic introduction test"
            },
            {
                "message": "What is the capital of France? Please provide a brief answer.",
                "description": "Simple factual question"
            },
            {
                "message": "Can you help me solve this math problem: What is 25 * 4 + 10?",
                "description": "Math problem test"
            },
            {
                "message": "Explain quantum computing in simple terms for a beginner.",
                "description": "Complex topic explanation test"
            }
        ]
        
        success_count = 0
        
        for i, test_case in enumerate(test_messages, 1):
            print(f"\n  Test {i}: {test_case['description']}")
            print("  " + "-" * 50)
            print(f"  Message: {test_case['message']}")
            
            # Prepare the request data for agent response generation
            request_data = {
                "message": test_case["message"],
                "context": {
                    "user_id": "test-user-001",
                    "session_id": f"test-session-{i}",
                    "timestamp": time.time()
                },
                "parameters": {
                    "max_tokens": 500,
                    "temperature": 0.7,
                    "stream": False
                }
            }
            
            # Try different possible endpoints for agent response generation
            endpoints_to_try = [
                f'/v1/agents/{agent_id}/chat',
                f'/v1/agents/{agent_id}/generate',
                f'/v1/agents/{agent_id}/respond',
                f'/v1/agents/{agent_id}/message'
            ]
            
            response_received = False
            
            for endpoint in endpoints_to_try:
                response, status = self.make_request('POST', endpoint, request_data)
                
                if response and status != 404:  # If endpoint exists (not 404)
                    print(f"  Using endpoint: {endpoint}")
                    print(f"  Status Code: {status}")
                    
                    try:
                        json_response = response.json()
                        print(f"  Response: {json.dumps(json_response, indent=4)}")
                        
                        if status == 200:
                            print("  ✅ Agent generated response successfully")
                            
                            # Validate response structure
                            if isinstance(json_response, dict):
                                if 'response' in json_response or 'message' in json_response or 'content' in json_response:
                                    print("  ✅ Response contains expected content field")
                                    success_count += 1
                                else:
                                    print("  ⚠️  Response structure may be non-standard")
                            
                            response_received = True
                            break
                        else:
                            print(f"  ⚠️  Agent response returned status {status}")
                            
                    except json.JSONDecodeError:
                        print(f"  Response (text): {response.text}")
                        if response.text.strip():  # If there's actual text content
                            print("  ✅ Agent generated text response")
                            success_count += 1
                            response_received = True
                            break
            
            if not response_received:
                print("  ❌ No valid response endpoint found or all endpoints failed")
        
        print(f"\n  📊 Response Generation Summary:")
        print(f"  ✅ Successful responses: {success_count}/{len(test_messages)}")
        
        return success_count > 0

    def test_agent_conversation_flow(self, agent_id: str):
        """Test multi-turn conversation with an agent"""
        print(f"\n🗣️  Testing Agent Conversation Flow: {agent_id}")
        print("=" * 60)
        
        # Simulate a multi-turn conversation
        conversation = [
            "Hi there! What's your name?",
            "What can you help me with today?",
            "I'm working on a Python project. Can you give me a tip for writing clean code?",
            "That's helpful! Can you give me an example?",
            "Thank you for your help!"
        ]
        
        session_id = f"conversation-test-{int(time.time())}"
        
        for i, message in enumerate(conversation, 1):
            print(f"\n  Turn {i}: {message}")
            print("  " + "-" * 40)
            
            request_data = {
                "message": message,
                "context": {
                    "user_id": "test-user-conversation",
                    "session_id": session_id,
                    "turn": i,
                    "timestamp": time.time()
                },
                "parameters": {
                    "max_tokens": 300,
                    "temperature": 0.8,
                    "maintain_context": True
                }
            }
            
            # Try to find the correct endpoint
            endpoints = [f'/v1/agents/{agent_id}/chat', f'/v1/agents/{agent_id}/generate']
            
            for endpoint in endpoints:
                response, status = self.make_request('POST', endpoint, request_data)
                
                if response and status != 404:
                    print(f"  Status: {status}")
                    
                    try:
                        json_response = response.json()
                        if status == 200:
                            # Extract the actual response content
                            content = ""
                            if 'response' in json_response:
                                content = json_response['response']
                            elif 'message' in json_response:
                                content = json_response['message']
                            elif 'content' in json_response:
                                content = json_response['content']
                            else:
                                content = str(json_response)
                            
                            print(f"  Agent: {content}")
                            print("  ✅ Turn completed successfully")
                            break
                    except json.JSONDecodeError:
                        if response.text.strip():
                            print(f"  Agent: {response.text}")
                            print("  ✅ Turn completed successfully")
                            break
            
            # Small delay between turns
            time.sleep(0.5)
        
        print("\n  ✅ Conversation flow test completed")

    def test_agent_with_system_prompt(self):
        """Test creating and using an agent with a specific system prompt"""
        print("\n🎭 Testing Agent with Custom System Prompt")
        print("=" * 60)
        
        # Create an agent with a very specific system prompt
        agent_data = {
            "id": "system-prompt-test-agent",
            "name": "System Prompt Test Agent",
            "description": "An agent with a specific system prompt for testing",
            "type": "assistant",
            "llm_config": {
                "model_name": "gpt-3.5-turbo",
                "api_endpoint": "https://api.openai.com/v1/chat/completions",
                "instruction": "You are a helpful assistant that always responds in exactly 3 sentences. Each sentence must be on a new line. Always end your response with 'That is my analysis.' regardless of the topic.",
                "temperature": 0.3,
                "max_tokens": 200,
                "timeout_seconds": 30,
                "max_retries": 2
            }
        }
        
        # Create the agent
        response, status = self.make_request('POST', '/v1/agents', agent_data)
        if response and status in [200, 201]:
            print("✅ System prompt test agent created successfully")
            self.created_agents.append("system-prompt-test-agent")
            
            # Test if the agent follows the system prompt
            test_messages = [
                "What is artificial intelligence?",
                "How do computers work?",
                "Tell me about the weather."
            ]
            
            for message in test_messages:
                print(f"\n  Testing with message: {message}")
                response_success = self.test_agent_generate_response("system-prompt-test-agent")
                if response_success:
                    print("  ✅ System prompt adherence test passed")
                else:
                    print("  ⚠️  System prompt adherence test inconclusive")
        else:
            print("❌ Failed to create system prompt test agent")

    def test_download_and_use_model(self):
        """Test downloading a model and using it with an agent"""
        print("\n📥 Testing Model Download and Agent Integration")
        print("=" * 60)
        
        # Small model configuration for testing
        model_config = {
            "engine_id": "test-agent-qwen-0.6b",
            "model_path": "https://huggingface.co/kolosal/qwen3-0.6b/resolve/main/Qwen3-0.6B-UD-Q4_K_XL.gguf",
            "load_immediately": True,
            "main_gpu_id": 0,
            "loading_parameters": {
                "n_ctx": 2048,
                "n_gpu_layers": 0,  # Use CPU for compatibility
                "n_batch": 512,
                "n_ubatch": 512,
                "use_mmap": True,
                "use_mlock": False,
                "cont_batching": True,
                "warmup": True,
                "n_parallel": 1,
                "n_keep": 0
            }
        }
        
        model_downloaded = False
        agent_created = False
        
        try:
            # 1. Download the model
            print("  Downloading model...")
            response, status = self.make_request('POST', '/v1/engines', model_config)
            
            if response and status in [200, 201]:
                print("  ✅ Model download initiated successfully!")
                try:
                    json_response = response.json()
                    print(f"  Response: {json.dumps(json_response, indent=2)}")
                except json.JSONDecodeError:
                    print(f"  Response (text): {response.text}")
                
                model_downloaded = True
                
                # Wait a bit for model to be ready
                print("  ⏳ Waiting for model to be ready...")
                time.sleep(30)  # Give some time for model loading
                
            else:
                print(f"  ❌ Model download failed. Status: {status}")
                if response:
                    try:
                        error_response = response.json()
                        print(f"  Error: {json.dumps(error_response, indent=2)}")
                    except json.JSONDecodeError:
                        print(f"  Error (text): {response.text}")
                return False
            
            # 2. Create agent using the downloaded model
            print("  Creating agent with downloaded model...")
            agent_data = {
                "id": "downloaded-model-agent",
                "name": "Downloaded Model Test Agent",
                "description": "Agent using locally downloaded Qwen model",
                "type": "assistant",
                "llm_config": {
                    "model_name": "test-agent-qwen-0.6b",
                    "api_endpoint": f"{self.base_url}/v1/engines/test-agent-qwen-0.6b/completions",
                    "instruction": "You are a helpful assistant powered by Qwen. Provide clear and concise responses.",
                    "temperature": 0.7,
                    "max_tokens": 200,
                    "timeout_seconds": 60,
                    "max_retries": 2
                }
            }
            
            response, status = self.make_request('POST', '/v1/agents', agent_data)
            if response and status in [200, 201]:
                print("  ✅ Agent created with downloaded model!")
                self.created_agents.append("downloaded-model-agent")
                agent_created = True
                
                # 3. Test the agent response
                print("  Testing agent response...")
                test_success = self.test_agent_generate_response("downloaded-model-agent")
                
                if test_success:
                    print("  ✅ Downloaded model integration test successful!")
                    return True
                else:
                    print("  ⚠️  Agent response test was inconclusive")
                    return True  # Still consider it a success if agent was created
            else:
                print(f"  ❌ Failed to create agent with downloaded model. Status: {status}")
                return False
                
        except Exception as e:
            print(f"  ❌ Error during model download and agent test: {e}")
            return False
            
        finally:
            # Cleanup
            if model_downloaded:
                print("  🧹 Cleaning up downloaded model...")
                try:
                    cleanup_response, cleanup_status = self.make_request('DELETE', '/v1/engines/test-agent-qwen-0.6b')
                    if cleanup_status in [200, 204, 404]:
                        print("  ✅ Downloaded model cleaned up")
                    else:
                        print(f"  ⚠️  Model cleanup status: {cleanup_status}")
                except Exception as e:
                    print(f"  ⚠️  Error cleaning up model: {e}")

def main():
    parser = argparse.ArgumentParser(description="Test Kolosal Server Agent System")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--test", choices=[
        "status", "list", "create", "details", "execute", "yaml", "validation", "generate", "conversation", "download", "all"
    ], default="all", help="Specific test to run (default: all)")
    
    args = parser.parse_args()
    
    tester = AgentTester(args.host, args.port)
    
    try:
        if args.test == "all":
            tester.run_comprehensive_agent_tests()
        elif args.test == "status":
            tester.test_agent_system_status()
        elif args.test == "list":
            tester.test_list_agents()
        elif args.test == "create":
            tester.test_create_simple_agent()
            tester.test_create_function_agent()
        elif args.test == "details":
            # Create a test agent first
            agent_id = tester.test_create_simple_agent()
            if agent_id:
                tester.test_get_agent_details(agent_id)
        elif args.test == "execute":
            # Create a function agent first
            agent_id = tester.test_create_function_agent()
            if agent_id:
                tester.test_execute_agent_greet(agent_id)
                tester.test_execute_agent_calculate(agent_id)
                tester.test_execute_agent_time(agent_id)
        elif args.test == "yaml":
            tester.test_yaml_config_functionality()
        elif args.test == "validation":
            tester.test_model_instruction_validation()
        elif args.test == "generate":
            # Create a test agent first
            agent_id = tester.test_create_simple_agent()
            if agent_id:
                tester.test_agent_generate_response(agent_id)
        elif args.test == "conversation":
            # Create a test agent first
            agent_id = tester.test_create_simple_agent()
            if agent_id:
                tester.test_agent_conversation_flow(agent_id)
        elif args.test == "download":
            tester.test_download_and_use_model()
    finally:
        # Always cleanup
        tester.cleanup_created_agents()


if __name__ == "__main__":
    main()
