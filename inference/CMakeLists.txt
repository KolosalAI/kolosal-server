cmake_minimum_required(VERSION 3.14)
project(InferenceEngine VERSION 1.0.0 LANGUAGES CXX)

# Kolosal Inference Engine
# 
# Supports multiple acceleration backends:
# - CPU: Default fallback using OpenBLAS
# - CUDA: NVIDIA GPU acceleration (requires CUDA toolkit)
# - Vulkan: Cross-platform GPU acceleration (requires Vulkan SDK)
# - Metal: Apple GPU acceleration (macOS only, requires Metal frameworks)
# - MPI: Multi-process parallelization
#
# Usage:
# cmake -DUSE_CUDA=ON ..     # Enable CUDA acceleration
# cmake -DUSE_VULKAN=ON ..   # Enable Vulkan acceleration  
# cmake -DUSE_METAL=ON ..    # Enable Metal acceleration (macOS only)
# cmake -DUSE_MPI=ON ..      # Enable MPI support

# Include UCM for runtime library management (support both root and standalone inference builds)
set(_UCM_CANDIDATE_PATHS
    "${CMAKE_CURRENT_LIST_DIR}/cmake/ucm.cmake"           # if copied locally
    "${CMAKE_CURRENT_LIST_DIR}/../cmake/ucm.cmake"        # typical repo root when configuring from inference/
    "${CMAKE_SOURCE_DIR}/cmake/ucm.cmake"                 # when root is the overall project
)
set(_UCM_FOUND FALSE)
foreach(_ucm_path IN LISTS _UCM_CANDIDATE_PATHS)
    if(EXISTS "${_ucm_path}")
        include(${_ucm_path})
        set(_UCM_FOUND TRUE)
        message(STATUS "Included UCM from: ${_ucm_path}")
        break()
    endif()
endforeach()
if(NOT _UCM_FOUND)
    message(FATAL_ERROR "ucm.cmake not found. Looked in: ${_UCM_CANDIDATE_PATHS}")
endif()

# Static link the runtime libraries
ucm_set_runtime(STATIC)

# Use C++17 for this project (required for std::filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Windows symbol export
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

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

# Print UCM flags for debugging
ucm_print_flags()

# Options for inference engine
option(USE_CUDA   "Compile with CUDA support" OFF)
option(USE_VULKAN "Compile with VULKAN support" OFF)
option(USE_METAL  "Compile with Metal support (macOS only)" OFF)
option(USE_MPI    "Compile with MPI support" OFF)
option(DEBUG      "Compile with debugging information" OFF)

# Validate Metal option early
if(USE_METAL AND NOT APPLE)
    message(WARNING "Metal support is only available on macOS. Disabling USE_METAL.")
    set(USE_METAL OFF CACHE BOOL "Compile with Metal support (macOS only)" FORCE)
endif()

# Source files
set(SOURCES
    src/inference.cpp
)

# Header files
set(HEADERS
    include/inference.h
    include/inference_interface.h
)

# Determine the target name based on acceleration options
if(USE_CUDA)
    set(TARGET_NAME "llama-cuda")
    set(ACCELERATION_TYPE "cuda")
elseif(USE_VULKAN)
    set(TARGET_NAME "llama-vulkan")
    set(ACCELERATION_TYPE "vulkan")
elseif(USE_METAL)
    set(TARGET_NAME "llama-metal")
    set(ACCELERATION_TYPE "metal")
else()
    set(TARGET_NAME "llama-cpu")
    set(ACCELERATION_TYPE "cpu")
endif()

message(STATUS "Building inference engine: ${TARGET_NAME} (${ACCELERATION_TYPE})")

# Create the shared library target
add_library(${TARGET_NAME} SHARED ${SOURCES} ${HEADERS})

# Define compile definitions for the library
target_compile_definitions(${TARGET_NAME} PRIVATE INFERENCE_EXPORTS)

# Include directories
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# Find dependencies again if needed for target configuration
if(USE_VULKAN)
    find_package(Vulkan REQUIRED)
endif()

if(USE_CUDA)
    find_package(CUDA REQUIRED)
endif()

# Add Vulkan include directories if Vulkan is found and enabled
if(USE_VULKAN AND Vulkan_FOUND)
    target_include_directories(${TARGET_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_compile_definitions(${TARGET_NAME} PRIVATE USE_VULKAN)
endif()

# Add CUDA include directories if CUDA is found and enabled  
if(USE_CUDA AND CUDA_FOUND)
    target_include_directories(${TARGET_NAME} PRIVATE ${CUDA_INCLUDE_DIRS})
    target_compile_definitions(${TARGET_NAME} PRIVATE USE_CUDA)
endif()

# Add Metal definitions if Metal is enabled
if(USE_METAL)
    target_compile_definitions(${TARGET_NAME} PRIVATE USE_METAL GGML_USE_METAL)
endif()

# Find and setup llama.cpp (moved under inference/external)
# Primary (new) location: inference/external/llama.cpp
# Fallback (legacy) location: ../external/llama.cpp (for out-of-tree or un-migrated workspaces)
set(LLAMA_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp")
if(NOT EXISTS "${LLAMA_CPP_PATH}/CMakeLists.txt")
    set(LLAMA_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../external/llama.cpp")
    if(EXISTS "${LLAMA_CPP_PATH}/CMakeLists.txt")
        message(WARNING "Using legacy llama.cpp path: ${LLAMA_CPP_PATH}. Consider moving it to inference/external/llama.cpp")
    endif()
endif()
if(NOT EXISTS "${LLAMA_CPP_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "llama.cpp not found. Expected at inference/external/llama.cpp or external/llama.cpp relative to inference.")
endif()

# Find and setup dependencies first, before creating targets

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

# Find Vulkan early if needed
if(USE_VULKAN)
    find_package(Vulkan)
    if(Vulkan_FOUND)
        message(STATUS "Found Vulkan: ${Vulkan_LIBRARY}")
        message(STATUS "Vulkan headers: ${Vulkan_INCLUDE_DIRS}")
    else()
        message(WARNING "Vulkan not found. This will cause build failure.")
        message(STATUS "Please install Vulkan SDK from https://vulkan.lunarg.com/")
        message(FATAL_ERROR "Vulkan SDK required for Vulkan backend")
    endif()
endif()

# Find CUDA early if needed
if(USE_CUDA)
    find_package(CUDA REQUIRED)
    if(CUDA_FOUND)
        message(STATUS "Found CUDA: ${CUDA_INCLUDE_DIRS}")
    else()
        message(FATAL_ERROR "CUDA not found. Please install CUDA toolkit.")
    endif()
endif()

# Find Metal frameworks early if needed
if(USE_METAL)
    if(NOT APPLE)
        message(FATAL_ERROR "Metal support is only available on macOS. Please disable USE_METAL or build on macOS.")
    endif()
    
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(METALPERFORMANCE_FRAMEWORK MetalPerformanceShaders REQUIRED)
    
    if(METAL_FRAMEWORK AND FOUNDATION_FRAMEWORK AND METALKIT_FRAMEWORK AND METALPERFORMANCE_FRAMEWORK)
        message(STATUS "Found Metal framework: ${METAL_FRAMEWORK}")
        message(STATUS "Found Foundation framework: ${FOUNDATION_FRAMEWORK}")
        message(STATUS "Found MetalKit framework: ${METALKIT_FRAMEWORK}")
        message(STATUS "Found MetalPerformanceShaders framework: ${METALPERFORMANCE_FRAMEWORK}")
        
        # Add Metal-specific compiler flags
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(METAL_ARCH_FLAGS "-O3")
            message(STATUS "Using ARM64 optimizations for Metal")
        else()
            set(METAL_ARCH_FLAGS "-march=native -O3")
            message(STATUS "Using x86_64 optimizations for Metal")
        endif()
    else()
        message(FATAL_ERROR "Metal frameworks not found. Metal support requires macOS with Metal development frameworks.")
    endif()
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
    set(LLAMA_CUDA ON CACHE BOOL "Enable LLAMA CUDA support" FORCE)
    message(STATUS "Using CUDA for GGML acceleration")
    
    # On Windows with MSVC, we need to ensure CUDA uses the host C++ standard library
    if(WIN32 AND MSVC)
        # This flag tells nvcc to use host libstdc++ for device code
        set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --use-local-env")
        set(GGML_CUDA_USE_GRAPHS OFF CACHE BOOL "Disable CUDA graphs to avoid linking issues" FORCE)
    endif()
elseif(USE_VULKAN)
    set(GGML_VULKAN ON CACHE BOOL "Enable GGML Vulkan support" FORCE)
    message(STATUS "Using vulkan for GGML acceleration")
elseif(USE_METAL)
    set(GGML_METAL ON CACHE BOOL "Enable GGML Metal support" FORCE)
    message(STATUS "Using Metal for GGML acceleration")
else()
    message(STATUS "Using OpenBLAS for GGML acceleration")
endif()

# Link libraries based on acceleration type
if(USE_CUDA AND CUDA_FOUND)
    # Link CUDA runtime, cuBLAS, and the CUDA driver library
    target_link_libraries(${TARGET_NAME} PRIVATE ${CUDA_LIBRARIES} ${CUDA_CUBLAS_LIBRARIES} ${CUDA_CUDA_LIBRARY})
    # On Windows, we also need to link against the math library to resolve missing symbols
    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE ${CUDA_cudart_static_LIBRARY})
    endif()
endif()

if(USE_VULKAN AND Vulkan_FOUND)
    target_link_libraries(${TARGET_NAME} PRIVATE ${Vulkan_LIBRARIES})
endif()

if(USE_METAL)
    target_link_libraries(${TARGET_NAME} PRIVATE 
        ${METAL_FRAMEWORK} 
        ${FOUNDATION_FRAMEWORK} 
        ${METALKIT_FRAMEWORK}
        ${METALPERFORMANCE_FRAMEWORK}
    )
    
    target_compile_options(${TARGET_NAME} PRIVATE ${METAL_ARCH_FLAGS})
endif()

# Find and configure MPI if enabled
if(USE_MPI)
    find_package(MPI REQUIRED)
    if(MPI_FOUND)
        target_include_directories(${TARGET_NAME} PRIVATE ${MPI_CXX_INCLUDE_DIRS})
        target_link_libraries(${TARGET_NAME} PRIVATE ${MPI_CXX_LIBRARIES})
        target_compile_definitions(${TARGET_NAME} PRIVATE ${MPI_CXX_COMPILE_DEFINITIONS})
        if(MPI_CXX_COMPILE_FLAGS)
            set_target_properties(${TARGET_NAME} PROPERTIES
                COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}")
        endif()
        if(MPI_CXX_LINK_FLAGS)
            set_target_properties(${TARGET_NAME} PROPERTIES
                LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")
        endif()
        message(STATUS "Found MPI: ${MPI_CXX_INCLUDE_DIRS}")
    else()
        message(FATAL_ERROR "MPI not found. Please install MPI implementation (OpenMPI, MPICH, or MS-MPI).")
    endif()
endif()

# Define inference-related compile definitions
target_compile_definitions(${TARGET_NAME} PUBLIC
    $<$<BOOL:${USE_CUDA}>:USE_CUDA>
    $<$<BOOL:${USE_VULKAN}>:USE_VULKAN>
    $<$<BOOL:${USE_METAL}>:USE_METAL>
    $<$<BOOL:${USE_MPI}>:USE_MPI>
    $<$<BOOL:${DEBUG}>:DEBUG>
)

# Add subdirectories for external libraries
add_subdirectory(${LLAMA_CPP_PATH} llama-cpp-build)

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

# Link llama.cpp libraries
# Link only the required llama.cpp libraries (toolcall disabled)
set(LLAMA_LINK_LIBRARIES llama common ggml)
target_link_libraries(${TARGET_NAME} PRIVATE ${LLAMA_LINK_LIBRARIES})

# For CUDA on Windows, we need to link additional libraries for math functions
if(USE_CUDA AND WIN32)
    # Add CUDA device library which contains math functions
    target_link_libraries(${TARGET_NAME} PRIVATE ${CUDA_cudadevrt_LIBRARY})
endif()

# On MSVC, ensure symbols from static libs are included into this shared lib
if(MSVC)
    foreach(lib ${LLAMA_LINK_LIBRARIES})
        # Force the linker to include all public symbols from the static library
        # This prevents missing imports like 'llama_max_devices' when nothing references them directly
        target_link_options(${TARGET_NAME} PRIVATE 
            $<$<STREQUAL:$<TARGET_PROPERTY:${lib},TYPE>,STATIC_LIBRARY>:/WHOLEARCHIVE:$<TARGET_FILE:${lib}>>)
    endforeach()
endif()

# Link CURL libraries and include directories
if(WIN32)
    target_include_directories(${TARGET_NAME} PRIVATE ${CURL_INCLUDE_DIR})
    target_link_libraries(${TARGET_NAME} PRIVATE ${CURL_LIBRARIES})
else()
    target_include_directories(${TARGET_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(${TARGET_NAME} PRIVATE ${CURL_LIBRARIES})
endif()

# Check for OpenSSL on Linux (often required for CURL)
if(UNIX AND NOT APPLE)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
        target_link_libraries(${TARGET_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    else()
        message(WARNING "OpenSSL not found. HTTPS features might not work properly. Install libssl-dev (Ubuntu/Debian) or openssl-devel (RHEL/CentOS)")
    endif()
    
    # Check for zlib (often required for compression)
    find_package(ZLIB)
    if(ZLIB_FOUND)
        message(STATUS "Found zlib: ${ZLIB_VERSION_STRING}")
        target_link_libraries(${TARGET_NAME} PRIVATE ZLIB::ZLIB)
    else()
        message(WARNING "zlib not found. Install zlib1g-dev (Ubuntu/Debian) or zlib-devel (RHEL/CentOS)")
    endif()
endif()

# Include llama.cpp directories
target_include_directories(${TARGET_NAME} PUBLIC
    ${LLAMA_CPP_PATH}/include
    ${LLAMA_CPP_PATH}/common
    ${LLAMA_CPP_PATH}/ggml/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# Find and link thread library
find_package(Threads REQUIRED)
target_link_libraries(${TARGET_NAME} PRIVATE Threads::Threads)

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE ws2_32)
    # For CUDA on Windows, we need to link the standard C library for math functions
    if(USE_CUDA AND MSVC)
        # Link the MSVC runtime library explicitly to resolve CUDA math function symbols
        target_link_options(${TARGET_NAME} PRIVATE /DEFAULTLIB:libucrt.lib /DEFAULTLIB:libcmt.lib)
    endif()
    # Copy the libcurl.dll to the bin directory
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CURL_INCLUDE_DIR}/../bin/libcurl.dll"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>"
        COMMENT "Copying libcurl.dll to bin directory"
    )
elseif(APPLE)
    # For macOS, link with required system libraries
    target_link_libraries(${TARGET_NAME} PRIVATE dl pthread)
    
    # Add macOS-specific compile definitions
    target_compile_definitions(${TARGET_NAME} PRIVATE 
        __APPLE__
        __MACH__
    )
    
    # macOS-specific compiler flags for better performance
    if(USE_METAL)
        target_compile_options(${TARGET_NAME} PRIVATE 
            -Wno-format 
            -Wno-unused-variable 
            -Wno-unused-function
        )
    endif()
else()
    # For Linux, link with required system libraries
    target_link_libraries(${TARGET_NAME} PRIVATE dl rt pthread)
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
    message(STATUS "Using aggressive optimization flags for Release build"    )
endif()

# Platform-specific configurations
if(APPLE)
    # macOS-specific configurations
    message(STATUS "Configuring for macOS")
    
    # Set minimum macOS version for Metal support
    if(USE_METAL)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version for Metal support")
        message(STATUS "Setting minimum macOS version to 10.13 for Metal support")
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "Minimum macOS version")
    endif()
    
    # Add macOS-specific compile definitions
    target_compile_definitions(${TARGET_NAME} PRIVATE 
        _DARWIN_C_SOURCE
        __APPLE__
    )
    
    # Enable large file support
    target_compile_definitions(${TARGET_NAME} PRIVATE
        _FILE_OFFSET_BITS=64
        _LARGEFILE_SOURCE
        _LARGEFILE64_SOURCE
    )
    
    # Add Accelerate framework for better performance
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    if(ACCELERATE_FRAMEWORK)
        target_link_libraries(${TARGET_NAME} PRIVATE ${ACCELERATE_FRAMEWORK})
        message(STATUS "Found Accelerate framework: ${ACCELERATE_FRAMEWORK}")
    endif()
elseif(UNIX AND NOT APPLE)
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
    target_compile_definitions(${TARGET_NAME} PRIVATE 
        _GNU_SOURCE
        __LINUX__
    )
    
    # Enable large file support
    target_compile_definitions(${TARGET_NAME} PRIVATE
        _FILE_OFFSET_BITS=64
        _LARGEFILE_SOURCE
        _LARGEFILE64_SOURCE
    )
endif()

# Set the output directory
set_target_properties(${TARGET_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Add examples directory if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()

# Install targets
install(TARGETS ${TARGET_NAME} 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

option(BUILD_INFERENCE_TESTS "Build simple inference smoke test" ON)
if(BUILD_INFERENCE_TESTS)
    add_executable(inference_smoketest tests/test_inference.cpp)
    target_link_libraries(inference_smoketest PRIVATE ${TARGET_NAME})
    target_include_directories(inference_smoketest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    set_target_properties(inference_smoketest PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    if(APPLE)
        # Ensure the test can locate the dylib at runtime
        set_target_properties(inference_smoketest PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "@executable_path;@executable_path/../lib;@loader_path"
        )
    elseif(UNIX)
        set_target_properties(inference_smoketest PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "\$ORIGIN;\$ORIGIN/../lib"
        )
    endif()
    # Only install smoketest if the target was actually built
    install(TARGETS inference_smoketest 
        RUNTIME DESTINATION bin
        OPTIONAL
    )

    # Additional focused test executables
    set(INFERENCE_TEST_SOURCES
        tests/test_completion_basic.cpp
        tests/test_chat.cpp
        tests/test_chat_multiturn.cpp
        tests/test_session_cache.cpp
        tests/test_parallel.cpp
        tests/test_embedding.cpp
        tests/test_invalid_and_cancel.cpp
        tests/test_grammar.cpp
        tests/test_json_schema.cpp
        tests/test_multi_gpu.cpp
        tests/test_auto_seq.cpp
    )

    foreach(test_src ${INFERENCE_TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src} tests/test_common.h)
        target_link_libraries(${test_name} PRIVATE ${TARGET_NAME})
        # Special handling for test_multi_gpu which needs direct access to llama functions
        if(test_name STREQUAL "test_multi_gpu")
            target_link_libraries(${test_name} PRIVATE ${LLAMA_LINK_LIBRARIES})
        endif()
        target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(${test_name} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        if(APPLE)
            set_target_properties(${test_name} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "@executable_path;@executable_path/../lib;@loader_path"
            )
        elseif(UNIX)
            set_target_properties(${test_name} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "\$ORIGIN;\$ORIGIN/../lib"
            )
        endif()
        # Only install test if the target was actually built
        install(TARGETS ${test_name} 
            RUNTIME DESTINATION bin
            OPTIONAL
        )
    endforeach()

    # Convenience aggregate target to build all inference test executables at once
    add_custom_target(inference_tests
        DEPENDS
            inference_smoketest
            test_completion_basic
            test_chat
            test_session_cache
            test_parallel
            test_embedding
            test_invalid_and_cancel
            test_multi_gpu
    )
endif()

# Install header files
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Export the target name for parent CMakeLists.txt
set(INFERENCE_TARGET_NAME ${TARGET_NAME} PARENT_SCOPE)
set(INFERENCE_ACCELERATION_TYPE ${ACCELERATION_TYPE} PARENT_SCOPE)

# Export the headers to the parent scope as well
target_include_directories(${TARGET_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${LLAMA_CPP_PATH}/include>
    $<BUILD_INTERFACE:${LLAMA_CPP_PATH}/common>
    $<BUILD_INTERFACE:${LLAMA_CPP_PATH}/ggml/include>
    $<INSTALL_INTERFACE:include>
)

# Fix install name for macOS libraries
if(APPLE)
    # Fix the install name of the inference engine dylib to use @rpath with lib prefix
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND install_name_tool -id "@rpath/lib${TARGET_NAME}.dylib" "$<TARGET_FILE:${TARGET_NAME}>"
        COMMENT "Setting install name for lib${TARGET_NAME}.dylib"
    )
endif()
