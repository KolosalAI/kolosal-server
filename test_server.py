#!/usr/bin/env python3
"""
Simple test script to verify Kolosal Server functionality
"""

import requests
import json
import time
import sys
from typing import Dict, Any

class KolosalServerTester:
    def __init__(self, base_url: str = "http://127.0.0.1:8080"):
        self.base_url = base_url
        self.session = requests.Session()
        
    def check_health(self) -> Dict[str, Any]:
        """Check server health status"""
        try:
            response = self.session.get(f"{self.base_url}/health", timeout=10)
            response.raise_for_status()
            return response.json()
        except Exception as e:
            return {"error": str(e), "status": "unhealthy"}
    
    def test_chat_completion(self, model: str = "qwen3-0.6b") -> Dict[str, Any]:
        """Test chat completion endpoint"""
        payload = {
            "model": model,
            "messages": [{"role": "user", "content": "Hello! Say 'test successful' if you can read this."}],
            "temperature": 0.7,
            "max_tokens": 50
        }
        
        try:
            response = self.session.post(
                f"{self.base_url}/v1/chat/completions",
                json=payload,
                timeout=30
            )
            if response.status_code == 200:
                return {"status": "success", "response": response.json()}
            else:
                return {"status": "error", "code": response.status_code, "text": response.text}
        except Exception as e:
            return {"status": "error", "exception": str(e)}
    
    def test_embedding(self, model: str = "text-embedding-3-small") -> Dict[str, Any]:
        """Test embedding endpoint"""
        payload = {
            "model": model,
            "input": "Test embedding text"
        }
        
        try:
            response = self.session.post(
                f"{self.base_url}/v1/embeddings",
                json=payload,
                timeout=30
            )
            if response.status_code == 200:
                return {"status": "success", "response": response.json()}
            else:
                return {"status": "error", "code": response.status_code, "text": response.text}
        except Exception as e:
            return {"status": "error", "exception": str(e)}
    
    def test_document_parsing(self) -> Dict[str, Any]:
        """Test document parsing"""
        # Create a simple test file
        test_content = """
        Test Document
        
        This is a test document to verify PDF parsing functionality.
        The server should be able to extract this text content.
        """
        
        try:
            # For now, just test if the endpoint exists
            response = self.session.get(f"{self.base_url}/parse-pdf", timeout=10)
            # 405 Method Not Allowed is expected for GET on POST endpoint
            if response.status_code in [405, 422]:
                return {"status": "endpoint_available"}
            else:
                return {"status": "endpoint_check", "code": response.status_code}
        except Exception as e:
            return {"status": "error", "exception": str(e)}
    
    def test_vector_search(self) -> Dict[str, Any]:
        """Test vector search functionality"""
        payload = {
            "query": "test search",
            "limit": 5
        }
        
        try:
            response = self.session.post(
                f"{self.base_url}/vector-search",
                json=payload,
                timeout=30
            )
            if response.status_code == 200:
                return {"status": "success", "response": response.json()}
            else:
                return {"status": "error", "code": response.status_code, "text": response.text}
        except Exception as e:
            return {"status": "error", "exception": str(e)}
    
    def run_all_tests(self) -> Dict[str, Any]:
        """Run all tests and return results"""
        print("🚀 Starting Kolosal Server Tests")
        print("=" * 50)
        
        # Test 1: Health Check
        print("\n📊 Testing Health Status...")
        health = self.check_health()
        print(f"Status: {health.get('status', 'unknown')}")
        if 'engines' in health:
            engines = health['engines']
            print(f"Engines available: {len(engines) if isinstance(engines, list) else 0}")
        
        # Test 2: Chat Completion
        print("\n💬 Testing Chat Completion...")
        chat_result = self.test_chat_completion()
        print(f"Status: {chat_result['status']}")
        if chat_result['status'] == 'error':
            print(f"Error: {chat_result.get('code', 'N/A')} - {chat_result.get('text', chat_result.get('exception', 'Unknown'))}")
        
        # Test 3: Embedding
        print("\n🔢 Testing Embedding...")
        embedding_result = self.test_embedding()
        print(f"Status: {embedding_result['status']}")
        if embedding_result['status'] == 'error':
            print(f"Error: {embedding_result.get('code', 'N/A')} - {embedding_result.get('text', embedding_result.get('exception', 'Unknown'))}")
        
        # Test 4: Document Parsing
        print("\n📄 Testing Document Parsing...")
        doc_result = self.test_document_parsing()
        print(f"Status: {doc_result['status']}")
        
        # Test 5: Vector Search
        print("\n🔍 Testing Vector Search...")
        search_result = self.test_vector_search()
        print(f"Status: {search_result['status']}")
        if search_result['status'] == 'error':
            print(f"Error: {search_result.get('code', 'N/A')} - {search_result.get('text', search_result.get('exception', 'Unknown'))}")
        
        # Summary
        print("\n" + "=" * 50)
        print("📋 Test Summary:")
        
        results = {
            "health": health,
            "chat_completion": chat_result,
            "embedding": embedding_result,
            "document_parsing": doc_result,
            "vector_search": search_result
        }
        
        # Count successes
        success_count = 0
        total_tests = 5
        
        if health.get('status') != 'unhealthy':
            success_count += 1
        if chat_result['status'] == 'success':
            success_count += 1
        if embedding_result['status'] == 'success':
            success_count += 1
        if doc_result['status'] in ['endpoint_available', 'success']:
            success_count += 1
        if search_result['status'] == 'success':
            success_count += 1
        
        print(f"✅ Passed: {success_count}/{total_tests}")
        print(f"❌ Failed: {total_tests - success_count}/{total_tests}")
        
        if success_count == total_tests:
            print("🎉 All tests passed!")
        elif success_count > 0:
            print("⚠️ Some tests passed, check errors above")
        else:
            print("🚨 All tests failed, server may not be running properly")
        
        return results

def main():
    """Main function"""
    if len(sys.argv) > 1:
        base_url = sys.argv[1]
    else:
        base_url = "http://127.0.0.1:8080"
    
    tester = KolosalServerTester(base_url)
    
    # Wait for server to be ready
    print("⏳ Waiting for server to be ready...")
    for i in range(10):
        try:
            health = tester.check_health()
            if health.get('status') != 'unhealthy':
                break
        except:
            pass
        time.sleep(1)
        print(f"   Attempt {i+1}/10...")
    
    results = tester.run_all_tests()
    
    # Return appropriate exit code
    if results['chat_completion']['status'] == 'success' or \
       results['embedding']['status'] == 'success':
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
