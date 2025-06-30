cmake_minimum_required(VERSION 3.14)
project(KolosalServer VERSION 1.0.0 LANGUAGES CXX)

# Use C++17 for this project (required for std::filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable position independent code for shared libraries
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|ARM64|arm64|armv8|armv7)")
    message(STATUS "Building on ARM: Enabling -fPIC")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else()
    message(STATUS "Building on x86: Enabling -fPIC for shared library compatibility")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if (MINGW)
    add_compile_definitions(_WIN32_WINNT=0x602)
endif()

# Options for inference engine
option(USE_CUDA   "Compile with CUDA support" OFF)
option(USE_VULKAN "Compile with VULKAN support" OFF)
option(USE_MPI    "Compile with MPI support" OFF)
option(DEBUG      "Compile with debugging information" OFF)
option(USE_CPPUPROFILE_GPU "Enable cppuprofile GPU monitoring" ON)

# Define include directories.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/cppuprofile/lib)

# Define source files.
set(SOURCES
    src/server.cpp
    src/server_api.cpp    
    src/server_config.cpp
    src/logger.cpp
    src/download_utils.cpp
    src/download_manager.cpp
    src/system_monitor.cpp
    src/completion_monitor.cpp
    src/enhanced_gpu_monitor.cpp
    src/routes/chat_completion_route.cpp
    src/routes/completion_route.cpp
    src/routes/add_engine_route.cpp
    src/routes/list_engines_route.cpp
    src/routes/remove_engine_route.cpp
    src/routes/engine_status_route.cpp
    src/routes/health_status_route.cpp    
    src/routes/auth_config_route.cpp    
    src/routes/system_metrics_route.cpp    
    src/routes/completion_metrics_route.cpp
    src/routes/combined_metrics_route.cpp
    src/routes/download_progress_route.cpp
    src/routes/downloads_status_route.cpp
    src/routes/cancel_download_route.cpp
    src/routes/pause_download_route.cpp
    src/routes/resume_download_route.cpp
    src/routes/cancel_all_downloads_route.cpp
    src/metrics_converter.cpp
    src/auth/rate_limiter.cpp
    src/auth/cors_handler.cpp
    src/auth/auth_middleware.cpp
    src/node_manager.cpp
    src/inference.cpp
)

# Create shared library instead of static library.
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD INFERENCE_EXPORTS)

# Create executable
add_executable(kolosal_server_exe src/main.cpp)
target_link_libraries(kolosal_server_exe PRIVATE kolosal_server)
set_target_properties(kolosal_server_exe PROPERTIES OUTPUT_NAME "kolosal-server")

# Find and setup yaml-cpp
set(YAML_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp")
if(NOT EXISTS "${YAML_CPP_PATH}/CMakeLists.txt")
  message(FATAL_ERROR "yaml-cpp not found at ${YAML_CPP_PATH}. Please clone it or adjust YAML_CPP_PATH.")
endif()

# Configure yaml-cpp options
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "Build yaml-cpp as static library" FORCE)

# Add yaml-cpp subdirectory
add_subdirectory(${YAML_CPP_PATH})

# Find and setup llama.cpp
set(LLAMA_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp")
if(NOT EXISTS "${LLAMA_CPP_PATH}/CMakeLists.txt")
  message(FATAL_ERROR "llama.cpp not found at ${LLAMA_CPP_PATH}. Please clone it or adjust LLAMA_CPP_PATH.")
endif()

# Find CURL
if(WIN32)
    # On Windows, use bundled CURL
    set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/curl" ${CMAKE_PREFIX_PATH})
    find_package(CURL REQUIRED)
    if(NOT CURL_FOUND)
        message(FATAL_ERROR "CURL not found")
    endif()
    message(STATUS "Found CURL: ${CURL_INCLUDE_DIR}")
else()
    # On Linux, use system CURL
    find_package(CURL REQUIRED)
    if(NOT CURL_FOUND)
        message(FATAL_ERROR "CURL not found. Please install libcurl4-openssl-dev (Ubuntu/Debian) or libcurl-devel (RHEL/CentOS)")
    endif()
    message(STATUS "Found system CURL: ${CURL_INCLUDE_DIRS}")
endif()

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

# Force position-independent code for llama.cpp when building shared library
if(BUILD_SHARED_LIBS OR CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # Also set the global CXX flags to ensure all targets get -fPIC
    if(NOT WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    endif()
    message(STATUS "Forcing position-independent code for llama.cpp compatibility")
endif()

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
  
  # Try to find Vulkan with better error handling
  find_package(Vulkan)
  if(Vulkan_FOUND)
    message(STATUS "Found Vulkan: ${Vulkan_LIBRARY}")
    message(STATUS "Vulkan headers: ${Vulkan_INCLUDE_DIRS}")
    target_include_directories(kolosal_server PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(kolosal_server PRIVATE ${Vulkan_LIBRARIES})
  else()
    message(WARNING "Vulkan not found. Falling back to CPU-only mode.")
    message(STATUS "To enable Vulkan support, install Vulkan development libraries:")
    message(STATUS "  Ubuntu/Debian: sudo apt install libvulkan-dev vulkan-tools vulkan-validationlayers-dev")
    message(STATUS "  RHEL/CentOS: sudo yum install vulkan-headers vulkan-loader-devel vulkan-tools")
    message(STATUS "  Fedora: sudo dnf install vulkan-headers vulkan-loader-devel vulkan-tools")
    message(STATUS "  Arch: sudo pacman -S vulkan-headers vulkan-icd-loader vulkan-tools")
    message(STATUS "")
    message(STATUS "Or run: ./install-linux-deps.sh")
    
    # Disable Vulkan and fall back to CPU
    set(GGML_VULKAN OFF CACHE BOOL "Disable GGML Vulkan support" FORCE)
    set(USE_VULKAN OFF)
    message(STATUS "Continuing build without Vulkan support...")
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

# Define inference-related compile definitions
target_compile_definitions(kolosal_server PUBLIC
  $<$<BOOL:${USE_CUDA}>:USE_CUDA>
  $<$<BOOL:${USE_VULKAN}>:USE_VULKAN>
  $<$<BOOL:${USE_MPI}>:USE_MPI>
  $<$<BOOL:${DEBUG}>:DEBUG>
  $<$<BOOL:${USE_CPPUPROFILE_GPU}>:USE_CPPUPROFILE_GPU>
)

# Add subdirectories for external libraries
add_subdirectory(${LLAMA_CPP_PATH})

# Force -fPIC for all llama.cpp targets when building as shared library
if(CMAKE_POSITION_INDEPENDENT_CODE)
    # Apply -fPIC to llama.cpp targets
    set_target_properties(llama PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(ggml PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(common PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(TARGET toolcall)
        set_target_properties(toolcall PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    message(STATUS "Applied -fPIC to llama.cpp static libraries")
endif()

# Only add yaml-cpp if it hasn't been added by llama.cpp
if(NOT TARGET yaml-cpp)
    add_subdirectory(external/yaml-cpp EXCLUDE_FROM_ALL)
    # Ensure yaml-cpp is built with -fPIC if needed
    if(CMAKE_POSITION_INDEPENDENT_CODE)
        set_target_properties(yaml-cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
        message(STATUS "Applied -fPIC to yaml-cpp static library")
    endif()
endif()

# Add cppuprofile if GPU monitoring is enabled
if(USE_CPPUPROFILE_GPU)
    # Configure cppuprofile with NVIDIA support
    set(GPU_MONITOR_NVIDIA ON CACHE BOOL "Enable NVIDIA GPU monitoring in cppuprofile" FORCE)
    add_subdirectory(external/cppuprofile/lib)
    message(STATUS "cppuprofile GPU monitoring enabled")
endif()

# Link libraries
set(KOLOSAL_LINK_LIBRARIES llama common ggml toolcall yaml-cpp)

# Add cppuprofile if enabled
if(USE_CPPUPROFILE_GPU)
    list(APPEND KOLOSAL_LINK_LIBRARIES cppuprofile)
endif()

target_link_libraries(kolosal_server PRIVATE ${KOLOSAL_LINK_LIBRARIES})

# Link CURL libraries and include directories
if(WIN32)
    target_include_directories(kolosal_server PRIVATE ${CURL_INCLUDE_DIR})
    target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARIES})
else()
    target_include_directories(kolosal_server PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARIES})
endif()

# Check for OpenSSL on Linux (often required for CURL)
if(UNIX AND NOT APPLE)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
        target_link_libraries(kolosal_server PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    else()
        message(WARNING "OpenSSL not found. HTTPS features might not work properly. Install libssl-dev (Ubuntu/Debian) or openssl-devel (RHEL/CentOS)")
    endif()
    
    # Check for zlib (often required for compression)
    find_package(ZLIB)
    if(ZLIB_FOUND)
        message(STATUS "Found zlib: ${ZLIB_VERSION_STRING}")
        target_link_libraries(kolosal_server PRIVATE ZLIB::ZLIB)
    else()
        message(WARNING "zlib not found. Install zlib1g-dev (Ubuntu/Debian) or zlib-devel (RHEL/CentOS)")
    endif()
endif()

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
    target_link_libraries(kolosal_server PRIVATE ws2_32 pdh psapi)
    # Copy the libcurl.dll to the bin directory
    add_custom_command(TARGET kolosal_server POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CURL_INCLUDE_DIR}/../bin/libcurl.dll"
        "$<TARGET_FILE_DIR:kolosal_server>"
      COMMENT "Copying libcurl.dll to bin directory"
    )
else()
    # For Linux, link with required system libraries
    target_link_libraries(kolosal_server PRIVATE dl rt pthread)
    # Add additional Linux dependencies
    if(EXISTS "/etc/debian_version")
        # Ubuntu/Debian specific
        message(STATUS "Detected Debian/Ubuntu system")
    elseif(EXISTS "/etc/redhat-release")
        # RHEL/CentOS/Fedora specific
        message(STATUS "Detected RedHat-based system")
    endif()
endif()

# Try to find and link NVML for GPU monitoring
if(WIN32)
    find_library(NVML_LIBRARY nvidia-ml 
        HINTS 
        "C:/Program Files/NVIDIA Corporation/NVSMI"
        "C:/Windows/System32"
        NO_DEFAULT_PATH
    )
else()
    # Linux NVML detection
    find_library(NVML_LIBRARY nvidia-ml 
        HINTS 
        "/usr/lib/x86_64-linux-gnu"
        "/usr/lib64"
        "/usr/local/cuda/lib64"
        "/usr/lib/nvidia"
        "/usr/lib/x86_64-linux-gnu/nvidia/current"
        "/usr/local/cuda/lib"
        "/usr/lib/nvidia-current"
        "/usr/lib/x86_64-linux-gnu/nvidia-384"
        "/usr/lib/x86_64-linux-gnu/nvidia-390"
        "/usr/lib/x86_64-linux-gnu/nvidia-410"
        "/usr/lib/x86_64-linux-gnu/nvidia-440"
        "/usr/lib/x86_64-linux-gnu/nvidia-450"
        "/usr/lib/x86_64-linux-gnu/nvidia-460"
        "/usr/lib/x86_64-linux-gnu/nvidia-470"
        "/usr/lib/x86_64-linux-gnu/nvidia-480"
        "/usr/lib/x86_64-linux-gnu/nvidia-510"
        "/usr/lib/x86_64-linux-gnu/nvidia-520"
        "/usr/lib/x86_64-linux-gnu/nvidia-530"
        NO_DEFAULT_PATH
    )
endif()

if(NVML_LIBRARY)
    target_link_libraries(kolosal_server PRIVATE ${NVML_LIBRARY})
    message(STATUS "Found NVML: ${NVML_LIBRARY}")
    target_compile_definitions(kolosal_server PRIVATE NVML_AVAILABLE)
else()
    message(STATUS "NVML not found - GPU monitoring will be limited")
endif()

# Add aggressive optimization flags for better performance
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2 /Ob2 /DNDEBUG")
    else()
        # Use safer optimization flags for better compatibility
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
        
        # Add march=native only if explicitly requested
        option(ENABLE_NATIVE_OPTIMIZATION "Enable native CPU optimization (-march=native)" OFF)
        if(ENABLE_NATIVE_OPTIMIZATION)
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -mtune=native")
            set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=native -mtune=native")
            message(STATUS "Native CPU optimization enabled")
        else()
            message(STATUS "Using safe optimization flags (use -DENABLE_NATIVE_OPTIMIZATION=ON for native optimization)")
        endif()
    endif()
    message(STATUS "Using aggressive optimization flags for Release build")
endif()

# Platform-specific configurations
if(UNIX AND NOT APPLE)
    # Linux-specific configurations
    message(STATUS "Configuring for Linux")
    
    # Check for required system packages
    find_program(PKG_CONFIG_EXECUTABLE pkg-config)
    if(PKG_CONFIG_EXECUTABLE)
        message(STATUS "Found pkg-config: ${PKG_CONFIG_EXECUTABLE}")
    else()
        message(WARNING "pkg-config not found. Some dependencies might not be detected properly.")
    endif()
    
    # Add Linux-specific compile definitions
    target_compile_definitions(kolosal_server PRIVATE 
        _GNU_SOURCE
        __LINUX__
    )
    
    # Enable large file support
    target_compile_definitions(kolosal_server PRIVATE
        _FILE_OFFSET_BITS=64
        _LARGEFILE_SOURCE
        _LARGEFILE64_SOURCE
    )
    
endif()

# Install header files.
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)