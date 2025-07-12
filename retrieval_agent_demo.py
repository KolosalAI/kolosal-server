#!/usr/bin/env python3
"""
Demo script for testing retrieval-enhanced agents in Kolosal Server
This script demonstrates how agents can use document retrieval to provide context-aware responses.
"""

import requests
import json
import time
import sys
from typing import Dict, List, Any

# Server configuration
SERVER_URL = "http://localhost:8080"
HEADERS = {"Content-Type": "application/json"}

def check_server_health():
    """Check if the server is running and healthy."""
    try:
        response = requests.get(f"{SERVER_URL}/health", timeout=5)
        return response.status_code == 200
    except requests.RequestException:
        return False

def add_sample_documents():
    """Add sample documents to the knowledge base for testing retrieval."""
    print("📄 Adding sample documents to knowledge base...")
    
    sample_documents = [
        {
            "text": "Python is a high-level, interpreted programming language with dynamic semantics. Its high-level built-in data structures, combined with dynamic typing and dynamic binding, make it very attractive for Rapid Application Development, as well as for use as a scripting or glue language to connect existing components together.",
            "metadata": {
                "title": "Python Programming Language",
                "category": "programming",
                "source": "documentation"
            }
        },
        {
            "text": "Machine learning is a method of data analysis that automates analytical model building. It is a branch of artificial intelligence based on the idea that systems can learn from data, identify patterns and make decisions with minimal human intervention.",
            "metadata": {
                "title": "Machine Learning Introduction",
                "category": "ai",
                "source": "education"
            }
        },
        {
            "text": "Vector databases are specialized databases designed to store and query high-dimensional vectors efficiently. They are essential for applications like semantic search, recommendation systems, and retrieval-augmented generation (RAG) where similarity search is crucial.",
            "metadata": {
                "title": "Vector Databases",
                "category": "database",
                "source": "technical"
            }
        },
        {
            "text": "Retrieval-Augmented Generation (RAG) is a technique that combines information retrieval with text generation. It retrieves relevant documents from a knowledge base and uses them as context to generate more accurate and informed responses.",
            "metadata": {
                "title": "Retrieval-Augmented Generation",
                "category": "ai",
                "source": "research"
            }
        },
        {
            "text": "Qdrant is an open-source vector similarity search engine written in Rust. It provides a production-ready service with a convenient API to store, search, and manage points (vectors) with an additional payload.",
            "metadata": {
                "title": "Qdrant Vector Database",
                "category": "database",
                "source": "documentation"
            }
        }
    ]
    
    try:
        response = requests.post(
            f"{SERVER_URL}/add_documents",
            headers=HEADERS,
            json={"documents": sample_documents}
        )
        
        if response.status_code == 200:
            result = response.json()
            print(f"✅ Added {len(sample_documents)} documents successfully")
            print(f"   Added: {result.get('added_count', 0)}")
            print(f"   Failed: {result.get('failed_count', 0)}")
            return True
        else:
            print(f"❌ Failed to add documents: {response.status_code}")
            print(f"   Response: {response.text}")
            return False
            
    except requests.RequestException as e:
        print(f"❌ Error adding documents: {e}")
        return False

def list_agents() -> List[Dict[str, Any]]:
    """Get list of available agents."""
    try:
        response = requests.get(f"{SERVER_URL}/api/v1/agents", headers=HEADERS)
        if response.status_code == 200:
            data = response.json()
            return data.get("data", [])
        else:
            print(f"❌ Failed to list agents: {response.status_code}")
            return []
    except requests.RequestException as e:
        print(f"❌ Error listing agents: {e}")
        return []

def get_agent_uuid_by_name(agent_name: str) -> str:
    """Get agent UUID by name."""
    agents = list_agents()
    for agent in agents:
        if agent.get("name") == agent_name:
            return agent.get("uuid", agent.get("id", ""))
    return ""

def test_basic_retrieval():
    """Test basic document retrieval functionality."""
    print("\n🔍 Testing basic document retrieval...")
    
    queries = [
        "What is Python programming language?",
        "Tell me about machine learning",
        "What are vector databases?",
        "Explain RAG technique"
    ]
    
    for query in queries:
        print(f"\n🔎 Query: {query}")
        try:
            response = requests.post(
                f"{SERVER_URL}/retrieve",
                headers=HEADERS,
                json={
                    "query": query,
                    "k": 2,
                    "score_threshold": 0.1
                }
            )
            
            if response.status_code == 200:
                result = response.json()
                print(f"   Found {result.get('total_found', 0)} documents")
                
                for i, doc in enumerate(result.get('documents', [])[:2]):
                    print(f"   📋 Document {i+1} (Score: {doc.get('score', 0):.3f})")
                    print(f"      {doc.get('text', '')[:100]}...")
            else:
                print(f"   ❌ Retrieval failed: {response.status_code}")
                
        except requests.RequestException as e:
            print(f"   ❌ Error: {e}")

def test_agent_retrieval_function(agent_name: str):
    """Test agent's retrieval function directly."""
    print(f"\n🤖 Testing {agent_name} agent's retrieval function...")
    
    agent_uuid = get_agent_uuid_by_name(agent_name)
    if not agent_uuid:
        print(f"   ❌ Agent '{agent_name}' not found")
        return
    
    try:
        response = requests.post(
            f"{SERVER_URL}/api/v1/agents/{agent_uuid}/execute",
            headers=HEADERS,
            json={
                "function": "retrieval",
                "parameters": {
                    "query": "What is RAG and how does it work?",
                    "k": 3,
                    "score_threshold": 0.1
                }
            }
        )
        
        if response.status_code == 200:
            result = response.json()
            print(f"✅ Function executed successfully")
            print(f"   Result: {result.get('result', {}).get('result', 'No result')}")
            
            documents = result.get('result', {}).get('documents', [])
            print(f"   Retrieved {len(documents)} documents")
            
            return True
        else:
            print(f"❌ Function execution failed: {response.status_code}")
            print(f"   Response: {response.text}")
            return False
            
    except requests.RequestException as e:
        print(f"❌ Error: {e}")
        return False

def test_agent_context_retrieval_function(agent_name: str):
    """Test agent's context retrieval function."""
    print(f"\n🧠 Testing {agent_name} agent's context retrieval function...")
    
    agent_uuid = get_agent_uuid_by_name(agent_name)
    if not agent_uuid:
        print(f"   ❌ Agent '{agent_name}' not found")
        return
    
    try:
        response = requests.post(
            f"{SERVER_URL}/api/v1/agents/{agent_uuid}/execute",
            headers=HEADERS,
            json={
                "function": "context_retrieval",
                "parameters": {
                    "query": "machine learning and vector databases",
                    "k": 2,
                    "context_format": "detailed",
                    "score_threshold": 0.1
                }
            }
        )
        
        if response.status_code == 200:
            result = response.json()
            print(f"✅ Context retrieval executed successfully")
            
            context = result.get('result', {}).get('context', '')
            if context:
                print(f"   Generated context ({len(context)} characters):")
                print(f"   {context[:300]}..." if len(context) > 300 else f"   {context}")
            
            return True
        else:
            print(f"❌ Context retrieval failed: {response.status_code}")
            print(f"   Response: {response.text}")
            return False
            
    except requests.RequestException as e:
        print(f"❌ Error: {e}")
        return False

def test_knowledge_agent_rag():
    """Test the knowledge agent with RAG capabilities."""
    print(f"\n🎯 Testing knowledge agent with RAG...")
    
    knowledge_agent_uuid = get_agent_uuid_by_name("knowledge_agent")
    if not knowledge_agent_uuid:
        print("❌ knowledge_agent not found")
        return
    
    # Test questions that should benefit from retrieval
    test_questions = [
        "What is Python and what makes it good for rapid application development?",
        "Explain how machine learning works and what makes it powerful",
        "How do vector databases support semantic search applications?",
        "What is the difference between regular text generation and RAG?"
    ]
    
    for question in test_questions:
        print(f"\n❓ Question: {question}")
        
        try:
            # First get context using context_retrieval
            context_response = requests.post(
                f"{SERVER_URL}/api/v1/agents/{knowledge_agent_uuid}/execute",
                headers=HEADERS,
                json={
                    "function": "context_retrieval",
                    "parameters": {
                        "query": question,
                        "k": 2,
                        "context_format": "summary"
                    }
                }
            )
            
            if context_response.status_code == 200:
                context_result = context_response.json()
                context = context_result.get('result', {}).get('context', '')
                
                print(f"   📚 Retrieved context for question")
                
                # Now use inference with the context
                enhanced_prompt = f"Context: {context}\n\nQuestion: {question}\n\nPlease provide a comprehensive answer based on the context provided above."
                
                inference_response = requests.post(
                    f"{SERVER_URL}/api/v1/agents/{knowledge_agent_uuid}/execute",
                    headers=HEADERS,
                    json={
                        "function": "inference",
                        "parameters": {
                            "prompt": enhanced_prompt,
                            "max_tokens": 200,
                            "temperature": 0.3
                        }
                    }
                )
                
                if inference_response.status_code == 200:
                    inference_result = inference_response.json()
                    answer = inference_result.get('result', {}).get('text', '')
                    
                    print(f"   🤖 Answer: {answer}")
                else:
                    print(f"   ❌ Inference failed: {inference_response.status_code}")
            else:
                print(f"   ❌ Context retrieval failed: {context_response.status_code}")
                
        except requests.RequestException as e:
            print(f"   ❌ Error: {e}")
        
        time.sleep(1)  # Small delay between requests

def main():
    """Main demo function."""
    print("🚀 Kolosal Server Retrieval Demo")
    print("=" * 50)
    
    # Check server health
    print("🏥 Checking server health...")
    if not check_server_health():
        print("❌ Server is not running or not healthy!")
        print("   Please start the Kolosal server first.")
        sys.exit(1)
    print("✅ Server is running and healthy")
    
    # Add sample documents
    if not add_sample_documents():
        print("❌ Failed to add sample documents. Continuing with existing documents...")
    
    # Wait a moment for documents to be indexed
    print("\n⏳ Waiting for documents to be indexed...")
    time.sleep(2)
    
    # Test basic retrieval
    test_basic_retrieval()
    
    # List available agents
    print("\n🤖 Listing available agents...")
    agents = list_agents()
    
    retrieval_capable_agents = []
    for agent in agents:
        agent_name = agent.get('name', agent.get('id', 'unknown'))
        capabilities = agent.get('capabilities', [])
        
        # Check if agent has retrieval capabilities
        if any('retrieval' in cap.lower() for cap in capabilities):
            retrieval_capable_agents.append(agent_name)
            print(f"   📋 {agent_name} - Has retrieval capabilities")
        else:
            print(f"   📋 {agent_name} - No retrieval capabilities")
    
    # Test retrieval functions on capable agents
    for agent_name in retrieval_capable_agents:
        test_agent_retrieval_function(agent_name)
        test_agent_context_retrieval_function(agent_name)
    
    # Test knowledge agent specifically if available
    if 'knowledge_agent' in [agent.get('name', agent.get('id', '')) for agent in agents]:
        test_knowledge_agent_rag()
    else:
        print("\n⚠️  Knowledge agent not found. Make sure it's configured in agents.yaml")
    
    print("\n✅ Demo completed!")
    print("\n📝 Summary:")
    print("   - Added sample documents to knowledge base")
    print("   - Tested basic document retrieval")
    print("   - Tested agent retrieval functions")
    print("   - Demonstrated context-aware generation")

if __name__ == "__main__":
    main()
