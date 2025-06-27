#!/usr/bin/env python3
"""
Simple Workflow Client for Kolosal Server

A streamlined, user-friendly client that automatically handles all the complexity:
- Auto-detects and sets up model engines
- Auto-discovers and maps agent names to UUIDs  
- Auto-registers workflows with conflict resolution
- Simple API for creating and executing workflows
- Built-in common workflow templates

Usage:
    from simple_workflow_client import SimpleWorkflowClient
    
    # Create client - it handles all setup automatically
    client = SimpleWorkflowClient()
    
    # Create and run a workflow in one line
    result = client.run_content_creation_workflow(
        topic="AI in Healthcare",
        audience="healthcare professionals"
    )
    
    # Or create custom workflows easily
    workflow = client.create_workflow("my_workflow", "My Custom Workflow")
    workflow.add_step("research", "research_assistant", "Research about AI trends")
    workflow.add_step("write", "content_creator", "Write an article based on research")
    result = client.execute_workflow(workflow)
"""

import sys
import os
import json
import time
import logging
import requests
from typing import Dict, Any, Optional, List, Union
from dataclasses import dataclass, field

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('simple_workflow_client.log', encoding='utf-8'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

@dataclass
class WorkflowStep:
    """Represents a single workflow step"""
    step_id: str
    agent_name: str  # Human-readable agent name
    prompt: str
    function_name: str = "inference"
    timeout_seconds: int = 60
    max_retries: int = 2
    temperature: float = 0.7
    max_tokens: int = 1000
    parameters: Dict[str, Any] = field(default_factory=dict)
    
    def to_dict(self, agent_id: str) -> Dict[str, Any]:
        """Convert to API format with actual agent UUID"""
        base_params = {
            "prompt": self.prompt,
            "model": "default",
            "max_tokens": self.max_tokens,
            "temperature": self.temperature
        }
        base_params.update(self.parameters)
        
        return {
            "step_id": self.step_id,
            "step_name": self.step_id.replace('_', ' ').title(),
            "description": f"Execute {self.function_name} using {self.agent_name}",
            "agent_id": agent_id,
            "function_name": self.function_name,
            "timeout_seconds": self.timeout_seconds,
            "max_retries": self.max_retries,
            "continue_on_failure": False,
            "parameters": base_params
        }

@dataclass 
class SimpleWorkflow:
    """Represents a workflow that can be easily built and executed"""
    workflow_id: str
    workflow_name: str
    description: str = ""
    steps: List[WorkflowStep] = field(default_factory=list)
    global_context: Dict[str, Any] = field(default_factory=dict)
    stop_on_failure: bool = True
    max_execution_time_seconds: int = 300
    
    def add_step(self, step_id: str, agent_name: str, prompt: str, **kwargs) -> 'SimpleWorkflow':
        """Add a step to the workflow (fluent interface)"""
        step = WorkflowStep(step_id=step_id, agent_name=agent_name, prompt=prompt, **kwargs)
        self.steps.append(step)
        return self
    
    def add_research_step(self, topic: str, agent_name: str = "research_assistant") -> 'SimpleWorkflow':
        """Add a research step"""
        return self.add_step(
            "research", 
            agent_name,
            f"Research the latest information about: {topic}. Provide comprehensive and accurate information.",
            temperature=0.3,
            max_tokens=1200
        )
    
    def add_writing_step(self, content_type: str = "article", agent_name: str = "content_creator") -> 'SimpleWorkflow':
        """Add a content writing step"""
        return self.add_step(
            "write_content",
            agent_name, 
            f"Based on the research, write a professional {content_type}. Make it engaging and well-structured.",
            temperature=0.7,
            max_tokens=1500
        )
    
    def add_review_step(self, criteria: str = "accuracy, clarity, tone", agent_name: str = "qa_specialist") -> 'SimpleWorkflow':
        """Add a quality review step"""
        return self.add_step(
            "review",
            agent_name,
            f"Review the content for: {criteria}. Provide constructive feedback and suggestions.",
            function_name="text_processing",
            parameters={"operation": "quality_review", "criteria": criteria}
        )
    
    def add_code_generation_step(self, requirements: str, language: str = "python", agent_name: str = "code_assistant") -> 'SimpleWorkflow':
        """Add a code generation step"""
        return self.add_step(
            "generate_code",
            agent_name,
            f"Generate {language} code for: {requirements}. Include proper error handling and documentation.",
            temperature=0.2,
            max_tokens=1500
        )

class SimpleWorkflowClient:
    """
    A simple, automated workflow client that handles all complexity behind the scenes.
    Just create workflows and run them - everything else is automatic.
    """
    
    def __init__(self, host: str = "localhost", port: int = 8080, auto_setup: bool = True):
        """
        Initialize the client with automatic setup
        
        Args:
            host: Server hostname
            port: Server port  
            auto_setup: Whether to automatically setup engines and discover agents
        """
        self.host = host
        self.port = port
        self.base_url = f"http://{host}:{port}"
        self.session = requests.Session()
        
        # Internal state - automatically managed
        self._agents_cache: Dict[str, str] = {}  # name -> uuid mapping
        self._engine_ready = False
        self._server_ready = False
        
        if auto_setup:
            self._auto_setup()
    
    def _auto_setup(self) -> bool:
        """Automatically setup everything needed for workflows"""
        print("🚀 Setting up Kolosal Workflow Client...")
        
        # 1. Check server health
        if not self._check_server_health():
            print("❌ Server is not available. Please start the Kolosal Server first.")
            return False
        print("✅ Server is healthy")
        
        # 2. Setup model engine
        if not self._auto_setup_engine():
            print("⚠️  Engine setup failed - workflows may not work properly")
        else:
            print("✅ Model engine ready")
        
        # 3. Discover and cache agents
        if not self._discover_agents():
            print("❌ No agents available. Please configure agents first.")
            return False
        print(f"✅ Found {len(self._agents_cache)} agents")
        
        print("🎉 Client setup complete! Ready to run workflows.")
        return True
    
    def check_server_health(self) -> bool:
        """Public method to check if server is running and healthy"""
        return self._check_server_health()
    
    def _check_server_health(self) -> bool:
        """Check if server is running"""
        try:
            response = self.session.get(f"{self.base_url}/health", timeout=5)
            self._server_ready = response.status_code == 200
            return self._server_ready
        except Exception as e:
            logger.debug(f"Server health check failed: {e}")
            return False
    
    def _auto_setup_engine(self) -> bool:
        """Automatically setup the default model engine"""
        try:
            engine_id = "default"
            model_path = "downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf"
            
            # Check if engine already exists
            try:
                response = self.session.get(f"{self.base_url}/engines/{engine_id}")
                if response.status_code == 200:
                    self._engine_ready = True
                    return True
            except:
                pass
            
            # Check if model file exists
            if not os.path.exists(model_path):
                logger.warning(f"Model file not found: {model_path}")
                return False
            
            # Create engine
            engine_data = {
                "engine_id": engine_id,
                "model_path": model_path,
                "n_ctx": 4096,
                "n_gpu_layers": 0,
                "main_gpu_id": 0,
                "n_batch": 512,
                "n_threads": 4
            }
            
            response = self.session.post(f"{self.base_url}/engines", json=engine_data)
            if response.status_code in [200, 201]:
                self._engine_ready = True
                return True
            elif response.status_code == 400 and "engine_id_exists" in response.text:
                self._engine_ready = True
                return True
            
            return False
            
        except Exception as e:
            logger.debug(f"Engine setup failed: {e}")
            return False
    
    def _discover_agents(self) -> bool:
        """Automatically discover and cache agent mappings"""
        try:
            response = self.session.get(f"{self.base_url}/api/v1/agents")
            if response.status_code == 200:
                agents = response.json().get('data', [])
                self._agents_cache = {
                    agent.get('name'): agent.get('id') 
                    for agent in agents 
                    if agent.get('name') and agent.get('id')
                }
                return len(self._agents_cache) > 0
            return False
        except Exception as e:
            logger.debug(f"Agent discovery failed: {e}")
            return False
    
    def get_available_agents(self) -> List[str]:
        """Get list of available agent names"""
        return list(self._agents_cache.keys())
    
    def create_workflow(self, workflow_id: str, workflow_name: str, description: str = "") -> SimpleWorkflow:
        """Create a new workflow builder"""
        return SimpleWorkflow(
            workflow_id=workflow_id,
            workflow_name=workflow_name, 
            description=description or f"Custom workflow: {workflow_name}"
        )
    
    def execute_workflow(self, workflow: SimpleWorkflow, input_context: Optional[Dict[str, Any]] = None, streaming: bool = False) -> Optional[Dict[str, Any]]:
        """
        Execute a workflow with automatic registration and agent mapping
        
        Args:
            workflow: The workflow to execute
            input_context: Optional input context
            streaming: Whether to use streaming execution
            
        Returns:
            Execution result or None if failed
        """
        try:
            # Convert to API format with agent UUIDs
            api_workflow = self._convert_workflow_to_api(workflow)
            if not api_workflow:
                print("❌ Failed to convert workflow - missing agents")
                return None
            
            # Auto-register workflow (with cleanup if needed)
            if not self._register_workflow_with_cleanup(api_workflow):
                print(f"❌ Failed to register workflow: {workflow.workflow_id}")
                return None
            
            # Execute workflow
            if streaming:
                return self._execute_streaming(workflow.workflow_id, input_context)
            else:
                return self._execute_sync(workflow.workflow_id, input_context)
                
        except Exception as e:
            logger.error(f"Workflow execution failed: {e}")
            print(f"❌ Workflow execution failed: {e}")
            return None
    
    def _convert_workflow_to_api(self, workflow: SimpleWorkflow) -> Optional[Dict[str, Any]]:
        """Convert SimpleWorkflow to API format"""
        api_steps = []
        
        for step in workflow.steps:
            agent_id = self._agents_cache.get(step.agent_name)
            if not agent_id:
                print(f"❌ Agent '{step.agent_name}' not found. Available agents: {list(self._agents_cache.keys())}")
                return None
            api_steps.append(step.to_dict(agent_id))
        
        return {
            "workflow_id": workflow.workflow_id,
            "workflow_name": workflow.workflow_name,
            "description": workflow.description,
            "stop_on_failure": workflow.stop_on_failure,
            "max_execution_time_seconds": workflow.max_execution_time_seconds,
            "global_context": workflow.global_context,
            "steps": api_steps
        }
    
    def _register_workflow_with_cleanup(self, workflow: Dict[str, Any]) -> bool:
        """Register workflow with automatic cleanup of existing ones"""
        workflow_id = workflow['workflow_id']
        
        # Try to register first
        try:
            response = self.session.post(
                f"{self.base_url}/api/v1/sequential-workflows",
                json=workflow,
                headers={"Content-Type": "application/json"}
            )
            
            if response.status_code == 201:
                return True
            elif response.status_code == 409:
                # Workflow exists, delete and re-register
                delete_response = self.session.delete(f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}")
                if delete_response.status_code in [200, 204, 404]:
                    # Try to register again
                    response = self.session.post(
                        f"{self.base_url}/api/v1/sequential-workflows",
                        json=workflow,
                        headers={"Content-Type": "application/json"}
                    )
                    return response.status_code == 201
            
            return False
            
        except Exception as e:
            logger.error(f"Registration failed: {e}")
            return False
    
    def _execute_sync(self, workflow_id: str, input_context: Optional[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
        """Execute workflow synchronously"""
        try:
            payload = {"input_context": input_context} if input_context else {}
            
            response = self.session.post(
                f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}/execute",
                json=payload,
                headers={"Content-Type": "application/json"}
            )
            
            if response.status_code == 200:
                result = response.json()
                # Unwrap data if needed
                if isinstance(result, dict) and 'data' in result:
                    result = result['data']
                return result
            else:
                print(f"❌ Execution failed: {response.status_code} - {response.text}")
                return None
                
        except Exception as e:
            logger.error(f"Sync execution failed: {e}")
            return None
    
    def _execute_streaming(self, workflow_id: str, input_context: Optional[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
        """Execute workflow with streaming"""
        try:
            payload = {"input_context": input_context} if input_context else {}
            
            response = self.session.post(
                f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}/execute",
                json=payload,
                headers={
                    "Content-Type": "application/json",
                    "Accept": "text/event-stream"
                },
                stream=True
            )
            
            if response.status_code == 200:
                final_result = None
                print("🔄 Streaming workflow execution...")
                
                for chunk in self._parse_streaming_response(response):
                    if isinstance(chunk, str):
                        print(chunk, end="", flush=True)
                    elif isinstance(chunk, dict):
                        final_result = chunk
                
                print("\n✅ Streaming completed")
                return final_result
            else:
                print(f"❌ Streaming execution failed: {response.status_code}")
                return None
                
        except Exception as e:
            logger.error(f"Streaming execution failed: {e}")
            return None
    
    def _parse_streaming_response(self, response):
        """Parse streaming response"""
        buffer = ""
        for chunk in response.iter_content(chunk_size=1024, decode_unicode=True):
            if chunk:
                buffer += chunk
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    if line.startswith('data:'):
                        data = line[5:].strip()
                        try:
                            yield json.loads(data)
                        except:
                            yield data
                    elif line:
                        yield line

    # Pre-built workflow templates for common use cases
    
    def run_content_creation_workflow(self, topic: str, audience: str = "general audience", content_type: str = "article") -> Optional[Dict[str, Any]]:
        """Create and run a content creation workflow"""
        workflow = self.create_workflow(
            "content_creation", 
            "Content Creation Pipeline",
            f"Research, write, and review content about {topic}"
        )
        
        workflow.global_context = {
            "topic": topic,
            "audience": audience,
            "content_type": content_type
        }
        
        workflow.add_research_step(topic)
        workflow.add_writing_step(content_type)
        workflow.add_review_step()
        
        print(f"🚀 Running content creation workflow for: {topic}")
        return self.execute_workflow(workflow)
    
    def run_code_development_workflow(self, requirements: str, language: str = "python") -> Optional[Dict[str, Any]]:
        """Create and run a code development workflow"""
        workflow = self.create_workflow(
            "code_development",
            "Code Development Pipeline", 
            f"Generate, review, and document {language} code"
        )
        
        workflow.global_context = {
            "requirements": requirements,
            "language": language
        }
        
        workflow.add_code_generation_step(requirements, language)
        workflow.add_review_step("code quality, security, best practices", "qa_specialist")
        workflow.add_step(
            "document",
            "content_creator",
            f"Create comprehensive documentation for the {language} code including usage examples and API reference",
            temperature=0.3
        )
        
        print(f"🚀 Running code development workflow for: {requirements}")
        return self.execute_workflow(workflow)
    
    def run_data_analysis_workflow(self, data_description: str, analysis_type: str = "statistical summary") -> Optional[Dict[str, Any]]:
        """Create and run a data analysis workflow"""
        workflow = self.create_workflow(
            "data_analysis",
            "Data Analysis Pipeline",
            f"Analyze {data_description} and generate insights"
        )
        
        workflow.global_context = {
            "data_description": data_description,
            "analysis_type": analysis_type
        }
        
        workflow.add_step(
            "prepare_data",
            "data_analyst", 
            f"Prepare and validate the {data_description} for {analysis_type}",
            function_name="data_analysis",
            parameters={"operation": "data_preparation"}
        )
        
        workflow.add_step(
            "analyze", 
            "data_analyst",
            f"Perform {analysis_type} on the prepared data",
            function_name="data_analysis", 
            parameters={"operation": "statistical_analysis"}
        )
        
        workflow.add_step(
            "generate_insights",
            "research_assistant",
            f"Based on the {analysis_type} results, generate key insights and actionable recommendations",
            temperature=0.4
        )
        
        print(f"🚀 Running data analysis workflow for: {data_description}")
        return self.execute_workflow(workflow)
    
    def start_workflow_execution(self, workflow: SimpleWorkflow) -> Optional[str]:
        """Start asynchronous workflow execution, returns execution ID"""
        try:
            # For now, we'll implement this as synchronous but return a fake execution ID
            # In a real implementation, this would start async execution
            logger.warning("start_workflow_execution is implemented as synchronous for demo purposes")
            result = self.execute_workflow(workflow)
            if result:
                # Generate a fake execution ID for demo purposes
                execution_id = f"exec_{int(time.time() * 1000)}"
                # Store result for later retrieval
                if not hasattr(self, '_execution_results'):
                    self._execution_results = {}
                self._execution_results[execution_id] = result
                return execution_id
            return None
        except Exception as e:
            logger.error(f"Failed to start workflow execution: {e}")
            return None
    
    def get_workflow_status(self, execution_id: str) -> Optional[Dict[str, Any]]:
        """Get status of running workflow execution"""
        try:
            # For demo purposes, simulate workflow completion
            if hasattr(self, '_execution_results') and execution_id in self._execution_results:
                return {
                    "status": "completed",
                    "completed_steps": 4,
                    "total_steps": 4,
                    "execution_id": execution_id
                }
            return None
        except Exception as e:
            logger.error(f"Failed to get workflow status: {e}")
            return None
    
    def get_workflow_result(self, execution_id: str) -> Optional[Dict[str, Any]]:
        """Get final result of workflow execution"""
        try:
            if hasattr(self, '_execution_results') and execution_id in self._execution_results:
                result = self._execution_results[execution_id]
                # Clean up stored result
                del self._execution_results[execution_id]
                return result
            return None
        except Exception as e:
            logger.error(f"Failed to get workflow result: {e}")
            return None

# Convenience functions for quick usage
def quick_content_creation(topic: str, audience: str = "general audience") -> Optional[Dict[str, Any]]:
    """Quick content creation - one function call"""
    client = SimpleWorkflowClient()
    return client.run_content_creation_workflow(topic, audience)

def quick_code_generation(requirements: str, language: str = "python") -> Optional[Dict[str, Any]]:
    """Quick code generation - one function call"""
    client = SimpleWorkflowClient()
    return client.run_code_development_workflow(requirements, language)

def quick_data_analysis(data_description: str) -> Optional[Dict[str, Any]]:
    """Quick data analysis - one function call"""
    client = SimpleWorkflowClient()
    return client.run_data_analysis_workflow(data_description)

# Example usage demonstration
if __name__ == "__main__":
    # Example 1: Quick one-liner usage
    print("=== Quick Content Creation ===")
    result = quick_content_creation("The Future of Quantum Computing", "tech professionals")
    if result:
        print("✅ Content creation completed successfully!")
    
    # Example 2: Using the client for more control
    print("\n=== Custom Workflow ===")
    client = SimpleWorkflowClient()
    
    # Create a custom workflow
    workflow = client.create_workflow("my_research", "AI Research Pipeline")
    workflow.add_research_step("machine learning trends in 2024")
    workflow.add_writing_step("technical report")
    workflow.add_review_step("technical accuracy, clarity")
    
    # Execute it
    result = client.execute_workflow(workflow)
    if result:
        print("✅ Custom workflow completed successfully!")
    
    # Example 3: Check what agents are available
    print(f"\n=== Available Agents ===")
    agents = client.get_available_agents()
    for agent in agents:
        print(f"  - {agent}")
