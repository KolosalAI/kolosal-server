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

import requests
import json
import argparse
import sys
import time
import logging
from typing import Dict, Any, Optional, List
from datetime import datetime

# Set console encoding for Windows compatibility BEFORE configuring logging
if sys.platform == "win32":
    import codecs
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
        self.base_url = f"http://{host}:{port}"
        self.session = requests.Session()
        
    def check_server_health(self) -> bool:
        """Check if the server is running and healthy"""
        try:
            response = self.session.get(f"{self.base_url}/v1/health", timeout=5)
            return response.status_code == 200
        except Exception as e:
            logger.error(f"[X] Server health check failed: {e}")
            return False
    
    def list_available_agents(self) -> List[Dict[str, Any]]:
        """Get list of available agents"""
        try:
            logger.info("Fetching available agents from server...")
            response = self.session.get(f"{self.base_url}/api/v1/agents")
            if response.status_code == 200:
                data = response.json()
                agents = data.get('data', [])
                logger.info(f"Successfully retrieved {len(agents)} agents")
                return agents
            else:
                logger.error(f"[X] Failed to get agents: {response.status_code} - {response.text}")
                return []
        except Exception as e:
            logger.error(f"[X] Error getting agents: {e}")
            return []
    
    def get_agent_id_by_name(self, agent_name: str) -> Optional[str]:
        """Get agent UUID by agent name"""
        agents = self.list_available_agents()
        for agent in agents:
            if agent.get('name') == agent_name:
                return agent.get('id')
        logger.warning(f"Agent not found: {agent_name}")
        return None
    
    def get_agent_name_mapping(self) -> Dict[str, str]:
        """Get mapping from agent names to UUIDs"""
        agents = self.list_available_agents()
        mapping = {}
        for agent in agents:
            name = agent.get('name')
            agent_id = agent.get('id')
            if name and agent_id:
                mapping[name] = agent_id
        logger.debug(f"Created agent name mapping: {mapping}")
        return mapping
    
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
                logger.info(f"[OK] Workflow '{workflow['workflow_id']}' registered successfully")
                return True
            elif response.status_code == 409:
                # Workflow already exists - this is OK, we can still use it
                logger.info(f"[INFO] Workflow '{workflow['workflow_id']}' already exists - continuing")
                return True
            else:
                logger.error(f"[X] Failed to register workflow: {response.status_code} - {response.text}")
                try:
                    error_data = response.json()
                    logger.error(f"[X] Error details: {json.dumps(error_data, indent=2)}")
                except:
                    logger.error(f"[X] Raw response: {response.text}")
                return False
                
        except Exception as e:
            logger.error(f"[X] Error registering workflow: {e}")
            return False
    
    def execute_workflow(self, workflow_id: str, input_context: Optional[Dict[str, Any]] = None) -> Optional[Dict[str, Any]]:
        """Execute a workflow synchronously"""
        try:
            payload = {}
            if input_context:
                payload["input_context"] = input_context
            
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
                logger.info(f"[OK] Workflow '{workflow_id}' completed successfully in {execution_time:.2f}ms")
                return result['data']
            else:
                logger.error(f"[X] Workflow '{workflow_id}' execution failed: {response.status_code} - {response.text}")
                try:
                    error_data = response.json()
                    logger.error(f"[X] Error details: {json.dumps(error_data, indent=2)}")
                except:
                    logger.error(f"[X] Raw response: {response.text}")
                return None
                
        except Exception as e:
            logger.error(f"[X] Error executing workflow '{workflow_id}': {e}")
            return None
    
    def execute_workflow_async(self, workflow_id: str, input_context: Optional[Dict[str, Any]] = None) -> Optional[str]:
        """Execute a workflow asynchronously"""
        try:
            payload = {}
            if input_context:
                payload["input_context"] = input_context
            
            logger.info(f"[ASYNC] Starting async execution of workflow: {workflow_id}")
            
            response = self.session.post(
                f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}/execute-async",
                json=payload,
                headers={"Content-Type": "application/json"}
            )
            
            if response.status_code == 202:
                result = response.json()
                execution_id = result['data']['execution_id']
                logger.info(f"[START] Async workflow '{workflow_id}' started successfully (execution_id: {execution_id})")
                return execution_id
            else:
                logger.error(f"[X] Async workflow '{workflow_id}' execution failed: {response.status_code} - {response.text}")
                return None
                
        except Exception as e:
            logger.error(f"[X] Error executing async workflow '{workflow_id}': {e}")
            return None
    
    def get_workflow_status(self, workflow_id: str) -> Optional[Dict[str, Any]]:
        """Get workflow status"""
        try:
            logger.debug(f"Fetching status for workflow: {workflow_id}")
            response = self.session.get(f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}/status")
            
            if response.status_code == 200:
                status_data = response.json()['data']
                logger.debug(f"Successfully retrieved status for workflow '{workflow_id}'")
                return status_data
            else:
                logger.error(f"[X] Failed to get workflow status for '{workflow_id}': {response.status_code} - {response.text}")
                return None
                
        except Exception as e:
            logger.error(f"[X] Error getting workflow status for '{workflow_id}': {e}")
            return None
    
    def get_workflow_result(self, workflow_id: str) -> Optional[Dict[str, Any]]:
        """Get workflow execution result"""
        try:
            logger.debug(f"Fetching execution result for workflow: {workflow_id}")
            response = self.session.get(f"{self.base_url}/api/v1/sequential-workflows/{workflow_id}/result")
            
            if response.status_code == 200:
                result_data = response.json()['data']
                logger.debug(f"Successfully retrieved result for workflow '{workflow_id}'")
                return result_data
            else:
                logger.error(f"[X] Failed to get workflow result for '{workflow_id}': {response.status_code} - {response.text}")
                return None
                
        except Exception as e:
            logger.error(f"[X] Error getting workflow result for '{workflow_id}': {e}")
            return None
    
    def list_workflows(self) -> List[Dict[str, Any]]:
        """List all registered workflows"""
        try:
            logger.debug("Fetching list of all registered workflows...")
            response = self.session.get(f"{self.base_url}/api/v1/sequential-workflows")
            
            if response.status_code == 200:
                workflows_data = response.json()['data']
                logger.info(f"Successfully retrieved {len(workflows_data)} registered workflows")
                return workflows_data
            else:
                logger.error(f"[X] Failed to list workflows: {response.status_code} - {response.text}")
                return []
                
        except Exception as e:
            logger.error(f"[X] Error listing workflows: {e}")
            return []
    
    def display_workflow_result(self, result: Dict[str, Any]):
        """Display workflow execution result in a formatted way"""
        workflow_id = result.get('workflow_id', 'N/A')
        workflow_name = result.get('workflow_name', 'N/A')
        success = result.get('success', False)
        
        logger.info(f"Displaying results for workflow: {workflow_id} (Success: {success})")
        
        print("\n" + "="*80)
        print(f"WORKFLOW EXECUTION RESULT")
        print("="*80)
        
        print(f"Workflow ID: {workflow_id}")
        print(f"Workflow Name: {workflow_name}")
        print(f"Success: {'[OK] YES' if success else '[X] NO'}")
        print(f"Total Execution Time: {result.get('total_execution_time_ms', 0):.2f}ms")
        print(f"Total Steps: {result.get('total_steps', 0)}")
        print(f"Successful Steps: {result.get('successful_steps', 0)}")
        print(f"Failed Steps: {result.get('failed_steps', 0)}")
        
        if result.get('error_message'):
            print(f"Error: {result['error_message']}")
            logger.warning(f"Workflow '{workflow_id}' completed with error: {result['error_message']}")
        
        print("\nSTEP EXECUTION DETAILS:")
        print("-" * 40)
        
        executed_steps = result.get('executed_steps', [])
        logger.debug(f"Processing {len(executed_steps)} executed steps for display")
        
        for step_id in executed_steps:
            step_result = result.get('step_results', {}).get(step_id, {})
            execution_time = result.get('step_execution_times', {}).get(step_id, 0)
            
            status = "[OK] SUCCESS" if step_result.get('success', False) else "[X] FAILED"
            print(f"\nStep: {step_id}")
            print(f"  Status: {status}")
            print(f"  Execution Time: {execution_time:.2f}ms")
            
            if step_result.get('error_message'):
                print(f"  Error: {step_result['error_message']}")
                logger.debug(f"Step '{step_id}' failed with error: {step_result['error_message']}")
            
            result_data = step_result.get('result_data', {})
            if result_data:
                print(f"  Result Keys: {list(result_data.keys())}")
                # Display first few characters of text results
                for key, value in result_data.items():
                    if isinstance(value, str) and len(value) > 100:
                        print(f"    {key}: {value[:100]}...")
                    else:
                        print(f"    {key}: {value}")
        
        print("\n" + "="*80)
    
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
            result = self.execute_workflow(workflow_id)
            
            if result:
                logger.info(f"Workflow '{workflow_name}' completed successfully")
                self.display_workflow_result(result)
            else:
                logger.error(f"Workflow '{workflow_name}' execution failed")
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
            print("\nAvailable Workflows:")
            for key, (name, _) in workflows.items():
                print(f"  {key}. {name}")
            print("  q. Quit")
            
            try:
                choice = input("\nSelect a workflow to run (1-3) or 'q' to quit: ").strip().lower()
                logger.debug(f"User selected option: {choice}")
                
                if choice == 'q':
                    logger.info("User chose to quit interactive mode")
                    break
                elif choice in workflows:
                    workflow_name, workflow_config = workflows[choice]
                    workflow_id = workflow_config['workflow_id']
                    logger.info(f"User selected workflow: {workflow_name} (ID: {workflow_id})")
                    print(f"\n[RUN] Running {workflow_name} workflow...")
                    
                    # Register and execute
                    logger.info(f"Registering workflow: {workflow_id}")
                    if self.register_workflow_with_cleanup(workflow_config):
                        logger.info(f"Successfully registered workflow: {workflow_id}")
                        result = self.execute_workflow(workflow_config['workflow_id'])
                        if result:
                            logger.info(f"Workflow '{workflow_name}' completed successfully")
                            self.display_workflow_result(result)
                        else:
                            logger.error(f"Workflow '{workflow_name}' execution failed")
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
    
def main():
    parser = argparse.ArgumentParser(description="Sequential Workflow Demo for Kolosal Server")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--interactive", action="store_true", help="Run in interactive mode")
    parser.add_argument("--debug", action="store_true", help="Enable debug logging")
    
    args = parser.parse_args()
    
    # Set debug logging level if requested
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
        logger.info("Debug logging enabled")
    
    logger.info(f"Starting Sequential Workflow Demo - Host: {args.host}, Port: {args.port}, Interactive: {args.interactive}")
    
    try:
        # Create and run demo
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
