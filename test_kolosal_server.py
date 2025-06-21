#!/usr/bin/env python3
"""
Kolosal Server API Test Script

This script tests all available endpoints of the Kolosal Server.
Make sure the server is running before executing this script.

Usage:
    python test_kolosal_server.py [--host HOST] [--port PORT]
"""

import requests
import json
import argparse
import sys
import time
from typing import Dict, Any, Optional


class KolosalServerTester:
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.base_url = f"http://{host}:{port}"
        self.session = requests.Session()
        self.session.headers.update({
            'Content-Type': 'application/json',
            'User-Agent': 'KolosalServerTester/1.0'
        })
        
    def make_request(self, method: str, endpoint: str, data: Optional[Dict[Any, Any]] = None, 
                    params: Optional[Dict[str, Any]] = None) -> tuple:
        """Make HTTP request and return response and status"""
        url = f"{self.base_url}{endpoint}"
        
        try:
            if method.upper() == 'GET':
                response = self.session.get(url, params=params)
            elif method.upper() == 'POST':
                response = self.session.post(url, json=data, params=params)
            elif method.upper() == 'DELETE':
                response = self.session.delete(url, params=params)
            else:
                raise ValueError(f"Unsupported HTTP method: {method}")
                
            return response, response.status_code
        except requests.exceptions.RequestException as e:
            print(f"❌ Request failed: {e}")
            return None, 0

    def test_health(self):
        """Test health endpoint"""
        print("\n🏥 Testing Health Endpoint")
        print("=" * 50)
        
        response, status = self.make_request('GET', '/health')
        if response:
            print(f"Status Code: {status}")
            print(f"Response: {response.text}")
            if status == 200:
                print("✅ Health check passed")
            else:
                print("⚠️  Health check returned non-200 status")
        else:
            print("❌ Health check failed")

    def test_models(self):
        """Test models endpoint"""
        print("\n🤖 Testing Models Endpoint")
        print("=" * 50)
        
        response, status = self.make_request('GET', '/models')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Models endpoint working")
                else:
                    print("⚠️  Models endpoint returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Models endpoint failed")

    def test_chat_completions(self):
        """Test OpenAI-compatible chat completions endpoint"""
        print("\n💬 Testing Chat Completions Endpoint")
        print("=" * 50)
        
        test_data = {
            "model": "gpt-3.5-turbo",
            "messages": [
                {"role": "system", "content": "You are a helpful assistant."},
                {"role": "user", "content": "Hello, can you help me test this API?"}
            ],
            "max_tokens": 100,
            "temperature": 0.7
        }
        
        response, status = self.make_request('POST', '/v1/chat/completions', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Chat completions endpoint working")
                else:
                    print("⚠️  Chat completions returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Chat completions endpoint failed")

    def test_completions(self):
        """Test OpenAI-compatible completions endpoint"""
        print("\n📝 Testing Text Completions Endpoint")
        print("=" * 50)
        
        test_data = {
            "model": "gpt-3.5-turbo",
            "prompt": "Once upon a time, in a land far away,",
            "max_tokens": 100,
            "temperature": 0.7
        }
        
        response, status = self.make_request('POST', '/v1/completions', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Text completions endpoint working")
                else:
                    print("⚠️  Text completions returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Text completions endpoint failed")

    def test_engines_list(self):
        """Test list engines endpoint"""
        print("\n🔧 Testing List Engines Endpoint")
        print("=" * 50)
        
        response, status = self.make_request('GET', '/engines')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ List engines endpoint working")
                    return json_response
                else:
                    print("⚠️  List engines returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ List engines endpoint failed")
        return None

    def test_engines_add(self):
        """Test add engine endpoint"""
        print("\n➕ Testing Add Engine Endpoint")
        print("=" * 50)
        
        test_data = {
            "id": "test-engine-1",
            "type": "llama",
            "model_path": "/path/to/model.gguf",
            "config": {
                "context_length": 2048,
                "temperature": 0.7
            }
        }
        
        response, status = self.make_request('POST', '/engines', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status in [200, 201]:
                    print("✅ Add engine endpoint working")
                    return "test-engine-1"
                else:
                    print("⚠️  Add engine returned non-success status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Add engine endpoint failed")
        return None

    def test_engine_status(self, engine_id: str):
        """Test engine status endpoint"""
        print(f"\n📊 Testing Engine Status Endpoint for {engine_id}")
        print("=" * 50)
        
        response, status = self.make_request('GET', f'/engines/{engine_id}/status')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Engine status endpoint working")
                else:
                    print("⚠️  Engine status returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Engine status endpoint failed")

    def test_engine_delete(self, engine_id: str):
        """Test delete engine endpoint"""
        print(f"\n🗑️  Testing Delete Engine Endpoint for {engine_id}")
        print("=" * 50)
        
        response, status = self.make_request('DELETE', f'/engines/{engine_id}')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status in [200, 204]:
                    print("✅ Delete engine endpoint working")
                else:
                    print("⚠️  Delete engine returned non-success status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Delete engine endpoint failed")

    def test_agents_list(self):
        """Test list agents endpoint"""
        print("\n🤖 Testing List Agents Endpoint")
        print("=" * 50)
        
        response, status = self.make_request('GET', '/v1/agents')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ List agents endpoint working")
                    return json_response
                else:
                    print("⚠️  List agents returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ List agents endpoint failed")
        return None

    def test_agents_create(self):
        """Test create agent endpoint"""
        print("\n🔨 Testing Create Agent Endpoint")
        print("=" * 50)
        
        test_data = {
            "id": "test-agent-1",
            "name": "Test Agent",
            "description": "A test agent for API testing",
            "type": "assistant",
            "config": {
                "model": "gpt-3.5-turbo",
                "temperature": 0.7,
                "max_tokens": 1000
            },
            "functions": [
                {
                    "name": "test_function",
                    "description": "A test function",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "message": {
                                "type": "string",
                                "description": "Test message"
                            }
                        }
                    }
                }
            ]
        }
        
        response, status = self.make_request('POST', '/v1/agents', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status in [200, 201]:
                    print("✅ Create agent endpoint working")
                    return "test-agent-1"
                else:
                    print("⚠️  Create agent returned non-success status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Create agent endpoint failed")
        return None

    def test_agent_details(self, agent_id: str):
        """Test get agent details endpoint"""
        print(f"\n🔍 Testing Agent Details Endpoint for {agent_id}")
        print("=" * 50)
        
        response, status = self.make_request('GET', f'/v1/agents/{agent_id}')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Agent details endpoint working")
                else:
                    print("⚠️  Agent details returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Agent details endpoint failed")

    def test_agent_execute(self, agent_id: str):
        """Test execute agent function endpoint"""
        print(f"\n⚡ Testing Agent Execute Endpoint for {agent_id}")
        print("=" * 50)
        
        test_data = {
            "function": "test_function",
            "parameters": {
                "message": "Hello from test script!"
            }
        }
        
        response, status = self.make_request('POST', f'/v1/agents/{agent_id}/execute', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Agent execute endpoint working")
                else:
                    print("⚠️  Agent execute returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Agent execute endpoint failed")

    def test_agent_system_status(self):
        """Test agent system status endpoint"""
        print("\n📈 Testing Agent System Status Endpoint")
        print("=" * 50)
        
        response, status = self.make_request('GET', '/v1/agents/system/status')
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status == 200:
                    print("✅ Agent system status endpoint working")
                else:
                    print("⚠️  Agent system status returned non-200 status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Agent system status endpoint failed")

    def test_orchestration_workflows(self):
        """Test create workflow endpoint"""
        print("\n🔄 Testing Create Workflow Endpoint")
        print("=" * 50)
        
        test_data = {
            "name": "test-workflow",
            "description": "A test workflow for API testing",
            "steps": [
                {
                    "id": "step1",
                    "type": "agent_execute",
                    "agent_id": "test-agent-1",
                    "function": "test_function",
                    "parameters": {
                        "message": "Step 1 message"
                    }
                },
                {
                    "id": "step2",
                    "type": "agent_execute",
                    "agent_id": "test-agent-1",
                    "function": "test_function",
                    "parameters": {
                        "message": "Step 2 message"
                    },
                    "depends_on": ["step1"]
                }
            ]
        }
        
        response, status = self.make_request('POST', '/v1/orchestration/workflows', test_data)
        if response:
            print(f"Status Code: {status}")
            try:
                json_response = response.json()
                print(f"Response: {json.dumps(json_response, indent=2)}")
                if status in [200, 201]:
                    print("✅ Create workflow endpoint working")
                else:
                    print("⚠️  Create workflow returned non-success status")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Create workflow endpoint failed")

    def test_server_connectivity(self):
        """Test basic server connectivity"""
        print("🔗 Testing Server Connectivity")
        print("=" * 50)
        
        try:
            response = self.session.get(f"{self.base_url}/health", timeout=5)
            print(f"✅ Server is reachable at {self.base_url}")
            return True
        except requests.exceptions.RequestException as e:
            print(f"❌ Cannot reach server at {self.base_url}: {e}")
            return False

    def run_all_tests(self):
        """Run all API tests"""
        print("🚀 Starting Kolosal Server API Tests")
        print("=" * 60)
        
        # Test connectivity first
        if not self.test_server_connectivity():
            print("\n❌ Server is not reachable. Please make sure the server is running.")
            return False
        
        # Basic endpoints
        self.test_health()
        self.test_models()
        
        # OpenAI-compatible endpoints
        self.test_chat_completions()
        self.test_completions()
        
        # Engine management
        engines = self.test_engines_list()
        engine_id = self.test_engines_add()
        if engine_id:
            self.test_engine_status(engine_id)
            self.test_engine_delete(engine_id)
        
        # Agent system
        agents = self.test_agents_list()
        agent_id = self.test_agents_create()
        if agent_id:
            self.test_agent_details(agent_id)
            self.test_agent_execute(agent_id)
        
        self.test_agent_system_status()
        
        # Orchestration
        self.test_orchestration_workflows()
        
        print("\n" + "=" * 60)
        print("🏁 All tests completed!")
        print("=" * 60)
        
        return True


def main():
    parser = argparse.ArgumentParser(description="Test Kolosal Server API endpoints")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--endpoint", help="Test specific endpoint only")
    
    args = parser.parse_args()
    
    tester = KolosalServerTester(args.host, args.port)
    
    if args.endpoint:
        # Test specific endpoint
        method_map = {
            "health": tester.test_health,
            "models": tester.test_models,
            "chat": tester.test_chat_completions,
            "completions": tester.test_completions,
            "engines": tester.test_engines_list,
            "agents": tester.test_agents_list,
            "agent-status": tester.test_agent_system_status,
            "workflows": tester.test_orchestration_workflows
        }
        
        if args.endpoint in method_map:
            method_map[args.endpoint]()
        else:
            print(f"Unknown endpoint: {args.endpoint}")
            print(f"Available endpoints: {', '.join(method_map.keys())}")
            sys.exit(1)
    else:
        # Run all tests
        tester.run_all_tests()


if __name__ == "__main__":
    main()
