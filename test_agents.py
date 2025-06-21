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
            "config": {
                "model": "gpt-3.5-turbo",
                "temperature": 0.7,
                "max_tokens": 500
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
            "config": {
                "model": "gpt-3.5-turbo",
                "temperature": 0.5,
                "max_tokens": 1000
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
            
            # 2. Test listing agents (initially empty or with existing agents)
            initial_agents = self.test_list_agents()
            
            # 3. Test creating simple agent
            simple_agent_id = self.test_create_simple_agent()
            
            # 4. Test creating function agent
            function_agent_id = self.test_create_function_agent()
            
            # 5. Test invalid agent creation
            self.test_agent_with_invalid_data()
            
            # 6. Test getting agent details
            if simple_agent_id:
                self.test_get_agent_details(simple_agent_id)
            
            if function_agent_id:
                self.test_get_agent_details(function_agent_id)
            
            # 7. Test function executions
            if function_agent_id:
                self.test_execute_agent_greet(function_agent_id)
                self.test_execute_agent_calculate(function_agent_id)
                self.test_execute_agent_time(function_agent_id)
                self.test_execute_invalid_function(function_agent_id)
            
            # 8. Test listing agents again (should show created agents)
            final_agents = self.test_list_agents()
            
            # 9. Show summary
            print("\n📊 Test Summary")
            print("=" * 60)
            print(f"✅ Simple agent created: {'Yes' if simple_agent_id else 'No'}")
            print(f"✅ Function agent created: {'Yes' if function_agent_id else 'No'}")
            print(f"✅ Agent details retrieved: {'Yes' if simple_agent_id or function_agent_id else 'No'}")
            print(f"✅ Function executions tested: {'Yes' if function_agent_id else 'No'}")
            
        finally:
            # Cleanup created agents
            self.cleanup_created_agents()
        
        print("\n" + "=" * 70)
        print("🏁 Agent system tests completed!")
        print("=" * 70)
        
        return True


def main():
    parser = argparse.ArgumentParser(description="Test Kolosal Server Agent System")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--test", choices=[
        "status", "list", "create", "details", "execute", "all"
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
    finally:
        # Always cleanup
        tester.cleanup_created_agents()


if __name__ == "__main__":
    main()
