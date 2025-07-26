#!/usr/bin/env python3
"""
Simple Python test script for Kolosal Server RAG Agent
Tests the new knowledge_agent and rag_specialist agents with document retrieval capabilities.
"""

import requests
import json
import time
import sys
from typing import Dict, List, Any, Optional

class KolosalAgentTester:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url
        self.session = requests.Session()
        # Set default headers
        self.session.headers.update({
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        })
    
    def _make_request(self, method: str, endpoint: str, data: Optional[Dict] = None) -> Dict:
        """Make HTTP request with error handling"""
        url = f"{self.base_url}{endpoint}"
        try:
            if method.upper() == 'GET':
                response = self.session.get(url)
            elif method.upper() == 'POST':
                response = self.session.post(url, json=data)
            elif method.upper() == 'DELETE':
                response = self.session.delete(url, json=data)
            else:
                raise ValueError(f"Unsupported HTTP method: {method}")
            
            # Print request details
            print(f"\n{'='*60}")
            print(f"Request: {method.upper()} {url}")
            if data:
                print(f"Payload: {json.dumps(data, indent=2)}")
            print(f"Status Code: {response.status_code}")
            
            response_data = response.json() if response.content else {}
            print(f"Response: {json.dumps(response_data, indent=2)}")
            
            response.raise_for_status()
            return response_data
            
        except requests.exceptions.RequestException as e:
            print(f"❌ Request failed: {e}")
            if hasattr(e, 'response') and e.response is not None:
                try:
                    error_data = e.response.json()
                    print(f"Error details: {json.dumps(error_data, indent=2)}")
                except:
                    print(f"Error text: {e.response.text}")
            return {"success": False, "error": str(e)}
    
    def check_server_status(self) -> bool:
        """Check if the server is running"""
        print("🔍 Checking server status...")
        try:
            response = self._make_request('GET', '/api/v1/agents/system/status')
            if response.get('success'):
                print("✅ Server is running!")
                return True
            else:
                print("❌ Server is not responding properly")
                return False
        except Exception as e:
            print(f"❌ Cannot connect to server: {e}")
            return False
    
    def list_agents(self) -> List[Dict]:
        """List all available agents"""
        print("\n📋 Listing all agents...")
        response = self._make_request('GET', '/api/v1/agents')
        if response.get('success'):
            agents_data = response.get('data', [])
            print(f"Debug: agents_data type = {type(agents_data)}")
            print(f"Debug: agents_data = {agents_data}")
            
            # Handle both list and dict responses
            if isinstance(agents_data, list):
                agents = agents_data
                print(f"✅ Found {len(agents)} agents (list format)")
                if agents and len(agents) > 0:
                    print(f"Debug: first agent type = {type(agents[0])}")
                    print(f"Debug: first agent = {agents[0]}")
                else:
                    print("Debug: No agents found in the response")
            elif isinstance(agents_data, dict):
                # If it's a dict, extract the agents list or convert keys to list
                if 'agents' in agents_data:
                    agents = agents_data['agents']
                    print(f"✅ Found {len(agents)} agents (dict.agents format)")
                else:
                    # Treat dict keys as agent names
                    agents = list(agents_data.keys())
                    print(f"✅ Found {len(agents)} agents (dict keys format)")
                    print(f"Debug: agent names = {agents}")
            else:
                print("❌ Unexpected agents data format")
                agents = []
            
            for agent in agents:
                # Handle both string and dict responses
                if isinstance(agent, str):
                    print(f"  - {agent}")
                elif isinstance(agent, dict):
                    print(f"  - {agent.get('name', 'unknown')} ({agent.get('type', 'unknown')}) - Running: {agent.get('running', 'unknown')}")
                else:
                    print(f"  - {agent} (type: {type(agent)})")
            return agents
        else:
            print("❌ Failed to list agents")
            return []
    
    def get_agent_details(self, agent_identifier: str) -> Optional[Dict]:
        """Get detailed information about a specific agent using either agent_id or agent_name"""
        print(f"\n🔍 Getting details for agent: {agent_identifier}")
        response = self._make_request('GET', f'/api/v1/agents/{agent_identifier}')
        if response.get('success'):
            print(f"✅ Agent {agent_identifier} details retrieved")
            return response.get('data')
        else:
            print(f"❌ Failed to get agent details for {agent_identifier}")
            return None
    
    def test_document_service(self, agent_identifier: str) -> bool:
        """Test the document service connection"""
        print(f"\n🔬 Testing document service with agent: {agent_identifier}")
        
        # First check if agent exists and has the test_document_service function
        agent_details = self.get_agent_details(agent_identifier)
        if not agent_details:
            print(f"❌ Agent {agent_identifier} not found")
            return False
        
        functions = agent_details.get('functions', [])
        if 'test_document_service' not in functions:
            print(f"❌ Agent {agent_identifier} doesn't have test_document_service function")
            print(f"Available functions: {functions}")
            return False
        
        # Execute test_document_service function
        payload = {
            "parameters": {
                "detailed": True
            }
        }
        
        response = self._make_request('POST', f'/api/v1/agents/{agent_identifier}/functions/test_document_service', payload)
        if response.get('success') and response.get('data', {}).get('success'):
            print("✅ Document service test passed")
            return True
        else:
            print("❌ Document service test failed")
            return False
    
    def add_sample_documents(self, agent_identifier: str) -> bool:
        """Add sample documents to the knowledge base"""
        print(f"\n📄 Adding sample documents using agent: {agent_identifier}")
        
        sample_docs = [
            "Artificial Intelligence (AI) is a branch of computer science that aims to create intelligent machines that can perform tasks that typically require human intelligence, such as visual perception, speech recognition, decision-making, and language translation.",
            "Machine Learning is a subset of artificial intelligence that provides systems the ability to automatically learn and improve from experience without being explicitly programmed. It focuses on the development of computer programs that can access data and use it to learn for themselves.",
            "Deep Learning is a subset of machine learning that uses neural networks with multiple layers (hence 'deep') to model and understand complex patterns in data. It has been particularly successful in areas like image recognition, natural language processing, and speech recognition.",
            "Natural Language Processing (NLP) is a field of artificial intelligence that focuses on the interaction between computers and humans using natural language. It involves teaching computers to understand, interpret, and generate human language in a valuable way.",
            "Computer Vision is a field of artificial intelligence that trains computers to interpret and understand the visual world. Using digital images from cameras and videos and deep learning models, machines can accurately identify and classify objects."
        ]
        
        payload = {
            "texts": sample_docs,
            "collection_name": "test_documents"
        }
        
        # Try adding documents one by one if the array method failed
        success_count = 0
        for i, doc in enumerate(sample_docs):
            payload = {
                "parameters": {
                    "text": doc,  # Use singular 'text' instead of 'texts'
                    "collection_name": "test_documents"
                }
            }
            
            response = self._make_request('POST', f'/api/v1/agents/{agent_identifier}/functions/add_document', payload)
            if response.get('success') and response.get('data', {}).get('success'):
                success_count += 1
                print(f"  ✅ Added document {i+1}")
            else:
                print(f"  ❌ Failed to add document {i+1}")
        
        if success_count > 0:
            print(f"✅ Added {success_count}/{len(sample_docs)} sample documents")
            return True
        else:
            print("❌ Failed to add sample documents")
            return False
    
    def test_document_retrieval(self, agent_identifier: str, query: str = "what is machine learning") -> bool:
        """Test document retrieval functionality"""
        print(f"\n🔍 Testing document retrieval with query: '{query}'")
        
        payload = {
            "parameters": {
                "query": query,
                "k": 3,
                "score_threshold": 0.1,
                "collection_name": "test_documents"
            }
        }
        
        response = self._make_request('POST', f'/api/v1/agents/{agent_identifier}/functions/retrieval', payload)
        if response.get('success'):
            result = response.get('data', {}).get('result', {})
            if result:  # Check if result is not None
                documents = result.get('documents', [])
                print(f"✅ Retrieved {len(documents)} documents")
                
                for i, doc in enumerate(documents):
                    print(f"  Document {i+1}:")
                    # Handle both string and dict formats for documents
                    if isinstance(doc, dict):
                        print(f"    Score: {doc.get('score', 'N/A')}")
                        print(f"    Text preview: {doc.get('text', '')[:100]}...")
                    elif isinstance(doc, str):
                        print(f"    Score: N/A (string format)")
                        print(f"    Text preview: {doc[:100]}...")
                    else:
                        print(f"    Score: N/A (unknown format)")
                        print(f"    Content: {str(doc)[:100]}...")
                
                return len(documents) > 0
            else:
                print("❌ No results returned from retrieval")
                return False
        else:
            print("❌ Document retrieval failed")
            return False
    
    def test_context_retrieval(self, agent_identifier: str, query: str = "explain deep learning") -> bool:
        """Test context retrieval functionality"""
        print(f"\n🎯 Testing context retrieval with query: '{query}'")
        
        payload = {
            "parameters": {
                "query": query,
                "k": 3,
                "context_format": "detailed",
                "collection_name": "test_documents"
            }
        }
        
        response = self._make_request('POST', f'/api/v1/agents/{agent_identifier}/functions/context_retrieval', payload)
        if response.get('success'):
            result = response.get('data', {}).get('result', {})
            if result:  # Check if result is not None
                context = result.get('context', '')
                print(f"✅ Context retrieval successful")
                print(f"Context preview: {context[:200]}...")
                return len(context) > 0
            else:
                print("❌ No context returned from retrieval")
                return False
        else:
            print("❌ Context retrieval failed")
            return False
    
    def test_rag_enhanced_query(self, agent_identifier: str, query: str = "What are the main types of artificial intelligence?") -> bool:
        """Test RAG-enhanced query with inference"""
        print(f"\n🧠 Testing RAG-enhanced query: '{query}'")
        
        # First retrieve context
        context_payload = {
            "parameters": {
                "query": query,
                "k": 5,
                "context_format": "detailed",
                "collection_name": "test_documents"
            }
        }
        
        print("  Step 1: Retrieving relevant context...")
        context_response = self._make_request('POST', f'/api/v1/agents/{agent_identifier}/functions/context_retrieval', context_payload)
        
        if not context_response.get('success'):
            print("❌ Failed to retrieve context")
            return False
        
        context_result = context_response.get('data', {}).get('result', {})
        if not context_result:
            print("❌ No context result returned")
            return False
            
        context = context_result.get('context', '')
        print(f"  ✅ Context retrieved ({len(context)} characters)")
        
        # Now use context with inference
        enhanced_prompt = f"""Based on the following context, please answer the question comprehensively:

Context:
{context}

Question: {query}

Please provide a detailed answer based on the retrieved context."""
        
        inference_payload = {
            "parameters": {
                "prompt": enhanced_prompt,
                "max_tokens": 500,
                "temperature": 0.3
            }
        }
        
        print("  Step 2: Generating RAG-enhanced response...")
        inference_response = self._make_request('POST', f'/api/v1/agents/{agent_identifier}/functions/inference', inference_payload)
        
        if inference_response.get('success'):
            result = inference_response.get('data', {}).get('result', {})
            generated_text = result.get('text', result.get('response', ''))
            print(f"  ✅ RAG-enhanced response generated")
            print(f"  Response preview: {generated_text[:300]}...")
            return len(generated_text) > 0
        else:
            print("  ❌ Failed to generate RAG-enhanced response")
            return False
    
    def test_agent_message(self, agent_identifier: str, message: str = "Hello, can you help me understand AI?") -> bool:
        """Test direct messaging to agent"""
        print(f"\n💬 Testing agent message: '{message}'")
        
        payload = {
            "message": message,
            "model": "qwen3-0.6b",
            "temperature": 0.7,
            "max_tokens": 1024
        }
        
        response = self._make_request('POST', f'/api/v1/agents/{agent_identifier}/message', payload)
        if response.get('success'):
            data = response.get('data', {})
            agent_response = data.get('response', '')
            execution_time = data.get('execution_time_ms', 0)
            print(f"✅ Agent responded in {execution_time}ms")
            print(f"Response preview: {agent_response[:200]}...")
            return len(agent_response) > 0
        else:
            print("❌ Agent message failed")
            return False
    
    def run_comprehensive_test(self):
        """Run a comprehensive test of the RAG agent functionality"""
        print("🚀 Starting Comprehensive RAG Agent Test")
        print("="*60)
        
        test_results = []
        
        # Test 1: Server Status
        test_results.append(("Server Status", self.check_server_status()))
        
        # Test 2: List Agents
        agents = self.list_agents()
        test_results.append(("List Agents", len(agents) > 0))
        
        # Find RAG-capable agents
        rag_agents = []
        for agent in agents:
            if isinstance(agent, str):
                agent_name = agent
                agent_id = agent
            elif isinstance(agent, dict):
                agent_name = agent.get('name', '')
                agent_id = agent.get('agent_id', agent_name)
            else:
                continue
            
            if 'knowledge' in agent_name.lower() or 'rag' in agent_name.lower():
                rag_agents.append({'name': agent_name, 'id': agent_id})
        
        if not rag_agents:
            print("❌ No RAG-capable agents found!")
            print("Available agents:", [agent if isinstance(agent, str) else agent.get('name', 'unknown') for agent in agents])
            # Try default names and find their IDs
            default_agents = ['knowledge_agent', 'rag_specialist']
            for default_name in default_agents:
                for agent in agents:
                    if isinstance(agent, dict) and agent.get('name') == default_name:
                        rag_agents.append({'name': default_name, 'id': agent.get('agent_id', default_name)})
                        break
            print(f"🔄 Trying default agent names: {[agent['name'] for agent in rag_agents]}")
        
        print(f"\n🎯 Testing RAG agents: {[agent['name'] for agent in rag_agents]}")
        
        for agent_info in rag_agents:
            agent_name = agent_info['name']
            agent_id = agent_info['id']
            print(f"\n{'='*40}")
            print(f"Testing Agent: {agent_name} (ID: {agent_id})")
            print(f"{'='*40}")
            
            # Test 3: Agent Details
            agent_details = self.get_agent_details(agent_id)  # Use agent_id instead of name
            test_results.append((f"{agent_name} Details", agent_details is not None))
            
            if not agent_details:
                print(f"⏭️  Skipping tests for {agent_name} - agent not found")
                continue
            
            # Check if agent is running
            if not agent_details.get('running', False):
                print(f"⚠️  Agent {agent_name} is not running, some tests may fail")
            
            # Test 4: Document Service
            test_results.append((f"{agent_name} Document Service", self.test_document_service(agent_id)))
            
            # Test 5: Add Sample Documents
            test_results.append((f"{agent_name} Add Documents", self.add_sample_documents(agent_id)))
            
            # Test 6: Document Retrieval
            test_results.append((f"{agent_name} Document Retrieval", self.test_document_retrieval(agent_id)))
            
            # Test 7: Context Retrieval
            test_results.append((f"{agent_name} Context Retrieval", self.test_context_retrieval(agent_id)))
            
            # Test 8: RAG-Enhanced Query
            test_results.append((f"{agent_name} RAG Query", self.test_rag_enhanced_query(agent_id)))
            
            # Test 9: Direct Messaging
            test_results.append((f"{agent_name} Direct Message", self.test_agent_message(agent_id)))
        
        # Test Summary
        print(f"\n{'='*60}")
        print("📊 TEST SUMMARY")
        print(f"{'='*60}")
        
        passed = 0
        total = len(test_results)
        
        for test_name, result in test_results:
            status = "✅ PASS" if result else "❌ FAIL"
            print(f"{test_name:<40} {status}")
            if result:
                passed += 1
        
        success_rate = (passed / total) * 100 if total > 0 else 0
        print(f"\n📈 Overall Success Rate: {passed}/{total} ({success_rate:.1f}%)")
        
        if success_rate >= 80:
            print("🎉 RAG Agent testing completed successfully!")
        elif success_rate >= 60:
            print("⚠️  RAG Agent testing completed with some issues")
        else:
            print("❌ RAG Agent testing failed - multiple issues detected")
        
        return success_rate >= 80

def main():
    """Main function to run the tests"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Test Kolosal Server RAG Agent")
    parser.add_argument("--url", default="http://localhost:8080", help="Server URL (default: http://localhost:8080)")
    parser.add_argument("--agent", help="Specific agent to test (default: test all RAG agents)")
    parser.add_argument("--query", default="What is artificial intelligence?", help="Test query for RAG functionality")
    
    args = parser.parse_args()
    
    print("🤖 Kolosal Server RAG Agent Tester")
    print(f"Server: {args.url}")
    print(f"Test Query: {args.query}")
    
    tester = KolosalAgentTester(args.url)
    
    if args.agent:
        # Test specific agent
        print(f"\n🎯 Testing specific agent: {args.agent}")
        
        # Run individual tests
        tester.check_server_status()
        agents = tester.list_agents()
        
        # Find the agent by name in the list
        agent_id = None
        for agent in agents:
            if isinstance(agent, dict) and agent.get('name') == args.agent:
                agent_id = agent.get('agent_id')
                break
        
        if not agent_id:
            # If not found by name, try using the provided name as ID
            agent_id = args.agent
        
        agent_details = tester.get_agent_details(agent_id)
        if agent_details:
            tester.test_document_service(agent_id)
            tester.add_sample_documents(agent_id)
            tester.test_document_retrieval(agent_id, args.query)
            tester.test_context_retrieval(agent_id, args.query)
            tester.test_rag_enhanced_query(agent_id, args.query)
            tester.test_agent_message(agent_id, args.query)
        else:
            print(f"❌ Agent {args.agent} not found!")
            sys.exit(1)
    else:
        # Run comprehensive test
        success = tester.run_comprehensive_test()
        sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
