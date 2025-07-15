cmake_minimum_required(VERSION 3.14)
project(KolosalServer VERSION 1.0.0 LANGUAGES CXX)

# Include UCM for runtime library management
include(${CMAKE_SOURCE_DIR}/cmake/ucm.cmake)

# Static link the runtime libraries
ucm_set_runtime(DYNAMIC)

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

if (WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(NOMINMAX)
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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/pugixml/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf/thirdparty/zlib/contrib/minizip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf/thirdparty/zlib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/minizip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/maddy/include)

# Define source files.
set(SOURCES
    src/server.cpp
    src/server_api.cpp    
    src/server_config.cpp
    src/logger.cpp
    src/download_utils.cpp
    src/download_manager.cpp
    src/qdrant_client.cpp
    src/node_manager.cpp
    src/inference_loader.cpp
    src/inference_interface_impl.cpp
    src/gpu_detection.cpp
    src/image_utils.cpp

    # Route files
    src/routes/oai_completions_route.cpp
    src/routes/completion_route.cpp
    src/routes/models_route.cpp
    src/routes/engines_route.cpp
    src/routes/health_status_route.cpp    
    src/routes/auth_config_route.cpp    
    src/routes/server_logs_route.cpp    
    src/routes/embedding_route.cpp
    src/routes/internet_search_route.cpp
    src/routes/parse_docx_route.cpp
    src/routes/parse_pdf_route.cpp
    src/routes/parse_html_route.cpp
    src/routes/retrieve_route.cpp
    src/routes/add_documents_route.cpp
    src/routes/remove_documents_route.cpp
    src/routes/downloads_route.cpp
    
    # Retrieval files
    src/retrieval/add_document_types.cpp
    src/retrieval/remove_document_types.cpp
    src/retrieval/retrieve_types.cpp
    src/retrieval/document_service.cpp
    src/retrieval/parse_docx.cpp
    src/retrieval/parse_pdf.cpp
    src/retrieval/parse_html.cpp
    
    # Model files
    src/models/embedding_request_model.cpp
    src/models/embedding_response_model.cpp

    src/auth/rate_limiter.cpp
    src/auth/cors_handler.cpp
    src/auth/auth_middleware.cpp
    
    # src/completion_monitor.cpp
    
    external/pugixml/src/pugixml.cpp
    # Minizip sources for DOCX parsing
    external/mupdf/thirdparty/zlib/contrib/minizip/unzip.c
    external/mupdf/thirdparty/zlib/contrib/minizip/ioapi.c
    external/mupdf/thirdparty/zlib/contrib/minizip/iowin32.c
)

# Create shared library instead of static library.
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD INFERENCE_EXPORTS)

# Create executable
add_executable(kolosal_server_exe src/main.cpp)
target_link_libraries(kolosal_server_exe PRIVATE kolosal_server)
set_target_properties(kolosal_server_exe PROPERTIES OUTPUT_NAME "kolosal-server")

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

# Add yaml-cpp subdirectory
add_subdirectory(${YAML_CPP_PATH})

# Build the inference engine as a subdirectory
# This will create the llama-<cpu/cuda/vulkan> shared library
# All llama.cpp, CUDA, Vulkan, and MPI dependencies are handled by the inference library
add_subdirectory(inference)

# Configure MuPDF for the main server
set(MUPDF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf")
target_include_directories(kolosal_server PRIVATE 
    ${MUPDF_PATH}/include
    ${MUPDF_PATH}/thirdparty/zlib/contrib/minizip
    ${MUPDF_PATH}/thirdparty/zlib
)

# Force Release build to match MuPDF (prebuilt Release library)
if(WIN32 AND MSVC)
    # MuPDF is only available as Release build, so force Release mode
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
    message(STATUS "Forcing Release build to match MuPDF prebuilt library")
endif()

# Build MuPDF if it doesn't exist
if(WIN32)
    set(MUPDF_LIB_DIR "${MUPDF_PATH}/platform/win32/x64/Release")
    set(MUPDF_LIBS "${MUPDF_LIB_DIR}/libmupdf.lib" "${MUPDF_LIB_DIR}/libthirdparty.lib")
    
    # Check if MuPDF libraries exist, if not build them
    set(MUPDF_MISSING FALSE)
    foreach(lib ${MUPDF_LIBS})
        if(NOT EXISTS "${lib}")
            set(MUPDF_MISSING TRUE)
            break()
        endif()
    endforeach()
    
    if(MUPDF_MISSING)
        # Add custom target to build MuPDF using devenv (Visual Studio)
        # devenv can automatically retarget projects to the current VS version
        add_custom_target(mupdf_build
            COMMAND devenv mupdf.sln /upgrade
            COMMAND devenv mupdf.sln /build "Release|x64" /project libmupdf
            COMMAND devenv mupdf.sln /build "Release|x64" /project libthirdparty
            WORKING_DIRECTORY ${MUPDF_PATH}/platform/win32
            COMMENT "Building MuPDF libraries using devenv with auto-upgrade"
        )
        add_dependencies(kolosal_server mupdf_build)
    endif()
endif()

# Only add yaml-cpp if it hasn't been added by the inference subdirectory
# Note: yaml-cpp is still needed for server configuration
if(NOT TARGET yaml-cpp)
    add_subdirectory(external/yaml-cpp EXCLUDE_FROM_ALL)
    # Ensure yaml-cpp is built with -fPIC if needed
    if(CMAKE_POSITION_INDEPENDENT_CODE)
        set_target_properties(yaml-cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
        message(STATUS "Applied -fPIC to yaml-cpp static library")
    endif()
endif()

# Link the main server library with yaml-cpp only
target_link_libraries(kolosal_server PUBLIC yaml-cpp)

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
endif()

# Configure MuPDF for the main server
set(MUPDF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf")
target_include_directories(kolosal_server PRIVATE 
    ${MUPDF_PATH}/include
    ${MUPDF_PATH}/thirdparty/zlib/contrib/minizip
    ${MUPDF_PATH}/thirdparty/zlib
)

# Link MuPDF libraries to main server
if(WIN32)
    # For Windows, we'll need to build MuPDF or use prebuilt libraries
    # Point to the correct x64/Release directory where the libraries are built
    target_link_directories(kolosal_server PRIVATE 
        ${MUPDF_PATH}/platform/win32/x64/Release
    )
    target_link_libraries(kolosal_server PRIVATE 
        libmupdf libthirdparty
    )
else()
    # For Unix systems, link standard MuPDF libraries
    target_link_libraries(kolosal_server PRIVATE 
        mupdf mupdf-third
    )
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
# llama.cpp is handled by the inference subdirectory, not directly
# set(LLAMA_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp")
# add_subdirectory(${LLAMA_CPP_PATH})

# Only add yaml-cpp if it hasn't been added by llama.cpp
if(NOT TARGET yaml-cpp)
    add_subdirectory(external/yaml-cpp EXCLUDE_FROM_ALL)
endif()

# Add cppuprofile if GPU monitoring is enabled
if(USE_CPPUPROFILE_GPU)
    # Configure cppuprofile with NVIDIA support
    set(GPU_MONITOR_NVIDIA ON CACHE BOOL "Enable NVIDIA GPU monitoring in cppuprofile" FORCE)
    add_subdirectory(external/cppuprofile/lib)
    message(STATUS "cppuprofile GPU monitoring enabled")
endif()

# Link libraries - llama components are handled by inference library
set(KOLOSAL_LINK_LIBRARIES yaml-cpp llama-cpu)

# Add cppuprofile if enabled
if(USE_CPPUPROFILE_GPU)
    list(APPEND KOLOSAL_LINK_LIBRARIES cppuprofile)
endif()

target_link_libraries(kolosal_server PRIVATE ${KOLOSAL_LINK_LIBRARIES})

# Link CURL libraries and include directories
target_include_directories(kolosal_server PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARIES})

# MuPDF linking handled in main configuration section above

# llama.cpp include directories are handled by the inference library
# target_include_directories(kolosal_server PUBLIC
#   ${LLAMA_CPP_PATH}/include
#   ${LLAMA_CPP_PATH}/common
#   ${LLAMA_CPP_PATH}/ggml/include
# )

# Find and link thread library for the main library.
find_package(Threads REQUIRED)
target_link_libraries(kolosal_server PRIVATE Threads::Threads)

# Platform-specific link libraries for the main library.
if(WIN32)
    target_link_libraries(kolosal_server PRIVATE ws2_32 wbemuuid)
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

# Add acggressive optimization flags for better performance
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