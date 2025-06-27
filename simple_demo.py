#!/usr/bin/env python3
"""
Simple Workflow Demo - Shows how easy workflows are now!

This demonstrates the dramatically simplified workflow API.
Compare this to the complex legacy approach!
"""

from simple_workflow_client import SimpleWorkflowClient, quick_content_creation, quick_code_generation

def main():
    print("🎉 Welcome to the Simple Workflow Demo!")
    print("See how easy workflows are now compared to the complex original demo.\n")
    
    # Method 1: Ultra-simple one-liners (no setup needed!)
    print("="*60)
    print("METHOD 1: One-Line Workflows (Zero Setup!)")
    print("="*60)
    
    print("\n📝 Creating content about AI with just one line:")
    print("quick_content_creation('AI in Healthcare', 'doctors')")
    
    result = quick_content_creation("AI in Healthcare", "doctors")
    if result:
        print("✅ Done! Content created successfully.")
    else:
        print("❌ Failed - check if server is running")
    
    print("\n💻 Generating code with just one line:")
    print("quick_code_generation('REST API for user authentication')")
    
    result = quick_code_generation("REST API for user authentication")
    if result:
        print("✅ Done! Code generated successfully.")
    else:
        print("❌ Failed - check if server is running")
    
    # Method 2: Slightly more control with client
    print("\n" + "="*60)
    print("METHOD 2: Simple Client (Auto Setup)")
    print("="*60)
    
    print("\n🚀 Creating client (auto-detects everything):")
    client = SimpleWorkflowClient()  # That's it! Auto-setup handles everything
    
    print("\n📊 Available agents:")
    for agent in client.get_available_agents():
        print(f"  - {agent}")
    
    print("\n🔧 Creating custom workflow:")
    workflow = client.create_workflow("demo", "Demo Workflow")
    workflow.add_research_step("blockchain technology")
    workflow.add_writing_step("technical summary")
    
    print("   ✓ Added research step")
    print("   ✓ Added writing step")
    
    print("\n▶️  Executing workflow:")
    result = client.execute_workflow(workflow)
    if result:
        print("✅ Custom workflow completed!")
        
        # Show some results
        step_results = result.get('step_results', {})
        success_count = sum(1 for step in step_results.values() if step.get('success'))
        print(f"📊 Executed {len(step_results)} steps, {success_count} successful")
    else:
        print("❌ Custom workflow failed")
    
    # Method 3: Built-in workflow templates
    print("\n" + "="*60)
    print("METHOD 3: Built-in Templates")
    print("="*60)
    
    print("\n📈 Running data analysis workflow:")
    result = client.run_data_analysis_workflow("customer satisfaction survey data")
    if result:
        print("✅ Data analysis completed!")
    
    print("\n🎯 COMPARISON WITH OLD DEMO:")
    print("  Old way: 1200+ lines, complex setup, manual agent mapping")
    print("  New way: 3-10 lines, zero setup, everything automatic!")
    print("\n🎉 Workflows are now incredibly simple to use!")

if __name__ == "__main__":
    main()
