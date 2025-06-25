#!/usr/bin/env python3
"""
Sequential Workflow Demo for Kolosal Server

This script demonstrates how to create and execute sequential workflows using the enhanced 
workflow system. It shows step-by-step agent execution with proper error handling, 
monitoring, and result aggregation.

Features:
- Create sequential workflows with multiple steps
- Execute workflows with different agent types
- Monitor workflow progress and results
- Handle errors and retries
- Demonstrate real-world workflow patterns

Usage:
    python sequential_workflow_demo.py [--host HOST] [--port PORT] [--interactive]
"""

import sys
import codecs
import argparse
import os
import json
import time
import logging
import requests
from typing import Dict, Any, Optional, List, Generator
from datetime import datetime

# Set console encoding for Windows compatibility BEFORE configuring logging
if sys.platform == "win32":
    sys.stdout = codecs.getwriter("utf-8")(sys.stdout.detach())
    sys.stderr = codecs.getwriter("utf-8")(sys.stderr.detach())

# Configure logging with UTF-8 encoding for Windows compatibility
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('sequential_workflow_demo.log', encoding='utf-8'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

class SequentialWorkflowDemo:
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.host = host
        self.port = port
        self.base_url = f"http://{host}:{port}"
        self.session = requests.Session()
        self.streaming_enabled = False  # Enable streaming by default
        
    def check_server_health(self) -> bool:
        """Check if the server is running and healthy"""
        try:
            response = self.session.get(f"{self.base_url}/health")
            return response.status_code == 200
        except Exception as e:
            logger.error(f"Error checking server health: {e}")
            return False
    
    def list_available_agents(self) -> List[Dict[str, Any]]:
        """Get list of available agents"""
        try:
            response = self.session.get(f"{self.base_url}/api/v1/agents")
            if response.status_code == 200:
                return response.json().get('data', [])
            else:
                logger.error(f"Failed to list agents: {response.status_code} - {response.text}")
                return []
        except Exception as e:
            logger.error(f"Error listing agents: {e}")
            return []
    
    def get_agent_id_by_name(self, agent_name: str) -> Optional[str]:
        """Get agent UUID by agent name"""
        agents = self.list_available_agents()
        for agent in agents:
            if agent.get('name') == agent_name:
                return agent.get('id')
        return None
    
    def get_agent_name_mapping(self) -> Dict[str, str]:
        """Get mapping from agent names to UUIDs"""
        agents = self.list_available_agents()
        return {agent.get('name'): agent.get('id') for agent in agents if agent.get('name') and agent.get('id')}
    
    def create_content_creation_workflow(self) -> Dict[str, Any]:
        """Create a content creation workflow"""
        logger.debug("Creating content creation workflow configuration...")
        workflow = {
            "workflow_id": "content_creation_workflow",
            "workflow_name": "Content Creation Pipeline",
            "description": "Research, write, and review content in sequential steps",
            "stop_on_failure": True,
            "max_execution_time_seconds": 300,
            "global_context": {
                "topic": "The Future of AI in Healthcare",
                "target_audience": "healthcare professionals",
                "content_length": "medium"
            },
            "steps": [
                {
                    "step_id": "research_step",
                    "step_name": "Research Information",
                    "description": "Gather information about the topic",
                    "agent_id": "research_assistant",
                    "function_name": "inference",
                    "timeout_seconds": 60,
                    "max_retries": 2,
                    "continue_on_failure": False,
                    "parameters": {
                        "prompt": "Research the latest developments in AI applications for healthcare. Focus on practical applications, benefits, and challenges. Provide a comprehensive overview suitable for healthcare professionals.",
                        "model": "default",
                        "max_tokens": 1024,
                        "temperature": 0.3
                    }
                },
                {
                    "step_id": "content_writing_step",
                    "step_name": "Write Content",
                    "description": "Create content based on research",
                    "agent_id": "content_creator",
                    "function_name": "inference",
                    "timeout_seconds": 90,
                    "max_retries": 2,
                    "continue_on_failure": False,
                    "parameters": {
                        "prompt": "Based on the research information, write a professional article about AI in healthcare. Make it engaging and informative for healthcare professionals. Include practical examples and future outlook.",
                        "model": "default",
                        "max_tokens": 1500,
                        "temperature": 0.7
                    }
                },
                {
                    "step_id": "quality_review_step",
                    "step_name": "Quality Review",
                    "description": "Review and validate the content",
                    "agent_id": "qa_specialist",
                    "function_name": "text_processing",
                    "timeout_seconds": 45,
                    "max_retries": 1,
                    "continue_on_failure": True,
                    "parameters": {
                        "operation": "quality_review",
                        "criteria": "accuracy, clarity, professional tone"
                    }
                }
            ]
        }
        logger.debug(f"Content creation workflow created with {len(workflow['steps'])} steps")
        return workflow
    
    def create_data_analysis_workflow(self) -> Dict[str, Any]:
        """Create a data analysis workflow"""
        logger.debug("Creating data analysis workflow configuration...")
        workflow = {
            "workflow_id": "data_analysis_workflow",
            "workflow_name": "Data Analysis Pipeline",
            "description": "Analyze data, generate insights, and create visualizations",
            "stop_on_failure": False,
            "max_execution_time_seconds": 600,
            "global_context": {
                "dataset_topic": "healthcare performance metrics",
                "analysis_type": "statistical_summary"
            },
            "steps": [
                {
                    "step_id": "data_preparation",
                    "step_name": "Data Preparation",
                    "description": "Prepare and validate data for analysis",
                    "agent_id": "data_analyst",
                    "function_name": "data_analysis", 
                    "timeout_seconds": 60,
                    "max_retries": 2,
                    "parameters": {
                        "operation": "data_validation",
                        "data_source": "synthetic_healthcare_metrics"
                    }
                },
                {
                    "step_id": "statistical_analysis",
                    "step_name": "Statistical Analysis",
                    "description": "Perform statistical analysis on the data",
                    "agent_id": "data_analyst",
                    "function_name": "data_analysis",
                    "timeout_seconds": 90,
                    "max_retries": 2,
                    "parameters": {
                        "operation": "statistical_summary",
                        "include_charts": "true"
                    }
                },
                {
                    "step_id": "insight_generation",
                    "step_name": "Generate Insights",
                    "description": "Generate insights and recommendations from analysis",
                    "agent_id": "research_assistant", 
                    "function_name": "inference",
                    "timeout_seconds": 60,
                    "max_retries": 1,
                    "parameters": {
                        "prompt": "Based on the statistical analysis results, generate key insights and actionable recommendations for healthcare performance improvement.",
                        "model": "default",
                        "max_tokens": 800,
                        "temperature": 0.4
                    }
                }
            ]
        }
        logger.debug(f"Data analysis workflow created with {len(workflow['steps'])} steps")
        return workflow
    
    def create_code_development_workflow(self) -> Dict[str, Any]:
        """Create a code development workflow"""
        logger.debug("Creating code development workflow configuration...")
        workflow = {
            "workflow_id": "code_development_workflow",
            "workflow_name": "Code Development Pipeline",
            "description": "Generate, review, and test code in sequential steps",
            "stop_on_failure": True,
            "max_execution_time_seconds": 240,
            "global_context": {
                "project_type": "web_api",
                "language": "python",
                "framework": "fastapi"
            },
            "steps": [
                {
                    "step_id": "code_generation",
                    "step_name": "Generate Code",
                    "description": "Generate code based on requirements",
                    "agent_id": "code_assistant",
                    "function_name": "inference",
                    "timeout_seconds": 90,
                    "max_retries": 2,
                    "parameters": {
                        "prompt": "Create a FastAPI endpoint for user authentication with JWT tokens. Include proper error handling, input validation, and documentation.",
                        "model": "default",
                        "max_tokens": 1200,
                        "temperature": 0.2
                    }
                },
                {
                    "step_id": "code_review",
                    "step_name": "Code Review",
                    "description": "Review the generated code for quality and best practices",
                    "agent_id": "qa_specialist",
                    "function_name": "text_processing",
                    "timeout_seconds": 60,
                    "max_retries": 1,
                    "parameters": {
                        "operation": "code_review",
                        "focus": "security, best_practices, documentation"
                    }
                },
                {
                    "step_id": "documentation_generation",
                    "step_name": "Generate Documentation",
                    "description": "Create documentation for the code",
                    "agent_id": "content_creator",
                    "function_name": "inference",
                    "timeout_seconds": 45,
                    "max_retries": 1,
                    "parameters": {
                        "prompt": "Create comprehensive API documentation for the authentication endpoint including usage examples, request/response formats, and error codes.",
                        "model": "default",
                        "max_tokens": 800,
                        "temperature": 0.3
                    }
                }
            ]
        }
        logger.debug(f"Code development workflow created with {len(workflow['steps'])} steps")
        return workflow
    
    def register_workflow(self, workflow: Dict[str, Any]) -> bool:
        """Register a workflow with the server"""
        try:
            logger.debug(f"Attempting to register workflow: {workflow['workflow_id']}")
            response = self.session.post(
                f"{self.base_url}/api/v1/sequential-workflows",
                json=workflow,
                headers={"Content-Type": "application/json"}
            )
            logger.debug(f"Registration response status: {response.status_code}")
            logger.debug(f"Registration response text: {response.text}")
            if response.status_code == 201:
                logger.info(f"Workflow '{workflow['workflow_id']}' registered successfully")
                return True
            elif response.status_code == 409:
                logger.warning(f"Workflow '{workflow['workflow_id']}' already exists")
                return False
            else:
                logger.error(f"Failed to register workflow: {response.status_code} - {response.text}")
                return False
        except Exception as e:
            logger.error(f"[X] Error registering workflow: {e}")
            return False
    
    def execute_workflow(self, workflow_id: str, input_context: Optional[Dict[str, Any]] = None) -> Optional[Dict[str, Any]]:
        """Execute a workflow synchronously"""
        try:
            payload = {}
            if input_context:
                payload['input_context'] = input_context
            logger.info(f"[START] Executing workflow: {workflow_id}")
            logger.debug(f"Execution payload: {json.dumps(payload, indent=2)}")
            start_time = time.time()
            response = self.session.post(
                f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}/execute",
                json=payload,
                headers={"Content-Type": "application/json"}
            )
            execution_time = (time.time() - start_time) * 1000  # Convert to milliseconds
            logger.debug(f"Execution response status: {response.status_code}")
            logger.debug(f"Execution response text: {response.text}")
            if response.status_code == 200:
                result = response.json()
                logger.debug(f"Raw workflow execution result: {json.dumps(result, indent=2)}")
                # Unwrap if result is under 'data' key
                if isinstance(result, dict) and 'data' in result and isinstance(result['data'], dict):
                    result = result['data']
                    logger.debug(f"Unwrapped result from 'data' key: {json.dumps(result, indent=2)}")
                # Add execution time if not present
                if 'total_execution_time_ms' not in result:
                    result['total_execution_time_ms'] = execution_time
                # Fallbacks for missing workflow_id/name
                if 'workflow_id' not in result:
                    result['workflow_id'] = workflow_id
                if 'workflow_name' not in result:
                    result['workflow_name'] = workflow_id.replace('_', ' ').title()
                # Fallback for step results
                if 'step_results' not in result:
                    # Try to extract from other possible keys
                    for key in ['steps', 'executed_steps', 'results']:
                        if key in result:
                            result['step_results'] = result[key]
                            logger.debug(f"Using '{key}' as step_results")
                            break
                logger.debug(f"Final processed result: {json.dumps(result, indent=2)}")
                return result
            else:
                logger.error(f"Failed to execute workflow: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            logger.error(f"[X] Error executing workflow '{workflow_id}': {e}")
            return None
    
    def execute_workflow_async(self, workflow_id: str, input_context: Optional[Dict[str, Any]] = None) -> Optional[str]:
        """Execute a workflow asynchronously"""
        try:
            payload = {}
            if input_context:
                payload['input_context'] = input_context
            logger.info(f"[ASYNC] Starting async execution of workflow: {workflow_id}")
            response = self.session.post(
                f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}/execute-async",
                json=payload,
                headers={"Content-Type": "application/json"}
            )
            if response.status_code == 202:
                return response.json().get('execution_id')
            else:
                logger.error(f"Failed to start async workflow: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            logger.error(f"[X] Error executing async workflow '{workflow_id}': {e}")
            return None
    
    def execute_workflow_streaming(self, workflow_id: str, input_context: Optional[Dict[str, Any]] = None) -> Generator[str, None, Dict[str, Any]]:
        """Execute a workflow with streaming output for inference steps"""
        try:
            payload = {}
            if input_context:
                payload['input_context'] = input_context
            logger.info(f"[START] Executing workflow with streaming: {workflow_id}")
            logger.debug(f"Execution payload: {json.dumps(payload, indent=2)}")
            start_time = time.time()
            response = self.session.post(
                f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}/execute",
                json=payload,
                headers={
                    "Content-Type": "application/json",
                    "Accept": "text/event-stream"
                },
                stream=True
            )
            logger.debug(f"Response status: {response.status_code}")
            logger.debug(f"Response headers: {dict(response.headers)}")
            if response.status_code == 200:
                content_type = response.headers.get('Content-Type', '')
                chunk_count = 0
                final_result = None
                logger.debug(f"Content-Type: {content_type}")
                if 'application/json' in content_type and not 'stream' in content_type:
                    final_result = response.json()
                else:
                    for chunk in self._parse_workflow_streaming_response(response):
                        chunk_count += 1
                        yield chunk
                        if isinstance(chunk, dict):
                            final_result = chunk
                logger.debug(f"Processed {chunk_count} chunks, final_result: {final_result is not None}")
                if final_result is None and response.status_code == 200:
                    logger.info("No explicit final result received, but HTTP 200 - assuming success")
                    final_result = {
                        "status": "success",
                        "message": "Workflow completed successfully",
                        "execution_time": (time.time() - start_time) * 1000
                    }
                    yield f"\n✅ Workflow completed successfully (inferred from HTTP 200)!\n"
                execution_time = (time.time() - start_time) * 1000
                logger.info(f"[OK] Streaming workflow '{workflow_id}' completed in {execution_time:.2f}ms")
                return final_result
            else:
                error_msg = f"[X] Workflow '{workflow_id}' streaming failed: {response.status_code} - {response.text}"
                logger.error(error_msg)
                yield error_msg
                return None
        except Exception as e:
            error_msg = f"[X] Error in streaming workflow '{workflow_id}': {e}"
            logger.error(error_msg)
            yield error_msg
            return None
    
    def _parse_workflow_streaming_response(self, response: requests.Response) -> Generator[Any, None, None]:
        """Parse workflow streaming response"""
        try:
            buffer = ""
            for chunk in response.iter_content(chunk_size=1024, decode_unicode=True):
                if chunk:
                    buffer += chunk
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.strip()
                        if not line:
                            continue
                        if line.startswith('data:'):
                            data = line[5:].strip()
                            try:
                                obj = json.loads(data)
                                yield obj
                            except Exception:
                                yield data
                        else:
                            yield line
        except Exception as e:
            logger.error(f"Streaming parse error: {e}")
            yield f"❌ Streaming parse error: {e}"
    
    def register_workflow_with_cleanup(self, workflow: Dict[str, Any]) -> bool:
        """Register a workflow, deleting existing one if necessary"""
        workflow_id = workflow['workflow_id']
        workflow_name = workflow.get('workflow_name', workflow_id)
        
        logger.info(f"Registering workflow with cleanup: {workflow_name} (ID: {workflow_id})")
        
        # Update workflow with actual agent UUIDs
        workflow = self.update_workflow_with_agent_ids(workflow)
        
        # Try to register first
        if self.register_workflow(workflow):
            # Verify the registration was successful
            if self.verify_workflow_registration(workflow_id):
                logger.info(f"Workflow '{workflow_id}' registered and verified successfully")
                return True
            else:
                logger.warning(f"Workflow '{workflow_id}' registration returned success but verification failed")
        
        # If that failed, try to delete and re-register
        logger.info(f"First registration attempt failed, attempting cleanup and re-registration for: {workflow_id}")
        if self.delete_workflow(workflow_id):
            logger.info(f"Attempting to re-register workflow after cleanup: {workflow_id}")
            # Try to register again after deletion
            if self.register_workflow(workflow):
                # Verify the re-registration
                if self.verify_workflow_registration(workflow_id):
                    logger.info(f"Workflow '{workflow_id}' successfully re-registered and verified after cleanup")
                    return True
                else:
                    logger.error(f"Workflow '{workflow_id}' re-registration returned success but verification failed")
                    return False
            else:
                logger.error(f"Failed to re-register workflow '{workflow_id}' even after cleanup")
                return False
        else:
            logger.error(f"Failed to delete existing workflow '{workflow_id}' during cleanup")
            return False

    def verify_workflow_registration(self, workflow_id: str) -> bool:
        """Verify if a workflow is properly registered"""
        try:
            logger.debug(f"Verifying registration for workflow: {workflow_id}")
            response = self.session.get(f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}")
            
            if response.status_code == 200:
                logger.debug(f"Workflow '{workflow_id}' verification successful")
                return True
            elif response.status_code == 404:
                logger.warning(f"Workflow '{workflow_id}' not found during verification")
                return False
            else:
                logger.error(f"Unexpected status during verification: {response.status_code} - {response.text}")
                return False
                
        except Exception as e:
            logger.error(f"Error verifying workflow '{workflow_id}': {e}")
            return False
    
    def update_workflow_with_agent_ids(self, workflow: Dict[str, Any]) -> Dict[str, Any]:
        """Update workflow configuration with actual agent UUIDs"""
        logger.debug(f"Updating workflow '{workflow['workflow_id']}' with agent UUIDs")
        
        # Get agent name to UUID mapping
        agent_mapping = self.get_agent_name_mapping()
        
        # Update each step with correct agent ID
        for step in workflow.get('steps', []):
            agent_name = step.get('agent_id')
            if agent_name in agent_mapping:
                step['agent_id'] = agent_mapping[agent_name]
                logger.debug(f"Updated step '{step['step_id']}': {agent_name} -> {step['agent_id']}")
            else:
                logger.error(f"Agent '{agent_name}' not found in mapping for step '{step['step_id']}'")
                
        return workflow
    
    def setup_model_engine(self) -> bool:
        """Setup the model engine using the downloaded model"""
        try:
            model_path = "downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf"
            engine_id = "default"  # Use the engine name that agents expect
            
            # Check if model file exists
            if not os.path.exists(model_path):
                logger.error(f"Model file not found: {model_path}")
                print("❌ Model file not found. Please download the model first using the download script")
                return False
            
            logger.info(f"Found model file: {model_path}")
            print(f"✅ Found model file: {model_path}")
            
            # First, check if engine already exists by trying to get its details
            logger.debug(f"Checking if engine '{engine_id}' already exists...")
            print(f"🔍 Checking if engine '{engine_id}' already exists...")
            engine_check_response = self.session.get(f"{self.base_url}/engines/{engine_id}")
            
            if engine_check_response.status_code == 200:
                logger.info(f"Engine '{engine_id}' already exists and is available")
                print(f"✅ Engine '{engine_id}' already exists and is available")
                return True
            elif engine_check_response.status_code == 404:
                logger.info(f"Engine '{engine_id}' not found, will create new engine")
                print(f"🔄 Engine '{engine_id}' not found, will create new engine")
            else:
                logger.warning(f"Engine check returned status {engine_check_response.status_code}, will attempt to list all engines")
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
                        logger.info(f"Engine '{engine_id}' found in engines list")
                        print(f"✅ Engine '{engine_id}' found in engines list")
                        return True
            
            # Engine doesn't exist, create it
            logger.info(f"Creating engine '{engine_id}'...")
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
                logger.info(f"Engine '{engine_id}' created successfully")
                print(f"✅ Engine '{engine_id}' created successfully")
                return True
            elif create_response.status_code == 400:
                # Check if error is due to existing engine ID
                try:
                    error_response = create_response.json()
                    if (isinstance(error_response, dict) and 
                        'error' in error_response and 
                        error_response['error'].get('code') == 'engine_id_exists'):
                        logger.info(f"Engine '{engine_id}' already exists (detected during creation), using existing engine")
                        print(f"✅ Engine '{engine_id}' already exists (detected during creation), using existing engine")
                        return True
                except json.JSONDecodeError:
                    pass
                logger.error(f"Failed to create engine: {create_response.text}")
                print(f"❌ Failed to create engine: {create_response.text}")
                return False
            else:
                logger.error(f"Failed to create engine: {create_response.text}")
                print(f"❌ Failed to create engine: {create_response.text}")
                return False
                
        except Exception as e:
            logger.error(f"Error setting up model engine: {e}")
            print(f"❌ Error setting up model engine: {e}")
            return False
    
    def run_demo(self, interactive: bool = False):
        """Run the sequential workflow demonstration"""
        logger.info("Starting Sequential Workflow Demo...")
        print("[START] Sequential Workflow Demo Starting...")
        
        # Check server health
        logger.info("Performing server health check...")
        if not self.check_server_health():
            logger.error("Server health check failed - unable to continue")
            print("[X] Server is not available. Please start the Kolosal Server first.")
            return
        
        logger.info("Server health check passed")
        print("[OK] Server is healthy")
        
        # Setup model engine
        logger.info("Setting up model engine...")
        print("\n🔧 Setting up model engine...")
        if not self.setup_model_engine():
            logger.error("Failed to setup model engine - workflows may fail")
            print("[X] Failed to setup model engine. Workflows may fail without inference capability.")
            print("Continuing anyway to demonstrate workflow registration...")
        else:
            logger.info("Model engine setup completed successfully")
            print("[OK] Model engine setup completed")
        
        # List available agents
        logger.info("Fetching available agents...")
        agents = self.list_available_agents()
        if not agents:
            logger.error("No agents available - unable to continue")
            print("[X] No agents available. Please configure agents first.")
            return
        
        logger.info(f"Found {len(agents)} available agents")
        print(f"[OK] Found {len(agents)} available agents:")
        for agent in agents:
            agent_name = agent.get('name', 'Unknown')
            agent_type = agent.get('type', 'Unknown')
            print(f"  - {agent_name} ({agent_type})")
            logger.debug(f"Available agent: {agent_name} (type: {agent_type})")
        
        if interactive:
            logger.info("Starting interactive mode...")
            self.run_interactive_mode()
        else:
            logger.info("Starting automated demo mode...")
            self.run_automated_demo()
    
    def run_automated_demo(self):
        """Run automated demonstration of different workflow types"""
        logger.info("Starting automated workflow demonstrations...")
        print("\n[AUTO] Running Automated Workflow Demonstrations...")
        
        # Demo workflows
        workflows = [
            ("Content Creation", self.create_content_creation_workflow()),
            ("Data Analysis", self.create_data_analysis_workflow()),
            ("Code Development", self.create_code_development_workflow())
        ]
        
        logger.info(f"Will demonstrate {len(workflows)} workflow types")
        
        for i, (workflow_name, workflow_config) in enumerate(workflows, 1):
            logger.info(f"Starting demonstration {i}/{len(workflows)}: {workflow_name}")
            print(f"\n{'='*60}")
            print(f"DEMONSTRATING: {workflow_name}")
            print('='*60)
            
            # Register workflow (with cleanup if needed)
            logger.info(f"Registering workflow: {workflow_config['workflow_id']}")
            if not self.register_workflow_with_cleanup(workflow_config):
                logger.error(f"Failed to register workflow: {workflow_name}")
                print(f"[X] Failed to register {workflow_name} workflow")
                continue
            
            # Execute workflow
            workflow_id = workflow_config['workflow_id']
            logger.info(f"Executing workflow: {workflow_id}")
            
            if self.streaming_enabled:
                print(f"\n🚀 Executing {workflow_name} workflow with streaming...")
                result = None
                streaming_gen = self.execute_workflow_streaming(workflow_id)
                try:
                    for chunk in streaming_gen:
                        if isinstance(chunk, str):
                            print(chunk, end="", flush=True)
                        else:
                            result = chunk  # Final result
                except StopIteration as e:
                    # Capture the return value from the generator
                    if hasattr(e, 'value'):
                        result = e.value
                print()  # Add newline after streaming
                
                # Print streaming result summary
                print(f"\n📊 STREAMING WORKFLOW SUMMARY:")
                print("-" * 50)
                if result:
                    print(f"✅ Status: SUCCESS (Streaming)")
                    # Display basic info since full details were streamed
                    workflow_id = result.get('workflow_id', 'N/A')
                    success = result.get('success', False)
                    total_time = result.get('total_execution_time_ms', 0)
                    print(f"Workflow: {workflow_id}")
                    print(f"Success: {'YES' if success else 'NO'}")
                    print(f"Total Time: {total_time:.2f}ms")
                else:
                    print(f"❌ Status: FAILED (Streaming)")
                    print(f"No result data available from streaming execution")
            else:
                print(f"\n🚀 Executing {workflow_name} workflow...")
                result = self.execute_workflow(workflow_id)
            
            # Always print result information
            print(f"\n📊 WORKFLOW EXECUTION SUMMARY:")
            print("-" * 50)
            if result:
                logger.info(f"Workflow '{workflow_name}' completed successfully")
                print(f"✅ Status: SUCCESS")
                # Log detailed results
                self.log_workflow_results(workflow_id, result)
                self.display_workflow_result(result)
            else:
                logger.error(f"Workflow '{workflow_name}' execution failed")
                print(f"❌ Status: FAILED")
                print(f"[X] Failed to execute {workflow_name} workflow")
            
            # Small delay between workflows
            if i < len(workflows):  # Don't sleep after the last workflow
                logger.debug(f"Waiting 2 seconds before next workflow...")
                time.sleep(2)
        
        # Show executor metrics
        logger.info("Fetching executor metrics...")
        print(f"\n{'='*60}")
        print("EXECUTOR METRICS")
        print('='*60)
        
        try:
            response = self.session.get(f"{self.base_url}/api/v1/sequential-workflows/executor/metrics")
            if response.status_code == 200:
                metrics = response.json()['data']
                logger.info("Successfully retrieved executor metrics")
                for key, value in metrics.items():
                    print(f"{key.replace('_', ' ').title()}: {value}")
                    logger.debug(f"Metric - {key}: {value}")
            else:
                logger.error(f"Failed to get executor metrics: {response.status_code} - {response.text}")
                print("[X] Failed to get executor metrics")
        except Exception as e:
            logger.error(f"Error getting metrics: {e}")
            print(f"[X] Error getting metrics: {e}")
        
        logger.info("Automated demo completed")
        
        # Final demo summary
        print(f"\n{'='*60}")
        print("🎯 DEMO COMPLETION SUMMARY")
        print('='*60)
        print(f"✅ Completed {len(workflows)} workflow demonstrations")
        print("🎉 All workflow types have been demonstrated!")
        print(f"📝 Check the log file 'sequential_workflow_demo.log' for detailed execution logs")
        print('='*60)
    
    def run_interactive_mode(self):
        """Run interactive workflow demonstration"""
        logger.info("Starting interactive mode...")
        print("\n[INTERACTIVE] Interactive Mode - Choose workflows to run:")
        
        workflows = {
            "1": ("Content Creation", self.create_content_creation_workflow()),
            "2": ("Data Analysis", self.create_data_analysis_workflow()),
            "3": ("Code Development", self.create_code_development_workflow())
        }
        
        logger.debug(f"Available workflow options: {list(workflows.keys())}")
        
        while True:
            streaming_status = "✅ ON" if self.streaming_enabled else "❌ OFF"
            print(f"\nAvailable Workflows (Streaming: {streaming_status}):")
            for key, (name, _) in workflows.items():
                print(f"  {key}. {name}")
            print("  s. Toggle streaming mode")
            print("  q. Quit")
            
            try:
                choice = input("\nSelect a workflow to run (1-3), 's' to toggle streaming, or 'q' to quit: ").strip().lower()
                logger.debug(f"User selected option: {choice}")
                
                if choice == 'q':
                    logger.info("User chose to quit interactive mode")
                    break
                elif choice == 's':
                    self.streaming_enabled = not self.streaming_enabled
                    status = "✅ enabled" if self.streaming_enabled else "❌ disabled"
                    print(f"🔄 Streaming mode {status}")
                    logger.info(f"Streaming mode toggled to: {self.streaming_enabled}")
                    continue
                elif choice in workflows:
                    workflow_name, workflow_config = workflows[choice]
                    workflow_id = workflow_config['workflow_id']
                    logger.info(f"User selected workflow: {workflow_name} (ID: {workflow_id})")
                    print(f"\n[RUN] Running {workflow_name} workflow...")
                    
                    # Register and execute
                    logger.info(f"Registering workflow: {workflow_id}")
                    if self.register_workflow_with_cleanup(workflow_config):
                        logger.info(f"Successfully registered workflow: {workflow_id}")
                        
                        if self.streaming_enabled:
                            print(f"\n🚀 Executing {workflow_name} workflow with streaming...")
                            result = None
                            streaming_gen = self.execute_workflow_streaming(workflow_config['workflow_id'])
                            try:
                                for chunk in streaming_gen:
                                    if isinstance(chunk, str):
                                        print(chunk, end="", flush=True)
                                    else:
                                        result = chunk  # Final result
                            except StopIteration as e:
                                # Capture the return value from the generator
                                if hasattr(e, 'value'):
                                    result = e.value
                            print()  # Add newline after streaming
                            
                            # Print streaming result summary
                            print(f"\n📊 INTERACTIVE STREAMING RESULT:")
                            print("-" * 50)
                            if result:
                                print(f"✅ Status: SUCCESS (Streaming)")
                                # Display basic info since full details were streamed
                                workflow_id = result.get('workflow_id', workflow_name)
                                success = result.get('success', False)
                                total_time = result.get('total_execution_time_ms', 0)
                                print(f"Workflow: {workflow_id}")
                                print(f"Success: {'YES' if success else 'NO'}")
                                if total_time > 0:
                                    print(f"Total Time: {total_time:.2f}ms")
                            else:
                                print(f"❌ Status: FAILED (Streaming)")
                                print(f"No result data available from streaming execution")
                        else:
                            print(f"\n🚀 Executing {workflow_name} workflow...")
                            result = self.execute_workflow(workflow_config['workflow_id'])
                            
                            # Print non-streaming result summary
                            print(f"\n📊 INTERACTIVE RESULT:")
                            print("-" * 50)
                        
                        if result:
                            logger.info(f"Workflow '{workflow_name}' completed successfully")
                            if not self.streaming_enabled:  # Only show full details for non-streaming
                                print(f"✅ Status: SUCCESS")
                                # Log detailed results for non-streaming
                                self.log_workflow_results(workflow_config['workflow_id'], result)
                                self.display_workflow_result(result)
                            # For streaming, results are already logged during execution
                        else:
                            logger.error(f"Workflow '{workflow_name}' execution failed")
                            print(f"❌ Status: FAILED")
                            print(f"[X] Failed to execute {workflow_name} workflow")
                    else:
                        logger.error(f"Failed to register workflow: {workflow_name}")
                        print(f"[X] Failed to register {workflow_name} workflow")
                else:
                    logger.warning(f"Invalid choice received: {choice}")
                    print("[X] Invalid choice. Please select 1-3 or 'q'.")
                    
            except KeyboardInterrupt:
                logger.info("Interactive mode interrupted by user (Ctrl+C)")
                print("\n\n[INFO] Interactive mode interrupted. Goodbye!")
                break
            except EOFError:
                logger.info("Interactive mode ended (EOF)")
                print("\n\n[INFO] Goodbye!")
                break
            except Exception as e:
                logger.error(f"Unexpected error in interactive mode: {e}")
                print(f"[X] Unexpected error: {e}")
        
        logger.info("Interactive mode completed")

    def delete_workflow(self, workflow_id: str) -> bool:
        """Delete a workflow if it exists"""
        try:
            logger.info(f"Attempting to delete workflow: {workflow_id}")
            response = self.session.delete(f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}")
            
            if response.status_code in [200, 204]:
                logger.info(f"[OK] Workflow '{workflow_id}' deleted successfully")
                return True
            elif response.status_code == 404:
                logger.info(f"[OK] Workflow '{workflow_id}' didn't exist (404)")
                return True
            else:
                logger.warning(f"[WARN] Failed to delete workflow '{workflow_id}': {response.status_code} - {response.text}")
                return False
                
        except Exception as e:
            logger.warning(f"[WARN] Error deleting workflow '{workflow_id}': {e}")
            return False

    def display_workflow_result(self, result: Dict[str, Any]):
        workflow_id = result.get('workflow_id', 'N/A')
        workflow_name = result.get('workflow_name', 'N/A')
        success = result.get('success', False)
        logger.info(f"Displaying results for workflow: {workflow_id} (Success: {success})")
        print("\n" + "="*80)
        print(f"📋 DETAILED WORKFLOW EXECUTION RESULT")
        print("="*80)
        print(f"🔖 Workflow ID: {workflow_id}")
        print(f"📝 Workflow Name: {workflow_name}")
        success_icon = "✅" if success else "❌"
        print(f"{success_icon} Success: {'YES' if success else 'NO'}")
        print(f"⏱️  Total Execution Time: {result.get('total_execution_time_ms', 0):.2f}ms")
        print(f"📊 Total Steps: {result.get('total_steps', len(result.get('step_results', {})))}")
        print(f"✅ Successful Steps: {result.get('successful_steps', 0)}")
        print(f"❌ Failed Steps: {result.get('failed_steps', 0)}")
        if result.get('error_message'):
            print(f"🚨 Error: {result['error_message']}")
            logger.warning(f"Workflow '{workflow_id}' completed with error: {result['error_message']}")
        print(f"\n🔍 STEP EXECUTION DETAILS:")
        print("-" * 50)
        # Try to get step results from multiple possible keys
        step_results = result.get('step_results')
        if not step_results:
            # Try fallback keys
            for key in ['steps', 'executed_steps', 'results']:
                if key in result and isinstance(result[key], dict):
                    step_results = result[key]
                    break
        if not step_results:
            print("No steps were executed")
        else:
            # If step_results is a dict, iterate items; if list, enumerate
            if isinstance(step_results, dict):
                items = step_results.items()
            elif isinstance(step_results, list):
                items = enumerate(step_results, 1)
            else:
                items = []
            for i, (step_id, step_result) in enumerate(items, 1):
                if isinstance(step_result, dict):
                    execution_time = step_result.get('execution_time', 0)
                    step_success = step_result.get('success', False)
                    status_icon = "✅" if step_success else "❌"
                    status_text = "SUCCESS" if step_success else "FAILED"
                    print(f"\n{i}. 🔧 Step: {step_id}")
                    print(f"   {status_icon} Status: {status_text}")
                    print(f"   ⏱️  Execution Time: {execution_time:.2f}ms")
                    if step_result.get('error_message'):
                        print(f"   🚨 Error: {step_result['error_message']}")
                        logger.debug(f"Step '{step_id}' failed with error: {step_result['error_message']}")
                    result_data = step_result.get('result_data', {})
                    if result_data:
                        print(f"   📋 Result Keys: {list(result_data.keys())}")
                        for key, value in result_data.items():
                            if isinstance(value, str):
                                print(f"   🤖 {key.upper()} (LLM Generated Content):")
                                # Don't truncate - show full content
                                print(f"      {value}")
                            elif isinstance(value, (int, float)):
                                print(f"      {key}: {value}")
                            elif isinstance(value, (list, dict)):
                                print(f"      {key}: {type(value).__name__} with {len(value)} items")
                    else:
                        print("   📋 No result data available")
                else:
                    print(f"\n{i}. 🔧 Step: {step_id}")
                    print(f"   ⚠️  Step result is not a dict: {step_result}")
        print(f"\n🎯 EXECUTION SUMMARY:")
        print("-" * 30)
        total_time = result.get('total_execution_time_ms', 0)
        if total_time > 1000:
            print(f"⏱️  Total Time: {total_time/1000:.2f} seconds")
        else:
            print(f"⏱️  Total Time: {total_time:.2f} milliseconds")
        if success:
            print("🎉 Workflow completed successfully!")
        else:
            print("💥 Workflow failed to complete successfully")
        print("="*80)
    
    def log_workflow_content(self, workflow_id: str, content: str, step_id: str = None, content_type: str = "CONTENT"):
        """Log workflow content with proper formatting and context"""
        if step_id:
            logger.info(f"[{content_type}] {workflow_id}.{step_id}: {content.strip()}")
        else:
            logger.info(f"[{content_type}] {workflow_id}: {content.strip()}")
        
        # Also write to a separate LLM output file for easier analysis
        try:
            output_filename = f"llm_outputs_{workflow_id}.log"
            with open(output_filename, 'a', encoding='utf-8') as f:
                timestamp = time.strftime('%Y-%m-%d %H:%M:%S')
                if step_id:
                    f.write(f"\n[{timestamp}] {content_type} - {workflow_id}.{step_id}:\n")
                else:
                    f.write(f"\n[{timestamp}] {content_type} - {workflow_id}:\n")
                f.write("-" * 80 + "\n")
                f.write(content.strip() + "\n")
                f.write("-" * 80 + "\n\n")
        except Exception as e:
            logger.warning(f"Failed to write LLM output to file: {e}")
    
    def log_workflow_results(self, workflow_id: str, result: Dict[str, Any]):
        """Log comprehensive workflow results"""
        logger.info(f"[WORKFLOW_SUMMARY] {workflow_id}: Success={result.get('success', False)}, "
                   f"Steps={result.get('total_steps', 'N/A')}, "
                   f"Time={result.get('total_execution_time_ms', 0):.2f}ms")
        
        # Log each step's content
        step_results = result.get('step_results', {})
        if isinstance(step_results, dict):
            for step_id, step_result in step_results.items():
                if not isinstance(step_result, dict):
                    logger.warning(f"Step result for '{step_id}' is not a dict: {type(step_result)}")
                    continue
                    
                success = step_result.get('success', False)
                logger.info(f"[STEP_SUMMARY] {step_id}: Success={success}")
                
                # Log any generated content
                result_data = step_result.get('result_data')
                if result_data and isinstance(result_data, dict):
                    for key, value in result_data.items():
                        if isinstance(value, str) and value.strip():
                            # Log the actual generated content
                            self.log_workflow_content(workflow_id, value, step_id, f"GENERATED_{key.upper()}")
                elif result_data is None:
                    logger.debug(f"No result_data for step '{step_id}'")
                else:
                    logger.warning(f"result_data for step '{step_id}' is not a dict: {type(result_data)}")
                        
                # Display LLM output to console
                self.display_step_llm_output(step_id, step_result)
        else:
            logger.warning(f"step_results is not a dict: {type(step_results)}")
    
    def display_step_llm_output(self, step_id: str, step_result: Dict[str, Any]):
        """Display LLM output for a specific step"""
        if not step_result.get('success', False):
            print(f"\n❌ Step '{step_id}' failed - no LLM output to display")
            return
            
        result_data = step_result.get('result_data')
        if not result_data:
            print(f"\n⚠️  Step '{step_id}' succeeded but no result data available")
            return
        
        if not isinstance(result_data, dict):
            print(f"\n⚠️  Step '{step_id}' result_data is not a dict: {type(result_data)}")
            return
        
        # Use specialized inference display for better formatting
        self.display_inference_result(step_id, result_data)
        
        # Also log to file for detailed records
        logger.info(f"[LLM_OUTPUT] {step_id}: {json.dumps(result_data, indent=2)}")
        
        # Display any additional non-string data
        non_string_data = {k: v for k, v in result_data.items() if not isinstance(v, str)}
        if non_string_data:
            print(f"\n📋 Additional Data for Step '{step_id}':")
            print("-" * 40)
            for key, value in non_string_data.items():
                if isinstance(value, dict):
                    print(f"{key.upper()}: Dictionary with {len(value)} keys")
                    for sub_key, sub_value in list(value.items())[:3]:  # Show first 3 items
                        print(f"  {sub_key}: {sub_value}")
                    if len(value) > 3:
                        print(f"  ... and {len(value) - 3} more items")
                elif isinstance(value, list):
                    print(f"{key.upper()}: List with {len(value)} items")
                    for i, item in enumerate(value[:3]):  # Show first 3 items
                        print(f"  {i+1}. {item}")
                    if len(value) > 3:
                        print(f"  ... and {len(value) - 3} more items")
                else:
                    print(f"{key.upper()}: {value}")
            print("-" * 40)
    
    def display_inference_result(self, step_id: str, result_data: Dict[str, Any]):
        """Display inference/LLM generation results in a formatted way"""
        print(f"\n🧠 INFERENCE RESULT for Step '{step_id}':")
        print("=" * 70)
        
        # Common LLM response fields to look for
        llm_fields = ['response', 'text', 'content', 'generated_text', 'output', 'result', 'answer']
        
        # Display main LLM generated content
        main_content_found = False
        for field in llm_fields:
            if field in result_data and isinstance(result_data[field], str):
                content = result_data[field].strip()
                if content:
                    print(f"📝 Generated Content ({field.upper()}):")
                    print("-" * 50)
                    print(content)
                    print("-" * 50)
                    main_content_found = True
                    break
        
        # If no main content found, display all string fields
        if not main_content_found:
            for key, value in result_data.items():
                if isinstance(value, str) and value.strip():
                    print(f"📝 {key.upper()}:")
                    print("-" * 40)
                    # Don't truncate - show full content
                    print(value.strip())
                    print("-" * 40)
        
        # Display metadata/statistics if available
        metadata_fields = ['tokens_used', 'processing_time_ms', 'model_name', 'temperature', 'max_tokens']
        metadata_found = False
        for field in metadata_fields:
            if field in result_data:
                if not metadata_found:
                    print(f"\n📊 Generation Metadata:")
                    print("-" * 30)
                    metadata_found = True
                print(f"{field.replace('_', ' ').title()}: {result_data[field]}")
        
        if metadata_found:
            print("-" * 30)
        
        print("=" * 70)
    
    def display_workflow_llm_summary(self, workflow_id: str, result: Dict[str, Any]):
        """Display a comprehensive summary of all LLM outputs from the workflow"""
        print(f"\n{'='*80}")
        print(f"🤖 COMPLETE LLM OUTPUT SUMMARY FOR WORKFLOW: {workflow_id}")
        print('='*80)
        
        step_results = result.get('step_results', {})
        if not step_results:
            print("No step results available")
            return
            
        llm_output_count = 0
        
        for step_id, step_result in step_results.items():
            if not step_result.get('success', False):
                continue
                
            result_data = step_result.get('result_data', {})
            if not result_data:
                continue
                
            # Check if this step produced LLM output
            has_llm_output = any(isinstance(v, str) and v.strip() for v in result_data.values())
            if not has_llm_output:
                continue
                
            llm_output_count += 1
            print(f"\n🔍 STEP {llm_output_count}: {step_id}")
            print("-" * 60)
            
            # Display each text output from this step
            for key, value in result_data.items():
                if isinstance(value, str) and value.strip():
                    print(f"\n📝 {key.upper()}:")
                    content = value.strip()
                    # Don't truncate - show full content
                    print(content)
                    print()
        
        if llm_output_count == 0:
            print("No LLM outputs were generated in this workflow")
        else:
            print(f"\n🎯 SUMMARY: {llm_output_count} steps generated LLM content")
            
        print('='*80)

def main():
    parser = argparse.ArgumentParser(description="Sequential Workflow Demo for Kolosal Server")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--interactive", action="store_true", help="Run in interactive mode")
    parser.add_argument("--debug", action="store_true", help="Enable debug logging")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose content logging")
    args = parser.parse_args()
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
        logger.info("Debug logging enabled")
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
        logger.info("Verbose content logging enabled")
    logger.info(f"Starting Sequential Workflow Demo - Host: {args.host}, Port: {args.port}, Interactive: {args.interactive}")
    try:
        demo = SequentialWorkflowDemo(host=args.host, port=args.port)
        demo.run_demo(interactive=args.interactive)
        logger.info("Sequential Workflow Demo completed successfully")
    except KeyboardInterrupt:
        logger.info("Demo interrupted by user (Ctrl+C)")
        print("\n\n[INFO] Demo interrupted. Goodbye!")
    except Exception as e:
        logger.error(f"Unexpected error during demo execution: {e}", exc_info=True)
        print(f"\n[X] Unexpected error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
