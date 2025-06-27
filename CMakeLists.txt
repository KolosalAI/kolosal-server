cmake_minimum_required(VERSION 3.14)
project(KolosalServer VERSION 1.0.0 LANGUAGES CXX)

# Use C++17 for this project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MINGW)
    add_compile_definitions(_WIN32_WINNT=0x602)
endif()

# Options for inference engine
option(USE_CUDA   "Compile with CUDA support" OFF)
option(USE_VULKAN "Compile with VULKAN support" OFF)
option(USE_MPI    "Compile with MPI support" OFF)
option(DEBUG      "Compile with debugging information" OFF)
option(ENABLE_AGENTS "Enable multi-agent system" ON)

# Define include directories.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann)

# Define source files.
set(SOURCES
    src/server.cpp
    src/server_api.cpp    
    src/logger.cpp
    src/download_utils.cpp
    src/routes/chat_completion_route.cpp
    src/routes/completion_route.cpp
    src/routes/add_engine_route.cpp
    src/routes/list_engines_route.cpp
    src/routes/remove_engine_route.cpp
    src/routes/engine_status_route.cpp
    src/routes/health_status_route.cpp
    src/routes/auth_config_route.cpp
    src/auth/rate_limiter.cpp
    src/auth/cors_handler.cpp
    src/auth/auth_middleware.cpp
    src/node_manager.cpp
    src/inference.cpp
)

# Add agent system sources if enabled
if(ENABLE_AGENTS)
    list(APPEND SOURCES
        # Agent system core
        src/agents/agent_data.cpp
        src/agents/agent_interfaces.cpp
        src/agents/yaml_config.cpp
        
        # Agent functions
        src/agents/builtin_functions.cpp
        src/agents/function_manager.cpp
        
        # Agent infrastructure
        src/agents/job_manager.cpp
        src/agents/message_router.cpp
        src/agents/event_system.cpp
        
        # Agent core and system
        src/agents/agent_core.cpp
        src/agents/multi_agent_system.cpp
        src/agents/agent_orchestrator.cpp
        
        # Agent API routes
        src/routes/agents_route.cpp
        src/routes/orchestration_route.cpp
        src/routes/sequential_workflow_route.cpp
        src/routes/auto_setup_route.cpp
        
        # Sequential workflow system
        src/agents/sequential_workflow.cpp
        
        # Auto-setup system
        src/auto_setup_manager.cpp
    )
    message(STATUS "Agent system enabled - adding agent sources")
endif()

# Create shared library instead of static library.
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD INFERENCE_EXPORTS)

# Create executable
add_executable(kolosal_server_exe src/main.cpp)
target_link_libraries(kolosal_server_exe PRIVATE kolosal_server)
set_target_properties(kolosal_server_exe PROPERTIES OUTPUT_NAME "kolosal-server")

# Find and setup llama.cpp
set(LLAMA_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp")
if(NOT EXISTS "${LLAMA_CPP_PATH}/CMakeLists.txt")
  message(FATAL_ERROR "llama.cpp not found at ${LLAMA_CPP_PATH}. Please clone it or adjust LLAMA_CPP_PATH.")
endif()

# Find CURL
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/curl" ${CMAKE_PREFIX_PATH})
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    message(FATAL_ERROR "CURL not found")
endif()
message(STATUS "Found CURL: ${CURL_INCLUDE_DIR}")

# Configure llama.cpp options
set(GGML_NATIVE           OFF CACHE BOOL "Disable LLAMA_NATIVE in llama.cpp"    FORCE)
set(INS_ENB               ON  CACHE BOOL "Enable INS_ENB in llama.cpp"          FORCE)
set(LLAMA_BUILD_TESTS     OFF CACHE BOOL "Disable llama.cpp tests"              FORCE)
set(LLAMA_BUILD_EXAMPLES  OFF CACHE BOOL "Disable llama.cpp examples"           FORCE)
set(LLAMA_BUILD_SERVER    OFF CACHE BOOL "Disable llama.cpp server"             FORCE)
set(LLAMA_BUILD_COMMON    ON  CACHE BOOL "Enable  llama.cpp common"             FORCE)
set(LLAMA_ALL_WARNINGS    OFF CACHE BOOL "Disable warnings in llama.cpp"        FORCE)
set(LLAMA_CURL            ON  CACHE BOOL "Enable curl in llama.cpp"             FORCE)
set(LLAMA_TOOLCALL        ON  CACHE BOOL "Enable llama.cpp toolcall"            FORCE)
set(BUILD_SHARED_LIBS     OFF CACHE BOOL "Build llama.cpp as a static lib"      FORCE)
set(GGML_STATIC_LINK      ON  CACHE BOOL "Static link ggml libraries"           FORCE)
set(GGML_STATIC           ON  CACHE BOOL "Static link ggml libraries"           FORCE)
set(LLAMA_AVX512          OFF CACHE BOOL "Disable AVX512 in llama.cpp"          FORCE)

# Enable GGML acceleration based on options
if(USE_CUDA)
  set(GGML_CUDA ON CACHE BOOL "Enable GGML CUDA support" FORCE)
  message(STATUS "Using CUDA for GGML acceleration")
  
  find_package(CUDA REQUIRED)
  if(CUDA_FOUND)
    target_include_directories(kolosal_server PRIVATE ${CUDA_INCLUDE_DIRS})
    target_link_libraries(kolosal_server PRIVATE ${CUDA_LIBRARIES} ${CUDA_CUBLAS_LIBRARIES} ${CUDA_CUDA_LIBRARY})
  else()
    message(FATAL_ERROR "CUDA not found. Please install CUDA toolkit.")
  endif()
elseif(USE_VULKAN)
  set(GGML_VULKAN ON CACHE BOOL "Enable GGML Vulkan support" FORCE)
  message(STATUS "Using vulkan for GGML acceleration")
  
  find_package(Vulkan REQUIRED)
  if(Vulkan_FOUND)
    target_include_directories(kolosal_server PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(kolosal_server PRIVATE ${Vulkan_LIBRARIES})
  else()
    message(FATAL_ERROR "Vulkan not found. Please install Vulkan SDK.")
  endif()
else()
  message(STATUS "Using OpenBLAS for GGML acceleration")
endif()

# Find and configure MPI if enabled
if(USE_MPI)
  find_package(MPI REQUIRED)
  if(MPI_FOUND)
    target_include_directories(kolosal_server PRIVATE ${MPI_CXX_INCLUDE_DIRS})
    target_link_libraries(kolosal_server PRIVATE ${MPI_CXX_LIBRARIES})
    target_compile_definitions(kolosal_server PRIVATE ${MPI_CXX_COMPILE_DEFINITIONS})
    if(MPI_CXX_COMPILE_FLAGS)
      set_target_properties(kolosal_server PROPERTIES
        COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}")
    endif()
    if(MPI_CXX_LINK_FLAGS)
      set_target_properties(kolosal_server PROPERTIES
        LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")
    endif()
    message(STATUS "Found MPI: ${MPI_CXX_INCLUDE_DIRS}")
  else()
    message(FATAL_ERROR "MPI not found. Please install MPI implementation (OpenMPI, MPICH, or MS-MPI).")
  endif()
endif()

# Setup yaml-cpp for agent system
if(ENABLE_AGENTS)
    message(STATUS "Agent system enabled - setting up yaml-cpp...")
    
    # Try to find system yaml-cpp first
    find_package(yaml-cpp QUIET)
    
    if(NOT yaml-cpp_FOUND)
        # Check for local yaml-cpp
        set(YAML_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp")
        if(EXISTS "${YAML_CPP_PATH}/CMakeLists.txt")
            message(STATUS "Found local yaml-cpp at: ${YAML_CPP_PATH}")
            
            # Configure yaml-cpp options to avoid conflicts
            set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests" FORCE)
            set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools" FORCE)
            set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib" FORCE)
            set(YAML_CPP_INSTALL OFF CACHE BOOL "Disable yaml-cpp install" FORCE)
            set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build yaml-cpp as static" FORCE)
            
            # Temporarily set minimum required version for yaml-cpp
            set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
            
            # Add yaml-cpp as subdirectory
            add_subdirectory(${YAML_CPP_PATH} yaml-cpp EXCLUDE_FROM_ALL)
            
            # Create an alias target for consistency
            if(NOT TARGET yaml-cpp::yaml-cpp)
                add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
            endif()
            
            message(STATUS "Built yaml-cpp from source")
        else()
            message(STATUS "yaml-cpp not found locally, downloading...")
            
            # Download yaml-cpp using FetchContent
            include(FetchContent)
            FetchContent_Declare(
                yaml-cpp
                GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
                GIT_TAG 0.8.0
            )
            
            # Configure yaml-cpp options
            set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests" FORCE)
            set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools" FORCE)
            set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib" FORCE)
            set(YAML_CPP_INSTALL OFF CACHE BOOL "Disable yaml-cpp install" FORCE)
            
            FetchContent_MakeAvailable(yaml-cpp)
            message(STATUS "Downloaded and built yaml-cpp")
        endif()
    else()
        message(STATUS "Found system yaml-cpp")
    endif()
    
    # Link yaml-cpp to the main target
    target_link_libraries(kolosal_server PRIVATE yaml-cpp::yaml-cpp)
    target_compile_definitions(kolosal_server PRIVATE KOLOSAL_AGENTS_ENABLED)
endif()

# Define inference-related compile definitions
target_compile_definitions(kolosal_server PUBLIC
  $<$<BOOL:${USE_CUDA}>:USE_CUDA>
  $<$<BOOL:${USE_VULKAN}>:USE_VULKAN>
  $<$<BOOL:${USE_MPI}>:USE_MPI>
  $<$<BOOL:${DEBUG}>:DEBUG>
  $<$<BOOL:${ENABLE_AGENTS}>:KOLOSAL_AGENTS_ENABLED>
)

# Add llama.cpp subdirectory
add_subdirectory(${LLAMA_CPP_PATH})

# Link llama.cpp libraries
target_link_libraries(kolosal_server PRIVATE llama common ggml toolcall)

# Link CURL libraries and include directories
target_include_directories(kolosal_server PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARIES})

# Include llama.cpp directories
target_include_directories(kolosal_server PUBLIC
  ${LLAMA_CPP_PATH}/include
  ${LLAMA_CPP_PATH}/common
  ${LLAMA_CPP_PATH}/ggml/include
)

# Find and link thread library for the main library.
find_package(Threads REQUIRED)
target_link_libraries(kolosal_server PRIVATE Threads::Threads)

# Platform-specific link libraries for the main library.
if(WIN32)
    target_link_libraries(kolosal_server PRIVATE ws2_32)
    # Copy the libcurl.dll to the bin directory
    add_custom_command(TARGET kolosal_server POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CURL_INCLUDE_DIR}/../bin/libcurl.dll"
        "$<TARGET_FILE_DIR:kolosal_server>"
      COMMENT "Copying libcurl.dll to bin directory"
    )
endif()

# Add installation targets.
install(TARGETS kolosal_server kolosal_server_exe
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install header files.
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)