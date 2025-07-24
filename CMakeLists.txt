cmake_minimum_required(VERSION 3.14)
project(KolosalServer VERSION 1.0.0 LANGUAGES CXX)

# Include UCM for runtime library management
include(${CMAKE_SOURCE_DIR}/cmake/ucm.cmake)

# Configure runtime library for consistency
if(MSVC)
    # WORKAROUND: Use Release runtime (/MD) for all builds to match pre-compiled MuPDF libraries
    # This prevents LNK2038 runtime library mismatch errors
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    
    # Clear any existing runtime flags first for all configurations
    string(REGEX REPLACE "/M[TD]d?" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "/M[TD]d?" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/M[TD]d?" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REGEX REPLACE "/M[TD]d?" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "/M[TD]d?" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REGEX REPLACE "/M[TD]d?" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REGEX REPLACE "/M[TD]d?" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
    string(REGEX REPLACE "/M[TD]d?" "" CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
    
    # Set Release runtime (/MD) for ALL build configurations to match MuPDF
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MD")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MD")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MD")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MD")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MD")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MD")
    
    # Also disable iterator debug level in Debug builds to match MuPDF
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
        message(STATUS "Using Release runtime (/MD) and _ITERATOR_DEBUG_LEVEL=0 for Debug build to match MuPDF")
    else()
        message(STATUS "Using Release runtime (/MD) for Release build")
    endif()
else()
    # For non-MSVC compilers, use dynamic linking
    set(BUILD_SHARED_LIBS ON)
endif()

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

# Print UCM flags for debugging
ucm_print_flags()

# Options for inference engine
option(USE_CUDA   "Compile with CUDA support" OFF)
option(USE_VULKAN "Compile with VULKAN support" OFF)
option(USE_MPI    "Compile with MPI support" OFF)
option(DEBUG      "Compile with debugging information" OFF)

# Define include directories.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/cppuprofile/lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inference/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/ggml/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/pugixml/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf/thirdparty/zlib/contrib/minizip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf/thirdparty/zlib)

# Define source files.
set(SOURCES
    src/server.cpp
    src/server_api.cpp    
    src/server_config.cpp
    src/logger.cpp
    src/download_utils.cpp
    src/download_manager.cpp
    src/inference_loader.cpp
    src/inference_interface_impl.cpp

    # Model implementations
    src/models/embedding_request_model.cpp
    src/models/embedding_response_model.cpp

    # Basic routes
    src/routes/chat_completion_route.cpp
    src/routes/completion_route.cpp
    src/routes/inference_completion_route.cpp
    src/routes/inference_chat_completion_route.cpp
    src/routes/models_route.cpp
    src/routes/list_inference_engines_route.cpp
    src/routes/health_status_route.cpp    
    src/routes/auth_config_route.cpp    
    src/routes/server_logs_route.cpp    
    src/routes/downloads_route.cpp

    # Agent-related routes
    src/routes/agents_route.cpp
    src/routes/agent_monitoring_route.cpp
    src/routes/orchestration_route.cpp
    src/routes/sequential_workflow_route.cpp
    src/routes/function_execution_route.cpp
    src/routes/collaboration_route.cpp
    src/routes/agent_documents_route.cpp

    # Document retrieval routes
    src/routes/retrieve_route.cpp
    src/routes/retrieve_test_route.cpp
    src/routes/add_documents_route.cpp
    src/routes/parse_pdf_route.cpp
    src/routes/parse_docx_route.cpp
    src/routes/parse_pdf_file_route.cpp
    src/routes/parse_docx_file_route.cpp
    src/routes/embedding_route.cpp
    src/routes/internet_search_route.cpp
    src/routes/auto_setup_route.cpp
    src/routes/documents_route.cpp
    src/routes/documents_upload_route.cpp
    src/routes/document_search_route.cpp
    src/routes/collections_route.cpp
    src/routes/collections_list_route.cpp
    src/routes/collections_delete_route.cpp
    src/routes/bulk_operations_route.cpp
    src/routes/context_retrieval_route.cpp
    src/routes/vector_search_route.cpp
    src/routes/qdrant_route.cpp
    src/routes/not_found_route.cpp
    src/routes/rag_route.cpp
    src/routes/workflow_route.cpp
    src/routes/session_route.cpp

    # Agent system core
    src/agents/agent_core.cpp
    src/agents/agent_data.cpp
    src/agents/agent_config_validator.cpp
    src/agents/agent_interfaces.cpp
    src/agents/agent_orchestrator.cpp
    src/agents/multi_agent_system.cpp
    src/agents/sequential_workflow.cpp
    src/agents/yaml_config.cpp
    src/agents/message_router.cpp
    src/agents/function_manager.cpp
    src/agents/job_manager.cpp
    src/agents/event_system.cpp
    src/agents/builtin_functions.cpp

    # Authentication and authorization
    src/auth/rate_limiter.cpp
    src/auth/cors_handler.cpp
    src/auth/auth_middleware.cpp
    
    # Other core files
    src/node_manager.cpp
    src/auto_setup_manager.cpp
    src/qdrant_client.cpp

    # Document retrieval system
    src/retrieval/document_service.cpp
    src/retrieval/parse_pdf.cpp
    src/retrieval/parse_docx.cpp
    src/retrieval/add_document_types.cpp
    src/retrieval/remove_document_types.cpp
    src/retrieval/retrieve_types.cpp
    src/retrieval/remove_documents_route.cpp

    # External dependencies
    external/pugixml/src/pugixml.cpp
    
    # zlib source files (needed for minizip)
    external/mupdf/thirdparty/zlib/adler32.c
    external/mupdf/thirdparty/zlib/compress.c
    external/mupdf/thirdparty/zlib/crc32.c
    external/mupdf/thirdparty/zlib/deflate.c
    external/mupdf/thirdparty/zlib/gzclose.c
    external/mupdf/thirdparty/zlib/gzlib.c
    external/mupdf/thirdparty/zlib/gzread.c
    external/mupdf/thirdparty/zlib/gzwrite.c
    external/mupdf/thirdparty/zlib/infback.c
    external/mupdf/thirdparty/zlib/inffast.c
    external/mupdf/thirdparty/zlib/inflate.c
    external/mupdf/thirdparty/zlib/inftrees.c
    external/mupdf/thirdparty/zlib/trees.c
    external/mupdf/thirdparty/zlib/uncompr.c
    external/mupdf/thirdparty/zlib/zutil.c
    
    # minizip files
    external/mupdf/thirdparty/zlib/contrib/minizip/unzip.c
    external/mupdf/thirdparty/zlib/contrib/minizip/ioapi.c
)

# Create shared library instead of static library.
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD)

# Add MSVC-specific compile options to fix PDB file access issues
if(MSVC)
    target_compile_options(kolosal_server PRIVATE /FS /bigobj)
    # Completely disable parallel compilation to prevent PDB conflicts
    target_compile_options(kolosal_server PRIVATE /MP1)
    
    # Set runtime library to match MuPDF (always use Release runtime)
    set_target_properties(kolosal_server PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
    target_compile_options(kolosal_server PRIVATE
        $<$<CONFIG:Debug>:/MD>
        $<$<CONFIG:Release>:/MD>
        $<$<CONFIG:RelWithDebInfo>:/MD>
        $<$<CONFIG:MinSizeRel>:/MD>
    )
    # Disable iterator debug level for all configurations to match MuPDF
    target_compile_definitions(kolosal_server PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    message(STATUS "Using Release runtime (/MD) and _ITERATOR_DEBUG_LEVEL=0 for kolosal_server to match MuPDF")
    
    message(STATUS "Added /FS, /bigobj, /MP1, and runtime library options to kolosal_server target for MSVC")
endif()

# Create executable
add_executable(kolosal_server_exe src/main.cpp)
target_link_libraries(kolosal_server_exe PRIVATE kolosal_server)
set_target_properties(kolosal_server_exe PROPERTIES OUTPUT_NAME "kolosal-server")

# Add MSVC-specific compile options to the executable as well
if(MSVC)
    target_compile_options(kolosal_server_exe PRIVATE /FS /bigobj)
    target_compile_options(kolosal_server_exe PRIVATE /MP1)
    
    # Set runtime library to match MuPDF (always use Release runtime)
    set_target_properties(kolosal_server_exe PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
    target_compile_options(kolosal_server_exe PRIVATE
        $<$<CONFIG:Debug>:/MD>
        $<$<CONFIG:Release>:/MD>
        $<$<CONFIG:RelWithDebInfo>:/MD>
        $<$<CONFIG:MinSizeRel>:/MD>
    )
    # Disable iterator debug level for all configurations to match MuPDF
    target_compile_definitions(kolosal_server_exe PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    message(STATUS "Using Release runtime (/MD) and _ITERATOR_DEBUG_LEVEL=0 for kolosal_server_exe to match MuPDF")
    
    message(STATUS "Added /FS, /bigobj, /MP1, and runtime library options to kolosal_server_exe target for MSVC")
endif()

# Function to copy DLLs to executable directory
function(copy_dll_to_output target_name dll_path)
    if(WIN32 AND EXISTS "${dll_path}")
        get_filename_component(dll_name "${dll_path}" NAME)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${dll_path}"
            "$<TARGET_FILE_DIR:${target_name}>/${dll_name}"
            COMMENT "Copying ${dll_name} to output directory"
        )
        message(STATUS "Will copy ${dll_name} to output directory")
    endif()
endfunction()

# Copy necessary DLLs to the executable directory on Windows
if(WIN32)
    # Copy the main server DLL
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:kolosal_server>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying kolosal_server.dll to executable directory"
    )
    
    # Note: Inference engine DLLs are now loaded at runtime and should be placed
    # in the plugins directory or same directory as the executable manually
    
    # Copy CURL DLL if it exists
    set(CURL_DLL_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/external/curl/bin/libcurl.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/curl/lib/libcurl.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/curl/bin/curl.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/curl/lib/curl.dll"
    )
    
    foreach(curl_dll ${CURL_DLL_PATHS})
        if(EXISTS "${curl_dll}")
            copy_dll_to_output(kolosal_server_exe "${curl_dll}")
            break()
        endif()
    endforeach()
    
    # Copy inference engine DLL (llama-cpu.dll) to executable directory
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:llama-cpu>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying llama-cpu.dll to executable directory"
    )
    
    # Copy yaml-cpp DLL if it's built as shared library
    if(BUILD_SHARED_LIBS)
        add_custom_command(TARGET kolosal_server_exe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:yaml-cpp>"
            "$<TARGET_FILE_DIR:kolosal_server_exe>/"
            COMMENT "Copying yaml-cpp DLL to executable directory"
        )
    endif()
    
    # Copy any additional system DLLs that might be needed
    # Check for common MSVC runtime libraries
    if(MSVC)
        # These will be automatically handled by CMake's runtime library settings
        message(STATUS "MSVC detected - runtime libraries will be statically linked")
    endif()
endif()

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

# Configure yaml-cpp runtime library to match our project
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(YAML_CPP_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "yaml-cpp runtime library" FORCE)
    else()
        set(YAML_CPP_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "yaml-cpp runtime library" FORCE)
    endif()
endif()

# Add yaml-cpp subdirectory
add_subdirectory(${YAML_CPP_PATH})

# Configure yaml-cpp runtime library after it's been added
if(MSVC AND TARGET yaml-cpp)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(yaml-cpp PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL"
        )
        target_compile_options(yaml-cpp PRIVATE
            $<$<CONFIG:Debug>:/MDd>
        )
        message(STATUS "Configured yaml-cpp to use Debug runtime (/MDd)")
    else()
        set_target_properties(yaml-cpp PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
        )
        target_compile_options(yaml-cpp PRIVATE
            $<$<CONFIG:Release>:/MD>
            $<$<CONFIG:RelWithDebInfo>:/MD>
            $<$<CONFIG:MinSizeRel>:/MD>
        )
        message(STATUS "Configured yaml-cpp to use Release runtime (/MD)")
    endif()
endif()

# Note: yaml-cpp is still needed for server configuration
if(NOT TARGET yaml-cpp)
    add_subdirectory(external/yaml-cpp EXCLUDE_FROM_ALL)
    # Ensure yaml-cpp is built with -fPIC if needed
    if(CMAKE_POSITION_INDEPENDENT_CODE)
        set_target_properties(yaml-cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
        message(STATUS "Applied -fPIC to yaml-cpp static library")
    endif()
    
    # Configure runtime library for fallback yaml-cpp as well
    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set_target_properties(yaml-cpp PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL"
            )
            target_compile_options(yaml-cpp PRIVATE
                $<$<CONFIG:Debug>:/MDd>
            )
        else()
            set_target_properties(yaml-cpp PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
            )
            target_compile_options(yaml-cpp PRIVATE
                $<$<CONFIG:Release>:/MD>
                $<$<CONFIG:RelWithDebInfo>:/MD>
                $<$<CONFIG:MinSizeRel>:/MD>
            )
        endif()
    endif()
endif()

# Link the main server library with yaml-cpp only
target_link_libraries(kolosal_server PUBLIC yaml-cpp)

# Add inference subdirectory and link
add_subdirectory(inference)
# Link with the inference library
target_link_libraries(kolosal_server PRIVATE ${INFERENCE_TARGET_NAME})

# Configure MuPDF library linking
set(MUPDF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf")
if(WIN32)
    # Try different paths for MuPDF libraries
    set(MUPDF_LIB_PATHS 
        "${MUPDF_PATH}/platform/win32/x64/Release"
        "${MUPDF_PATH}/platform/win32/x64/Debug"
        "${MUPDF_PATH}/platform/win32/Release"
        "${MUPDF_PATH}/platform/win32/Debug"
        "${MUPDF_PATH}/build/x64/Release"
        "${MUPDF_PATH}/build/x64/Debug"
        "${MUPDF_PATH}/lib"
    )
    
    set(MUPDF_LIB_FOUND FALSE)
    foreach(MUPDF_LIB_PATH ${MUPDF_LIB_PATHS})
        if(EXISTS "${MUPDF_LIB_PATH}/libmupdf.lib")
            target_link_libraries(kolosal_server PRIVATE 
                "${MUPDF_LIB_PATH}/libmupdf.lib"
            )
            # Try to find libthirdparty.lib in the same directory
            if(EXISTS "${MUPDF_LIB_PATH}/libthirdparty.lib")
                target_link_libraries(kolosal_server PRIVATE 
                    "${MUPDF_LIB_PATH}/libthirdparty.lib"
                )
            endif()
            message(STATUS "Linking with MuPDF libraries from ${MUPDF_LIB_PATH}")
            set(MUPDF_LIB_FOUND TRUE)
            break()
        endif()
    endforeach()
    
    if(NOT MUPDF_LIB_FOUND)
        message(WARNING "MuPDF libraries not found in any of the searched paths")
        message(STATUS "Searched paths: ${MUPDF_LIB_PATHS}")
        message(STATUS "PDF parsing features will not be available")
    endif()
else()
    # For Linux, we might need to build MuPDF or use system packages
    message(STATUS "MuPDF configuration for Linux not yet implemented")
endif()

# Configure CURL for main server (download utilities need it)
set(CURL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/curl")
if(EXISTS "${CURL_PATH}/include/curl/curl.h")
    message(STATUS "Found CURL: ${CURL_PATH}/include")
    target_include_directories(kolosal_server PRIVATE "${CURL_PATH}/include")
    
    # Find CURL library
    if(WIN32)
        find_library(CURL_LIBRARY 
            NAMES curl libcurl curl_a libcurl_a
            PATHS "${CURL_PATH}/lib" 
            PATH_SUFFIXES Release Debug x64/Release x64/Debug
            NO_DEFAULT_PATH
        )
        if(CURL_LIBRARY)
            target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARY})
            message(STATUS "Linking kolosal_server with CURL library: ${CURL_LIBRARY}")
        else()
            message(WARNING "CURL library not found in ${CURL_PATH}/lib")
        endif()
    else()
        # On Linux, use system CURL or built version
        find_package(CURL QUIET)
        if(CURL_FOUND)
            target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARIES})
            target_include_directories(kolosal_server PRIVATE ${CURL_INCLUDE_DIRS})
            message(STATUS "Using system CURL: ${CURL_LIBRARIES}")
        else()
            # Try to find in external directory
            find_library(CURL_LIBRARY 
                NAMES curl libcurl
                PATHS "${CURL_PATH}/lib"
                NO_DEFAULT_PATH
            )
            if(CURL_LIBRARY)
                target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARY})
                message(STATUS "Using external CURL: ${CURL_LIBRARY}")
            else()
                message(WARNING "CURL not found. Download utilities may not work.")
            endif()
        endif()
    endif()
else()
    message(WARNING "CURL headers not found at ${CURL_PATH}/include/curl/curl.h")
    message(STATUS "Download utilities will be disabled")
endif()

# Find and link thread library for the main library.
find_package(Threads REQUIRED)
target_link_libraries(kolosal_server PRIVATE Threads::Threads)

# Platform-specific link libraries for the main library.
if(WIN32)
    target_link_libraries(kolosal_server PRIVATE ws2_32)
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

# Add MSVC-specific compiler flags to fix PDB file access issues
if(MSVC)
    # Add /FS flag to fix "cannot open program database" errors during parallel compilation
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FS /EHsc /bigobj")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /FS /bigobj")
    # Completely disable multiprocessor compilation to prevent PDB conflicts
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP1")
    message(STATUS "Added /FS, /EHsc, /bigobj, and /MP1 flags for MSVC to handle parallel compilation PDB access")
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
    
    # Set RPATH for proper library loading when installed and during development
    set_target_properties(kolosal_server_exe PROPERTIES
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib:/usr/lib"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
    
    set_target_properties(kolosal_server PROPERTIES
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib:/usr/lib"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
    
endif()

# Install header files only for development packages
option(INSTALL_HEADERS "Install header files for development" OFF)
if(INSTALL_HEADERS)
    install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()