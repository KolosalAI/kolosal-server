cmake_minimum_required(VERSION 3.14)

# Set policies for compatibility with newer CMake versions
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW) # Use <PackageName>_ROOT variables
endif()
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW) # option() honors normal variables
endif()
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # FetchContent uses DOWNLOAD_EXTRACT_TIMESTAMP
endif()

project(KolosalServer VERSION 1.0.0 LANGUAGES C CXX)

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
option(USE_METAL  "Compile with Metal support (macOS only)" OFF)
option(USE_MPI    "Compile with MPI support" OFF)
option(DEBUG      "Compile with debugging information" OFF)
option(USE_PODOFO "Compile with PoDoFo PDF support" ON)

# On Apple systems, enable Metal by default unless explicitly disabled
if(APPLE)
    message(STATUS "Apple system detected - enabling Metal acceleration by default")
    set(USE_METAL ON CACHE BOOL "Enable Metal acceleration on Apple systems" FORCE)
endif()

# Define include directories.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/cppuprofile/lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inference/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inference/external/llama.cpp/include")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inference/external/llama.cpp/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inference/external/llama.cpp/common)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inference/external/llama.cpp/ggml/include)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include")
    message(WARNING "Using legacy llama.cpp include path under kolosal-server/external/. Please migrate to inference/external/llama.cpp")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/common)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/ggml/include)
endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/pugixml/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/contrib/minizip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/maddy/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/zlib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/src)

# --- Ensure a usable 'freetype' CMake target exists (PoDoFo references it) ---
if(USE_PODOFO AND NOT TARGET freetype)
    # Prefer standard find_package (may provide target Freetype::Freetype)
    find_package(Freetype QUIET)

    # Collect library + include dirs
    set(_FT_INCLUDE_DIRS "${FREETYPE_INCLUDE_DIRS}")
    set(_FT_LIBS "${FREETYPE_LIBRARIES}")

    # If FREETYPE_LIBRARIES is a target-like token and not an existing path, try manual locate
    if(NOT _FT_LIBS)
        find_library(FREETYPE_LIBRARY NAMES freetype)
        if(FREETYPE_LIBRARY)
            set(_FT_LIBS ${FREETYPE_LIBRARY})
        endif()
    endif()

    if(NOT _FT_INCLUDE_DIRS)
        foreach(d "/opt/homebrew/include/freetype2" "/usr/local/include/freetype2" "/usr/include/freetype2")
            if(EXISTS "${d}/ft2build.h")
                set(_FT_INCLUDE_DIRS "${d}")
                break()
            endif()
        endforeach()
    endif()

    if(_FT_LIBS)
        # Create lightweight forwarding interface target; don't assume library path string is a real file if coming from an imported target.
        add_library(freetype INTERFACE IMPORTED GLOBAL)
        if(_FT_INCLUDE_DIRS)
            set_target_properties(freetype PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_FT_INCLUDE_DIRS}")
        endif()
        set_target_properties(freetype PROPERTIES INTERFACE_LINK_LIBRARIES "${_FT_LIBS}")
        message(STATUS "Global: Created INTERFACE freetype target forwarding: libs='${_FT_LIBS}' includes='${_FT_INCLUDE_DIRS}'")
    else()
        message(WARNING "Global: Failed to locate freetype library; PoDoFo may fail to link. Install freetype (brew install freetype or apt install libfreetype6-dev).")
    endif()
endif()

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

    # Route files
    src/routes/llm/oai_completions_route.cpp
    src/routes/llm/completion_route.cpp
    src/routes/models_route.cpp
    src/routes/engines_route.cpp
    src/routes/health_status_route.cpp    
    src/routes/config/auth_config_route.cpp    
    src/routes/server_logs_route.cpp    
    src/routes/retrieval/embedding_route.cpp
    src/routes/retrieval/internet_search_route.cpp
    src/routes/retrieval/parse_document_route.cpp
    src/routes/retrieval/documents_route.cpp
    src/routes/downloads_route.cpp
    src/routes/retrieval/chunking_route.cpp
    
    # Retrieval files
    src/retrieval/add_document_types.cpp
    src/retrieval/remove_document_types.cpp
    src/retrieval/document_list_types.cpp
    src/retrieval/retrieve_types.cpp
    src/retrieval/document_service.cpp
    src/retrieval/parse_docx.cpp
    src/retrieval/parse_pdf.cpp
    src/retrieval/parse_html.cpp
    src/retrieval/chunking_types.cpp
    
    # Model files
    src/models/embedding_request_model.cpp
    src/models/embedding_response_model.cpp
    src/models/chunking_request_model.cpp
    src/models/chunking_response_model.cpp

    src/auth/rate_limiter.cpp
    src/auth/cors_handler.cpp
    src/auth/auth_middleware.cpp
    
    # src/completion_monitor.cpp
)

# Create shared library instead of static library.
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD INFERENCE_EXPORTS)

# Set macOS-specific properties for the shared library
if(APPLE)
    set_target_properties(kolosal_server PROPERTIES
        MACOSX_RPATH TRUE
        INSTALL_NAME_DIR "@rpath"
    )
endif()

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

# Determine inference library target name based on acceleration options
# Allow GGML_CUDA (passed in from Docker build) to imply CUDA engine naming even if USE_CUDA not explicitly set.
if(USE_CUDA OR GGML_CUDA)
    set(INFERENCE_TARGET "llama-cuda")
    if(USE_CUDA AND GGML_CUDA)
        message(STATUS "Will build inference engine: ${INFERENCE_TARGET} (CUDA acceleration via USE_CUDA & GGML_CUDA)")
    elseif(GGML_CUDA)
        message(STATUS "Will build inference engine: ${INFERENCE_TARGET} (CUDA acceleration inferred from GGML_CUDA)")
    else()
        message(STATUS "Will build inference engine: ${INFERENCE_TARGET} (CUDA acceleration)")
    endif()
elseif(USE_VULKAN)
    set(INFERENCE_TARGET "llama-vulkan")
    message(STATUS "Will build inference engine: ${INFERENCE_TARGET} (Vulkan acceleration)")
elseif(USE_METAL)
    set(INFERENCE_TARGET "llama-metal")
    message(STATUS "Will build inference engine: ${INFERENCE_TARGET} (Metal acceleration)")
else()
    set(INFERENCE_TARGET "llama-cpu")
    message(STATUS "Will build inference engine: ${INFERENCE_TARGET} (CPU only)")
endif()

# Build the inference engine as a subdirectory
# This will create the llama-<cpu/cuda/vulkan/metal> shared library
# All llama.cpp, CUDA, Vulkan, Metal, and MPI dependencies are handled by the inference library
add_subdirectory(inference)

# Link the main server library with yaml-cpp
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

# Define inference-related compile definitions
target_compile_definitions(kolosal_server PUBLIC
  $<$<BOOL:${USE_CUDA}>:USE_CUDA>
  $<$<BOOL:${USE_VULKAN}>:USE_VULKAN>
  $<$<BOOL:${USE_MPI}>:USE_MPI>
  $<$<BOOL:${DEBUG}>:DEBUG>
  $<$<BOOL:${USE_CPPUPROFILE_GPU}>:USE_CPPUPROFILE_GPU>
  $<$<BOOL:${USE_PODOFO}>:USE_PODOFO>
)

# Add subdirectories for external libraries
# llama.cpp is handled by the inference subdirectory, not directly

# Add zlib (use system zlib on macOS/Linux, bundled on Windows)
if(NOT TARGET zlibstatic)
    # Try to use system zlib first on Unix systems
    if(UNIX)
        find_package(ZLIB QUIET)
        if(ZLIB_FOUND)
            message(STATUS "Using system ZLIB: ${ZLIB_LIBRARIES}")
            # Create an alias target for compatibility
            if(NOT TARGET zlibstatic)
                add_library(zlibstatic ALIAS ZLIB::ZLIB)
            endif()
            set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIRS})
            set(ZLIB_LIBRARY ${ZLIB_LIBRARIES})
        endif()
    endif()
    
    # Fallback to bundled zlib if system zlib not found or on Windows
    if(NOT ZLIB_FOUND OR WIN32)
        message(STATUS "Using bundled ZLIB")
        # Configure zlib options
        set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable zlib tests" FORCE)
        set(ZLIB_BUILD_MINIZIP OFF CACHE BOOL "Disable building libminizip contrib library" FORCE)
        set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Build zlib as static library" FORCE)
        set(ZLIB_BUILD_STATIC ON CACHE BOOL "Build zlib static library" FORCE)
        
        # Set ZLIB installation to OFF since we're building in-tree
        set(ZLIB_INSTALL OFF CACHE BOOL "Disable zlib installation" FORCE)
        
        add_subdirectory(external/zlib EXCLUDE_FROM_ALL)
        
        # Fix zlib compilation issue on macOS/Unix with newer compilers
        if(TARGET zlibstatic AND UNIX)
            target_compile_definitions(zlibstatic PRIVATE _POSIX_C_SOURCE=200809L)
        endif()
        
        # Set ZLIB_FOUND to TRUE so the ZLIB setup section below will run
        set(ZLIB_FOUND TRUE)
    endif()
endif()

# Add minizip manually
if(NOT TARGET minizip_static)
    set(MINIZIP_SOURCES
        external/zlib/contrib/minizip/ioapi.c
        external/zlib/contrib/minizip/mztools.c
        external/zlib/contrib/minizip/unzip.c
        external/zlib/contrib/minizip/zip.c
    )
    
    # Add Windows-specific source if on Windows
    if(WIN32)
        list(APPEND MINIZIP_SOURCES external/zlib/contrib/minizip/iowin32.c)
    endif()
    
    add_library(minizip_static STATIC ${MINIZIP_SOURCES})
    target_include_directories(minizip_static PUBLIC external/zlib/contrib/minizip external/zlib)
    
    # Link with appropriate zlib library
    if(TARGET zlibstatic)
        target_link_libraries(minizip_static PUBLIC zlibstatic)
    elseif(TARGET ZLIB::ZLIB)
        target_link_libraries(minizip_static PUBLIC ZLIB::ZLIB)
    endif()
    
    # Set compiler definitions for minizip
    target_compile_definitions(minizip_static PRIVATE
        $<$<BOOL:${WIN32}>:_CRT_SECURE_NO_WARNINGS>
        $<$<BOOL:${WIN32}>:_CRT_NONSTDC_NO_DEPRECATE>
    )
    
    set_target_properties(minizip_static PROPERTIES
        OUTPUT_NAME minizip
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# Set up ZLIB variables for other libraries (like PoDoFo) to find
if(ZLIB_FOUND)
    # Set ZLIB variables so find_package(ZLIB) works for dependencies
    
    # Set the include directory variables
    if(TARGET zlibstatic)
        message(STATUS "zlibstatic target found")
        get_target_property(ZLIB_INCLUDE_DIR zlibstatic INTERFACE_INCLUDE_DIRECTORIES)
        if(NOT ZLIB_INCLUDE_DIR)
            set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib;${CMAKE_CURRENT_BINARY_DIR}/external/zlib")
        endif()
        
        # Create the ZLIB::ZLIB alias immediately so PoDoFo can use it
        if(NOT TARGET ZLIB::ZLIB)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
            message(STATUS "Created ZLIB::ZLIB alias for zlibstatic")
        endif()
        
        # Set variables for PoDoFo to use the alias
        set(ZLIB_LIBRARY ZLIB::ZLIB CACHE STRING "ZLIB Library" FORCE)
        set(ZLIB_LIBRARIES ZLIB::ZLIB CACHE STRING "ZLIB Libraries" FORCE)
    else()
        message(STATUS "zlibstatic target not found")
        set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib;${CMAKE_CURRENT_BINARY_DIR}/external/zlib")
        set(ZLIB_LIBRARY "" CACHE STRING "ZLIB Library" FORCE)
        set(ZLIB_LIBRARIES "" CACHE STRING "ZLIB Libraries" FORCE)
    endif()
    
    set(ZLIB_VERSION_STRING "1.2.11") # Adjust if needed
    
    # Set the cache variables that find_package(ZLIB) expects  
    set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR} CACHE PATH "ZLIB include directory" FORCE)
    set(ZLIB_FOUND TRUE CACHE BOOL "ZLIB found" FORCE)
    
    message(STATUS "ZLIB variables set for PoDoFo: ${ZLIB_INCLUDE_DIR}")
endif()

# Add PoDoFo
if(USE_PODOFO AND NOT TARGET podofo_shared AND NOT TARGET podofo_static)
    message(STATUS "Configuring PoDoFo PDF support...")
    
    # Pre-configure OpenSSL variables for PoDoFo BEFORE any dependency checks
    # This ensures PoDoFo's find_package(OpenSSL) will find our bundled OpenSSL
    if(WIN32)
        set(PODOFO_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/extern/deps/3rdparty")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(PODOFO_LIB_SUBDIR "Win64")
        else()
            set(PODOFO_LIB_SUBDIR "Win32")
        endif()
        
        # Set OpenSSL paths for Windows
        set(OPENSSL_ROOT_DIR "${PODOFO_DEPS_DIR}/openssl" CACHE PATH "OpenSSL root directory" FORCE)
        set(OPENSSL_INCLUDE_DIR "${PODOFO_DEPS_DIR}/openssl/include" CACHE PATH "OpenSSL include directory" FORCE)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(OPENSSL_SSL_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/ssld.lib" CACHE FILEPATH "OpenSSL SSL library" FORCE)
            set(OPENSSL_CRYPTO_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/cryptod.lib" CACHE FILEPATH "OpenSSL crypto library" FORCE)
        else()
            set(OPENSSL_SSL_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/ssl.lib" CACHE FILEPATH "OpenSSL SSL library" FORCE)
            set(OPENSSL_CRYPTO_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/crypto.lib" CACHE FILEPATH "OpenSSL crypto library" FORCE)
        endif()
        set(OPENSSL_LIBRARIES "${OPENSSL_SSL_LIBRARY};${OPENSSL_CRYPTO_LIBRARY}" CACHE STRING "OpenSSL libraries" FORCE)
        set(OPENSSL_FOUND TRUE CACHE BOOL "OpenSSL found" FORCE)
        
        message(STATUS "Pre-configured OpenSSL for PoDoFo on Windows:")
        message(STATUS "  OPENSSL_ROOT_DIR = ${OPENSSL_ROOT_DIR}")
        message(STATUS "  OPENSSL_CRYPTO_LIBRARY = ${OPENSSL_CRYPTO_LIBRARY}")
        message(STATUS "  OPENSSL_SSL_LIBRARY = ${OPENSSL_SSL_LIBRARY}")
    endif()
    
    # Helpful message for developers
    if(APPLE)
        message(STATUS "For macOS developers: Install dependencies with 'brew install freetype jpeg libpng libtiff libxml2'")
    elseif(UNIX)
        message(STATUS "For Linux developers: Install dependencies with 'apt install libfreetype6-dev libjpeg-dev libpng-dev libtiff-dev libxml2-dev libfontconfig1-dev' (Ubuntu/Debian)")
        message(STATUS "Or 'yum install freetype-devel libjpeg-devel libpng-devel libtiff-devel libxml2-devel fontconfig-devel' (RHEL/CentOS/Fedora)")
    endif()
    
    # Check if PoDoFo has its own CMakeLists.txt
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/CMakeLists.txt")
        
        # First try to find system-installed dependencies (macOS/Linux with brew/apt)
        # This is the proper way for most developers who install dependencies via package managers
        if(UNIX)  # This includes both macOS and Linux
            message(STATUS "Looking for system-installed PoDoFo dependencies...")
            
            # Add common paths where homebrew and other package managers install libraries
            if(APPLE)
                # Homebrew paths for both Intel and Apple Silicon Macs
                list(APPEND CMAKE_PREFIX_PATH 
                    "/opt/homebrew"           # Apple Silicon
                    "/usr/local"              # Intel Mac
                    "/opt/local"              # MacPorts
                )
                # Also add library-specific paths
                list(APPEND CMAKE_LIBRARY_PATH
                    "/opt/homebrew/lib"
                    "/usr/local/lib"
                    "/opt/local/lib"
                )
                list(APPEND CMAKE_INCLUDE_PATH
                    "/opt/homebrew/include"
                    "/usr/local/include"
                    "/opt/local/include"
                )
                
                # Set PKG_CONFIG_PATH for pkg-config to find packages
                set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/lib/pkgconfig:/usr/local/lib/pkgconfig:/opt/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
            endif()
            
            # Find Freetype using pkg-config first, then fallback to find_package
            find_package(PkgConfig QUIET)
            if(PkgConfig_FOUND)
                pkg_check_modules(PC_FREETYPE QUIET freetype2)
                if(PC_FREETYPE_FOUND)
                    set(FREETYPE_INCLUDE_DIRS ${PC_FREETYPE_INCLUDE_DIRS})
                    set(FREETYPE_LIBRARIES ${PC_FREETYPE_LIBRARIES})
                    set(FREETYPE_FOUND TRUE)
                    message(STATUS "Found Freetype via pkg-config: ${FREETYPE_LIBRARIES}")
                endif()
            endif()
            
            if(NOT FREETYPE_FOUND)
                find_package(Freetype QUIET)
                if(FREETYPE_FOUND)
                    message(STATUS "Found system Freetype: ${FREETYPE_LIBRARIES}")
                    # Ensure we have the right variables set
                    if(NOT FREETYPE_INCLUDE_DIRS AND FREETYPE_INCLUDE_DIR)
                        set(FREETYPE_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIR})
                    endif()
                    if(NOT FREETYPE_LIBRARIES AND FREETYPE_LIBRARY)
                        set(FREETYPE_LIBRARIES ${FREETYPE_LIBRARY})
                    endif()
                endif()
            endif()

            # If PoDoFo is enabled we must have freetype; escalate search more aggressively
            if(USE_PODOFO AND NOT FREETYPE_FOUND)
                message(STATUS "Freetype still not found after initial attempts; retrying REQUIRED find_package")
                find_package(Freetype REQUIRED)
                if(FREETYPE_FOUND)
                    if(NOT FREETYPE_INCLUDE_DIRS AND FREETYPE_INCLUDE_DIR)
                        set(FREETYPE_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIR})
                    endif()
                    if(NOT FREETYPE_LIBRARIES AND FREETYPE_LIBRARY)
                        set(FREETYPE_LIBRARIES ${FREETYPE_LIBRARY})
                    endif()
                    message(STATUS "Successfully located Freetype (REQUIRED): ${FREETYPE_LIBRARIES}")
                else()
                    message(FATAL_ERROR "PoDoFo requires Freetype but it could not be found. Install libfreetype6-dev (Ubuntu/Debian) or freetype-devel (RHEL/CentOS/Fedora).")
                endif()
            endif()

            # Always create a 'freetype' alias to Freetype::Freetype if the module provided it
            if(FREETYPE_FOUND AND TARGET Freetype::Freetype AND NOT TARGET freetype)
                add_library(freetype ALIAS Freetype::Freetype)
                message(STATUS "Created alias target 'freetype' -> Freetype::Freetype")
            endif()
            
            if(FREETYPE_FOUND AND NOT TARGET Freetype::Freetype)
                add_library(Freetype::Freetype UNKNOWN IMPORTED)
                set_target_properties(Freetype::Freetype PROPERTIES
                    IMPORTED_LOCATION "${FREETYPE_LIBRARIES}"
                    INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
                )
                
                # Also create a simple alias target for compatibility
                if(NOT TARGET freetype)
                    add_library(freetype ALIAS Freetype::Freetype)
                endif()
                
                # Set the standard CMake Freetype variables for PoDoFo's find_package to detect
                set(Freetype_FOUND TRUE CACHE BOOL "Freetype found" FORCE)
                set(FREETYPE_FOUND TRUE CACHE BOOL "Freetype found" FORCE)
                set(FREETYPE_LIBRARY "${FREETYPE_LIBRARIES}" CACHE STRING "Freetype library" FORCE)
                set(FREETYPE_INCLUDE_DIR "${FREETYPE_INCLUDE_DIRS}" CACHE STRING "Freetype include dir" FORCE)
            endif()
            
            # Find JPEG
            find_package(JPEG QUIET)
            if(JPEG_FOUND)
                message(STATUS "Found system JPEG: ${JPEG_LIBRARIES}")
                if(NOT TARGET JPEG::JPEG)
                    add_library(JPEG::JPEG UNKNOWN IMPORTED)
                    set_target_properties(JPEG::JPEG PROPERTIES
                        IMPORTED_LOCATION "${JPEG_LIBRARIES}"
                        INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIR}"
                    )
                endif()
            endif()
            
            # Find PNG using pkg-config first
            if(PkgConfig_FOUND)
                pkg_check_modules(PC_PNG QUIET libpng)
                if(PC_PNG_FOUND)
                    set(PNG_FOUND TRUE)
                    set(PNG_LIBRARIES ${PC_PNG_LIBRARIES})
                    set(PNG_INCLUDE_DIRS ${PC_PNG_INCLUDE_DIRS})
                    message(STATUS "Found PNG via pkg-config: ${PNG_LIBRARIES}")
                endif()
            endif()
            
            if(NOT PNG_FOUND)
                find_package(PNG QUIET)
                if(PNG_FOUND)
                    message(STATUS "Found system PNG: ${PNG_LIBRARIES}")
                    # PNG::PNG target is usually created by find_package(PNG)
                endif()
            endif()
            
            # Find TIFF
            find_package(TIFF QUIET)
            if(TIFF_FOUND)
                message(STATUS "Found system TIFF: ${TIFF_LIBRARIES}")
                if(NOT TARGET TIFF::TIFF)
                    add_library(TIFF::TIFF UNKNOWN IMPORTED)
                    set_target_properties(TIFF::TIFF PROPERTIES
                        IMPORTED_LOCATION "${TIFF_LIBRARIES}"
                        INTERFACE_INCLUDE_DIRECTORIES "${TIFF_INCLUDE_DIR}"
                    )
                endif()
            endif()
            
            # Find LibXml2 using pkg-config first
            if(PkgConfig_FOUND)
                pkg_check_modules(PC_LIBXML2 QUIET libxml-2.0)
                if(PC_LIBXML2_FOUND)
                    set(LIBXML2_FOUND TRUE)
                    set(LIBXML2_LIBRARIES ${PC_LIBXML2_LIBRARIES})
                    set(LIBXML2_INCLUDE_DIRS ${PC_LIBXML2_INCLUDE_DIRS})
                    message(STATUS "Found LibXml2 via pkg-config: ${LIBXML2_LIBRARIES}")
                    
                    # Get the actual library path for linking
                    if(PC_LIBXML2_LIBRARY_DIRS)
                        # Try to find the actual library file
                        find_library(XML2_LIBRARY_LOCATION
                            NAMES xml2 libxml2
                            PATHS ${PC_LIBXML2_LIBRARY_DIRS}
                            NO_DEFAULT_PATH
                        )
                        message(STATUS "DEBUG: XML2_LIBRARY_LOCATION found at: ${XML2_LIBRARY_LOCATION}")
                        # If we found a concrete library path, prefer it over the bare 'xml2' name
                        if(XML2_LIBRARY_LOCATION AND EXISTS "${XML2_LIBRARY_LOCATION}")
                            # Override pkg-config's short name with the full path so that later
                            # imported targets get a valid IMPORTED_LOCATION (prevent 'xml2' file lookup)
                            set(LIBXML2_LIBRARIES "${XML2_LIBRARY_LOCATION}" CACHE STRING "Resolved libxml2 library path" FORCE)
                            message(STATUS "DEBUG: Overriding LIBXML2_LIBRARIES with concrete path: ${LIBXML2_LIBRARIES}")
                        endif()
                    endif()
                    
                    # If not found via pkg-config paths, use system libxml2 on macOS
                    if(NOT XML2_LIBRARY_LOCATION AND APPLE)
                        set(XML2_LIBRARY_LOCATION "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib/libxml2.tbd")
                        message(STATUS "DEBUG: Using system libxml2: ${XML2_LIBRARY_LOCATION}")
                    endif()
                else()
                    message(STATUS "LibXml2 not found via pkg-config; will try find_package")
                endif()
            endif()
            
            if(NOT LIBXML2_FOUND)
                find_package(LibXml2 QUIET)
                if(LIBXML2_FOUND)
                    message(STATUS "Found system LibXml2: ${LIBXML2_LIBRARIES}")
                    # LibXml2::LibXml2 target is usually created by find_package(LibXml2)
                endif()
            endif()

            # Defensive: if the cached XML2 path points at a macOS SDK but we are on Linux, discard it
            if(NOT APPLE AND XML2_LIBRARY_LOCATION MATCHES "MacOSX\.sdk")
                message(WARNING "Discarding stale macOS XML2 library path cached in XML2_LIBRARY_LOCATION=${XML2_LIBRARY_LOCATION} on non-Apple platform. Forcing re-detection.")
                unset(XML2_LIBRARY_LOCATION CACHE)
            endif()

            # Provide a unified imported target if none exists and we have raw libs
            if(LIBXML2_FOUND AND NOT TARGET LibXml2::LibXml2)
                # Some distros only give us LIBXML2_LIBRARIES as a full path list
                add_library(LibXml2::LibXml2 UNKNOWN IMPORTED)
                # Take the first library for IMPORTED_LOCATION if multiple
                set(_KOLOSAL_XML2_IMPORT "${LIBXML2_LIBRARIES}")
                if(_KOLOSAL_XML2_IMPORT MATCHES ";")
                    list(GET _KOLOSAL_XML2_IMPORT 0 _KOLOSAL_XML2_IMPORT)
                endif()
                # If the value is a bare name like 'xml2' try to resolve an actual path now
                if(NOT IS_ABSOLUTE "${_KOLOSAL_XML2_IMPORT}" OR NOT EXISTS "${_KOLOSAL_XML2_IMPORT}")
                    if(XML2_LIBRARY_LOCATION AND EXISTS "${XML2_LIBRARY_LOCATION}")
                        set(_KOLOSAL_XML2_IMPORT "${XML2_LIBRARY_LOCATION}")
                    else()
                        # Last resort: try a standard search (without NO_DEFAULT_PATH so system dirs are searched)
                        find_library(_KOLOSAL_XML2_FALLBACK NAMES xml2 libxml2)
                        if(_KOLOSAL_XML2_FALLBACK)
                            set(_KOLOSAL_XML2_IMPORT "${_KOLOSAL_XML2_FALLBACK}")
                        endif()
                    endif()
                endif()
                set_target_properties(LibXml2::LibXml2 PROPERTIES
                    IMPORTED_LOCATION "${_KOLOSAL_XML2_IMPORT}"
                    INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIRS}"
                )
                message(STATUS "Created fallback imported target LibXml2::LibXml2 -> ${_KOLOSAL_XML2_IMPORT}")
            endif()
            
            # Find Fontconfig (primarily for Linux, on macOS it might not be needed)
            if(NOT APPLE)
                find_package(Fontconfig QUIET)
                if(Fontconfig_FOUND)
                    message(STATUS "Found system Fontconfig: ${Fontconfig_LIBRARIES}")
                    if(NOT TARGET Fontconfig::Fontconfig)
                        add_library(Fontconfig::Fontconfig UNKNOWN IMPORTED)
                        set_target_properties(Fontconfig::Fontconfig PROPERTIES
                            IMPORTED_LOCATION "${Fontconfig_LIBRARIES}"
                            INTERFACE_INCLUDE_DIRECTORIES "${Fontconfig_INCLUDE_DIRS}"
                        )
                    endif()
                endif()
            else()
                # On macOS, fontconfig might not be needed or available
                message(STATUS "Skipping Fontconfig on macOS (usually not required)")
                set(Fontconfig_FOUND FALSE)
            endif()
        endif()
        
        # Fallback to bundled dependencies if system dependencies are not found
        # This maintains compatibility with the original Windows-focused setup
        if(WIN32 OR NOT (FREETYPE_FOUND AND JPEG_FOUND AND PNG_FOUND AND TIFF_FOUND AND LIBXML2_FOUND))
            message(STATUS "Using bundled PoDoFo dependencies...")
            
            # Set up paths to PoDoFo dependencies
            set(PODOFO_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/extern/deps/3rdparty")
            
            # Determine platform-specific library directory
            if(WIN32)
                if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                    set(PODOFO_LIB_SUBDIR "Win64")
                else()
                    set(PODOFO_LIB_SUBDIR "Win32")
                endif()
            elseif(APPLE)
                # Support both Intel and ARM architectures
                if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
                    set(PODOFO_LIB_SUBDIR "macosx-arm64")
                else()
                    set(PODOFO_LIB_SUBDIR "macosx-x86_64")
                endif()
            else()
                # Support both x86_64 and ARM architectures on Linux
                if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
                    set(PODOFO_LIB_SUBDIR "linux-arm64")
                else()
                    set(PODOFO_LIB_SUBDIR "linux-x86_64")
                endif()
            endif()
            
            # Only set up bundled dependencies if system ones weren't found
            if(NOT FREETYPE_FOUND)
                # Set up Freetype
                set(FREETYPE_INCLUDE_DIRS "${PODOFO_DEPS_DIR}/freetype/include/freetype2;${PODOFO_DEPS_DIR}/freetype/include")
                set(FREETYPE_INCLUDE_DIR "${PODOFO_DEPS_DIR}/freetype/include/freetype2")
                if(WIN32)
                    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                        set(FREETYPE_LIBRARY "${PODOFO_DEPS_DIR}/freetype/lib/${PODOFO_LIB_SUBDIR}/freetyped.lib")
                    else()
                        set(FREETYPE_LIBRARY "${PODOFO_DEPS_DIR}/freetype/lib/${PODOFO_LIB_SUBDIR}/freetype.lib")
                    endif()
                else()
                    set(FREETYPE_LIBRARY "${PODOFO_DEPS_DIR}/freetype/lib/${PODOFO_LIB_SUBDIR}/libfreetype.a")
                endif()
                set(FREETYPE_LIBRARIES ${FREETYPE_LIBRARY})
                
                # Create an imported target for Freetype so PoDoFo can find it properly
                if(NOT TARGET Freetype::Freetype AND EXISTS "${FREETYPE_LIBRARY}")
                    add_library(Freetype::Freetype STATIC IMPORTED)
                    set_target_properties(Freetype::Freetype PROPERTIES
                        IMPORTED_LOCATION ${FREETYPE_LIBRARY}
                        INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
                    )
                    set(FREETYPE_FOUND TRUE)
                    
                    # Also create a simple alias target for compatibility
                    if(NOT TARGET freetype)
                        add_library(freetype ALIAS Freetype::Freetype)
                    endif()
                    
                    # Set the standard CMake Freetype variables for PoDoFo's find_package to detect
                    set(Freetype_FOUND TRUE CACHE BOOL "Freetype found" FORCE)
                    set(FREETYPE_FOUND TRUE CACHE BOOL "Freetype found" FORCE)
                    set(FREETYPE_LIBRARY "${FREETYPE_LIBRARY}" CACHE STRING "Freetype library" FORCE)
                    set(FREETYPE_INCLUDE_DIR "${FREETYPE_INCLUDE_DIRS}" CACHE STRING "Freetype include dir" FORCE)
                endif()
            endif()
            
            if(NOT JPEG_FOUND)
                # Set up JPEG
                set(JPEG_INCLUDE_DIR "${PODOFO_DEPS_DIR}/libjpeg/include")
                if(WIN32)
                    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                        set(JPEG_LIBRARY "${PODOFO_DEPS_DIR}/libjpeg/lib/${PODOFO_LIB_SUBDIR}/jpegd.lib")
                    else()
                        set(JPEG_LIBRARY "${PODOFO_DEPS_DIR}/libjpeg/lib/${PODOFO_LIB_SUBDIR}/jpeg.lib")
                    endif()
                else()
                    set(JPEG_LIBRARY "${PODOFO_DEPS_DIR}/libjpeg/lib/${PODOFO_LIB_SUBDIR}/libjpeg.a")
                endif()
                set(JPEG_LIBRARIES ${JPEG_LIBRARY})
                
                # Create an imported target for JPEG
                if(NOT TARGET JPEG::JPEG AND EXISTS "${JPEG_LIBRARY}")
                    add_library(JPEG::JPEG STATIC IMPORTED)
                    set_target_properties(JPEG::JPEG PROPERTIES
                        IMPORTED_LOCATION ${JPEG_LIBRARY}
                        INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIR}"
                    )
                    set(JPEG_FOUND TRUE)
                endif()
            endif()
            
            if(NOT PNG_FOUND)
                # Set up PNG
                set(PNG_PNG_INCLUDE_DIR "${PODOFO_DEPS_DIR}/libpng/include")
                set(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR})
                if(WIN32)
                    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                        set(PNG_LIBRARY "${PODOFO_DEPS_DIR}/libpng/lib/${PODOFO_LIB_SUBDIR}/pngd.lib")
                    else()
                        set(PNG_LIBRARY "${PODOFO_DEPS_DIR}/libpng/lib/${PODOFO_LIB_SUBDIR}/png.lib")
                    endif()
                else()
                    set(PNG_LIBRARY "${PODOFO_DEPS_DIR}/libpng/lib/${PODOFO_LIB_SUBDIR}/libpng.a")
                endif()
                set(PNG_LIBRARIES ${PNG_LIBRARY})
                
                # Create an imported target for PNG
                if(NOT TARGET PNG::PNG AND EXISTS "${PNG_LIBRARY}")
                    add_library(PNG::PNG STATIC IMPORTED)
                    set_target_properties(PNG::PNG PROPERTIES
                        IMPORTED_LOCATION ${PNG_LIBRARY}
                        INTERFACE_INCLUDE_DIRECTORIES "${PNG_INCLUDE_DIR}"
                    )
                    set(PNG_FOUND TRUE)
                endif()
            endif()
            
            if(NOT TIFF_FOUND)
                # Set up TIFF
                set(TIFF_INCLUDE_DIR "${PODOFO_DEPS_DIR}/libtiff/include")
                if(WIN32)
                    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                        set(TIFF_LIBRARY "${PODOFO_DEPS_DIR}/libtiff/lib/${PODOFO_LIB_SUBDIR}/tiffd.lib")
                    else()
                        set(TIFF_LIBRARY "${PODOFO_DEPS_DIR}/libtiff/lib/${PODOFO_LIB_SUBDIR}/tiff.lib")
                    endif()
                else()
                    set(TIFF_LIBRARY "${PODOFO_DEPS_DIR}/libtiff/lib/${PODOFO_LIB_SUBDIR}/libtiff.a")
                endif()
                set(TIFF_LIBRARIES ${TIFF_LIBRARY})
                
                # Create an imported target for TIFF
                if(NOT TARGET TIFF::TIFF AND EXISTS "${TIFF_LIBRARY}")
                    add_library(TIFF::TIFF STATIC IMPORTED)
                    set_target_properties(TIFF::TIFF PROPERTIES
                        IMPORTED_LOCATION ${TIFF_LIBRARY}
                        INTERFACE_INCLUDE_DIRECTORIES "${TIFF_INCLUDE_DIR}"
                    )
                    set(TIFF_FOUND TRUE)
                endif()
            endif()
            
            if(NOT LIBXML2_FOUND)
                # Set up LibXml2
                set(LIBXML2_INCLUDE_DIR "${PODOFO_DEPS_DIR}/libxml2/include")
                set(LIBXML2_INCLUDE_DIRS ${LIBXML2_INCLUDE_DIR})
                if(WIN32)
                    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                        set(LIBXML2_LIBRARY "${PODOFO_DEPS_DIR}/libxml2/lib/${PODOFO_LIB_SUBDIR}/libxml2d.lib")
                    else()
                        set(LIBXML2_LIBRARY "${PODOFO_DEPS_DIR}/libxml2/lib/${PODOFO_LIB_SUBDIR}/libxml2.lib")
                    endif()
                else()
                    set(LIBXML2_LIBRARY "${PODOFO_DEPS_DIR}/libxml2/lib/${PODOFO_LIB_SUBDIR}/libxml2.a")
                endif()
                set(LIBXML2_LIBRARIES ${LIBXML2_LIBRARY})
                
                # Create an imported target for LibXml2
                if(NOT TARGET LibXml2::LibXml2 AND EXISTS "${LIBXML2_LIBRARY}")
                    add_library(LibXml2::LibXml2 STATIC IMPORTED)
                    set_target_properties(LibXml2::LibXml2 PROPERTIES
                        IMPORTED_LOCATION ${LIBXML2_LIBRARY}
                        INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIR}"
                    )
                    set(LIBXML2_FOUND TRUE)
                endif()
            endif()
            
            if(NOT Fontconfig_FOUND AND NOT APPLE)
                # Set up Fontconfig
                set(Fontconfig_INCLUDE_DIR "${PODOFO_DEPS_DIR}/fontconfig/include")
                set(Fontconfig_INCLUDE_DIRS ${Fontconfig_INCLUDE_DIR})
                if(WIN32)
                    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                        set(Fontconfig_LIBRARY "${PODOFO_DEPS_DIR}/fontconfig/lib/${PODOFO_LIB_SUBDIR}/fontconfigd.lib")
                    else()
                        set(Fontconfig_LIBRARY "${PODOFO_DEPS_DIR}/fontconfig/lib/${PODOFO_LIB_SUBDIR}/fontconfig.lib")
                    endif()
                else()
                    set(Fontconfig_LIBRARY "${PODOFO_DEPS_DIR}/fontconfig/lib/${PODOFO_LIB_SUBDIR}/libfontconfig.a")
                endif()
                set(Fontconfig_LIBRARIES ${Fontconfig_LIBRARY})
                
                # Create an imported target for Fontconfig
                if(NOT TARGET Fontconfig::Fontconfig AND EXISTS "${Fontconfig_LIBRARY}")
                    add_library(Fontconfig::Fontconfig STATIC IMPORTED)
                    set_target_properties(Fontconfig::Fontconfig PROPERTIES
                        IMPORTED_LOCATION ${Fontconfig_LIBRARY}
                        INTERFACE_INCLUDE_DIRECTORIES "${Fontconfig_INCLUDE_DIR}"
                    )
                    set(Fontconfig_FOUND TRUE)
                endif()
            endif()
        endif()
        
        # Log what was found
        if(FREETYPE_FOUND)
            message(STATUS "PoDoFo dependency - Freetype: FOUND")
        else()
            message(WARNING "PoDoFo dependency - Freetype: NOT FOUND")
        endif()
        
        if(JPEG_FOUND)
            message(STATUS "PoDoFo dependency - JPEG: FOUND")
        else()
            message(WARNING "PoDoFo dependency - JPEG: NOT FOUND")
        endif()
        
        if(PNG_FOUND)
            message(STATUS "PoDoFo dependency - PNG: FOUND")
        else()
            message(WARNING "PoDoFo dependency - PNG: NOT FOUND")
        endif()
        
        if(TIFF_FOUND)
            message(STATUS "PoDoFo dependency - TIFF: FOUND")
        else()
            message(WARNING "PoDoFo dependency - TIFF: NOT FOUND")
        endif()
        
        if(LIBXML2_FOUND)
            message(STATUS "PoDoFo dependency - LibXml2: FOUND")
        else()
            message(WARNING "PoDoFo dependency - LibXml2: NOT FOUND")
        endif()
        
        if(Fontconfig_FOUND)
            message(STATUS "PoDoFo dependency - Fontconfig: FOUND")
        elseif(NOT APPLE)
            message(WARNING "PoDoFo dependency - Fontconfig: NOT FOUND")
        else()
            message(STATUS "PoDoFo dependency - Fontconfig: SKIPPED (macOS)")
        endif()
        
        if(OPENSSL_FOUND)
            message(STATUS "PoDoFo dependency - OpenSSL: FOUND")
        else()
            message(WARNING "PoDoFo dependency - OpenSSL: NOT FOUND")
        endif()
        
        # Create or fix the global freetype target that PoDoFo will need for linking
        # This must be done before PoDoFo's subdirectory is processed
        message(STATUS "DEBUG: FREETYPE_FOUND=${FREETYPE_FOUND}")
        # Determine a usable freetype library path for imported/alias targets (avoid macOS-specific path on Linux)
        set(_KOLOSAL_FT_LIB "")
        if(APPLE)
            # Prefer Homebrew path (original behavior); could be refined to detect actual location
            set(_KOLOSAL_FT_LIB "/opt/homebrew/lib/libfreetype.dylib")
        elseif(FREETYPE_LIBRARIES)
            # Use first library from discovery
            list(GET FREETYPE_LIBRARIES 0 _KOLOSAL_FT_LIB)
        endif()
        if(TARGET freetype)
            message(STATUS "DEBUG: Target 'freetype' already exists")
            get_target_property(FREETYPE_TYPE freetype TYPE)
            get_target_property(FREETYPE_IMPORTED freetype IMPORTED)
            get_target_property(FREETYPE_ALIASED freetype ALIASED_TARGET)
            message(STATUS "DEBUG: freetype target type: ${FREETYPE_TYPE}")
            message(STATUS "DEBUG: freetype target imported: ${FREETYPE_IMPORTED}")
            message(STATUS "DEBUG: freetype aliased target: ${FREETYPE_ALIASED}")
            
            # If it's an alias, check and fix the properties of the actual target
            if(FREETYPE_ALIASED)
                message(STATUS "DEBUG: freetype is an ALIAS to ${FREETYPE_ALIASED}")
                if(TARGET ${FREETYPE_ALIASED})
                    get_target_property(REAL_FREETYPE_TYPE ${FREETYPE_ALIASED} TYPE)
                    get_target_property(REAL_FREETYPE_LOCATION ${FREETYPE_ALIASED} IMPORTED_LOCATION)
                    get_target_property(REAL_FREETYPE_INTERFACE_LINK ${FREETYPE_ALIASED} INTERFACE_LINK_LIBRARIES)
                    message(STATUS "DEBUG: Real freetype target type: ${REAL_FREETYPE_TYPE}")
                    message(STATUS "DEBUG: Real freetype location: ${REAL_FREETYPE_LOCATION}")
                    message(STATUS "DEBUG: Real freetype interface link: ${REAL_FREETYPE_INTERFACE_LINK}")
                    
                    # Fix the real target if it doesn't have proper library location
                    if(REAL_FREETYPE_LOCATION STREQUAL "freetype" OR NOT REAL_FREETYPE_LOCATION)
                        message(STATUS "DEBUG: Fixing ${FREETYPE_ALIASED} target with proper library path")
                        if(_KOLOSAL_FT_LIB)
                            set_target_properties(${FREETYPE_ALIASED} PROPERTIES
                                IMPORTED_LOCATION "${_KOLOSAL_FT_LIB}"
                                INTERFACE_LINK_LIBRARIES "${_KOLOSAL_FT_LIB}"
                                INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
                            )
                        endif()
                    endif()
                endif()
            else()
                # It's a regular target, try to update it if possible
                if(NOT FREETYPE_TYPE STREQUAL "ALIAS")
                    if(_KOLOSAL_FT_LIB)
                        set_target_properties(freetype PROPERTIES
                            INTERFACE_LINK_LIBRARIES "${_KOLOSAL_FT_LIB}"
                            INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
                            IMPORTED_LOCATION "${_KOLOSAL_FT_LIB}"
                        )
                    endif()
                endif()
            endif()
            message(STATUS "Handled existing freetype target for PoDoFo compatibility")
        else()
            message(STATUS "DEBUG: Target 'freetype' does not exist yet")
            if(_KOLOSAL_FT_LIB)
                add_library(freetype INTERFACE IMPORTED GLOBAL)
                set_target_properties(freetype PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${_KOLOSAL_FT_LIB}"
                    INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
                )
            else()
                message(WARNING "Could not determine freetype library path for imported target; relying on existing Freetype::Freetype if available")
            endif()
        endif()
        
        # Ensure libxml2 targets expected by PoDoFo exist
        message(STATUS "DEBUG: LIBXML2_FOUND=${LIBXML2_FOUND}")

        # If we found libxml2 but no imported target exists yet, create one using detected paths (avoid hardcoded macOS SDK)
        if(LIBXML2_FOUND AND NOT TARGET LibXml2::LibXml2)
            # Determine a concrete library file path for IMPORTED_LOCATION
            set(_KOLOSAL_XML2_IMPORT "${LIBXML2_LIBRARIES}")
            if(_KOLOSAL_XML2_IMPORT MATCHES ";")
                list(GET _KOLOSAL_XML2_IMPORT 0 _KOLOSAL_XML2_IMPORT)
            endif()
            # Resolve to a full path if it's still a bare name
            if(NOT IS_ABSOLUTE "${_KOLOSAL_XML2_IMPORT}" OR NOT EXISTS "${_KOLOSAL_XML2_IMPORT}")
                if(XML2_LIBRARY_LOCATION AND EXISTS "${XML2_LIBRARY_LOCATION}")
                    set(_KOLOSAL_XML2_IMPORT "${XML2_LIBRARY_LOCATION}")
                else()
                    find_library(_KOLOSAL_XML2_FALLBACK NAMES xml2 libxml2)
                    if(_KOLOSAL_XML2_FALLBACK)
                        set(_KOLOSAL_XML2_IMPORT "${_KOLOSAL_XML2_FALLBACK}")
                    endif()
                endif()
            endif()
            add_library(LibXml2::LibXml2 UNKNOWN IMPORTED GLOBAL)
            set_target_properties(LibXml2::LibXml2 PROPERTIES
                IMPORTED_LOCATION "${_KOLOSAL_XML2_IMPORT}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIRS}"
            )
            message(STATUS "DEBUG: Created LibXml2::LibXml2 imported library target at ${_KOLOSAL_XML2_IMPORT}")
        endif()

        # Some PoDoFo builds still reference a plain 'xml2' target; provide a lightweight bridge
        if(LIBXML2_FOUND AND NOT TARGET xml2)
            if(TARGET LibXml2::LibXml2)
                add_library(xml2 INTERFACE IMPORTED GLOBAL)
                set_target_properties(xml2 PROPERTIES
                    INTERFACE_LINK_LIBRARIES LibXml2::LibXml2
                    INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIRS}"
                )
                message(STATUS "DEBUG: Created interface xml2 target forwarding to LibXml2::LibXml2")
            else()
                # Fallback if only raw LIBXML2_LIBRARIES exist
                set(_KOLOSAL_XML2_IMPORT "${LIBXML2_LIBRARIES}")
                if(_KOLOSAL_XML2_IMPORT MATCHES ";")
                    list(GET _KOLOSAL_XML2_IMPORT 0 _KOLOSAL_XML2_IMPORT)
                endif()
                if(NOT IS_ABSOLUTE "${_KOLOSAL_XML2_IMPORT}" OR NOT EXISTS "${_KOLOSAL_XML2_IMPORT}")
                    if(XML2_LIBRARY_LOCATION AND EXISTS "${XML2_LIBRARY_LOCATION}")
                        set(_KOLOSAL_XML2_IMPORT "${XML2_LIBRARY_LOCATION}")
                    else()
                        find_library(_KOLOSAL_XML2_FALLBACK NAMES xml2 libxml2)
                        if(_KOLOSAL_XML2_FALLBACK)
                            set(_KOLOSAL_XML2_IMPORT "${_KOLOSAL_XML2_FALLBACK}")
                        endif()
                    endif()
                endif()
                add_library(xml2 UNKNOWN IMPORTED GLOBAL)
                set_target_properties(xml2 PROPERTIES
                    IMPORTED_LOCATION "${_KOLOSAL_XML2_IMPORT}"
                    INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIRS}"
                )
                message(STATUS "DEBUG: Created imported xml2 target at ${_KOLOSAL_XML2_IMPORT}")
            endif()
        endif()
        
        # Check if we have enough dependencies to build PoDoFo
        set(REQUIRED_DEPS_FOUND TRUE)
        if(NOT FREETYPE_FOUND OR NOT JPEG_FOUND OR NOT PNG_FOUND OR NOT TIFF_FOUND OR NOT LIBXML2_FOUND OR NOT OPENSSL_FOUND)
            set(REQUIRED_DEPS_FOUND FALSE)
        endif()
        if(NOT APPLE AND NOT Fontconfig_FOUND)
            set(REQUIRED_DEPS_FOUND FALSE)
        endif()
        
        if(NOT REQUIRED_DEPS_FOUND)
            message(WARNING "Some PoDoFo dependencies are missing. PDF support might not work correctly.")
            if(APPLE)
                message(STATUS "To install missing dependencies on macOS, run:")
                message(STATUS "  brew install freetype jpeg libpng libtiff libxml2")
            elseif(UNIX)
                message(STATUS "To install missing dependencies on Ubuntu/Debian, run:")
                message(STATUS "  sudo apt install libfreetype6-dev libjpeg-dev libpng-dev libtiff-dev libxml2-dev libfontconfig1-dev")
                message(STATUS "To install missing dependencies on RHEL/CentOS/Fedora, run:")
                message(STATUS "  sudo yum install freetype-devel libjpeg-devel libpng-devel libtiff-devel libxml2-devel fontconfig-devel")
            endif()
        endif()
        
        # Configure PoDoFo options
        set(PODOFO_BUILD_STATIC ON CACHE BOOL "Build PoDoFo as static library" FORCE)
        set(PODOFO_BUILD_TEST OFF CACHE BOOL "Disable PoDoFo tests" FORCE)
        set(PODOFO_BUILD_EXAMPLES OFF CACHE BOOL "Disable PoDoFo examples" FORCE)
        set(PODOFO_BUILD_UNSUPPORTED_TOOLS OFF CACHE BOOL "Disable PoDoFo tools" FORCE)
        set(PODOFO_BUILD_LIB_ONLY ON CACHE BOOL "Build only PoDoFo library" FORCE)
        
        # Disable installation and exports to avoid target export issues
        # set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Skip install rules for PoDoFo" FORCE)
        set(PODOFO_BUILD_INSTALL OFF CACHE BOOL "Disable PoDoFo installation" FORCE)
        
        # Set OpenSSL variables as cache variables BEFORE adding PoDoFo subdirectory
        if(WIN32)
            set(PODOFO_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/extern/deps/3rdparty")
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(PODOFO_LIB_SUBDIR "Win64")
            else()
                set(PODOFO_LIB_SUBDIR "Win32")
            endif()
            
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(OPENSSL_SSL_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/ssld.lib" CACHE STRING "OpenSSL SSL library" FORCE)
                set(OPENSSL_CRYPTO_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/cryptod.lib" CACHE STRING "OpenSSL crypto library" FORCE)
            else()
                set(OPENSSL_SSL_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/ssl.lib" CACHE STRING "OpenSSL SSL library" FORCE)
                set(OPENSSL_CRYPTO_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/crypto.lib" CACHE STRING "OpenSSL crypto library" FORCE)
            endif()
            set(OPENSSL_ROOT_DIR "${PODOFO_DEPS_DIR}/openssl" CACHE PATH "OpenSSL root directory" FORCE)
            set(OPENSSL_INCLUDE_DIR "${PODOFO_DEPS_DIR}/openssl/include" CACHE PATH "OpenSSL include directory" FORCE)
            set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} CACHE STRING "OpenSSL libraries" FORCE)
            
            message(STATUS "Pre-setting OpenSSL for PoDoFo:")
            message(STATUS "  OPENSSL_ROOT_DIR = ${OPENSSL_ROOT_DIR}")
            message(STATUS "  OPENSSL_CRYPTO_LIBRARY = ${OPENSSL_CRYPTO_LIBRARY}")
            message(STATUS "  OPENSSL_SSL_LIBRARY = ${OPENSSL_SSL_LIBRARY}")
        endif()
        
        # Pre-configure Freetype for PoDoFo (prevent it from running its own find_package)
        if(FREETYPE_FOUND)
            # Set all the freetype variables that PoDoFo expects
            set(Freetype_FOUND TRUE CACHE BOOL "Freetype found" FORCE)
            set(FREETYPE_FOUND TRUE CACHE BOOL "Freetype found" FORCE)
            set(FREETYPE_LIBRARY "${FREETYPE_LIBRARIES}" CACHE STRING "Freetype library" FORCE)
            set(FREETYPE_LIBRARIES "${FREETYPE_LIBRARIES}" CACHE STRING "Freetype libraries" FORCE)
            set(FREETYPE_INCLUDE_DIR "${FREETYPE_INCLUDE_DIRS}" CACHE STRING "Freetype include dir" FORCE)
            set(FREETYPE_INCLUDE_DIRS "${FREETYPE_INCLUDE_DIRS}" CACHE STRING "Freetype include dirs" FORCE)
        endif()
        
        # Create imported OpenSSL targets that PoDoFo expects
        # This bypasses the find_package(OpenSSL) call by providing the targets directly
        if(NOT TARGET OpenSSL::SSL AND NOT TARGET OpenSSL::Crypto)
            if(WIN32)
                # Windows: Use bundled OpenSSL from PoDoFo dependencies
                set(PODOFO_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/extern/deps/3rdparty")
                if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                    set(PODOFO_LIB_SUBDIR "Win64")
                else()
                    set(PODOFO_LIB_SUBDIR "Win32")
                endif()
                
                set(OPENSSL_INCLUDE_DIR "${PODOFO_DEPS_DIR}/openssl/include")
                if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                    set(OPENSSL_SSL_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/ssld.lib")
                    set(OPENSSL_CRYPTO_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/cryptod.lib")
                else()
                    set(OPENSSL_SSL_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/ssl.lib")
                    set(OPENSSL_CRYPTO_LIBRARY "${PODOFO_DEPS_DIR}/openssl/lib/${PODOFO_LIB_SUBDIR}/crypto.lib")
                endif()
                
                # Create the imported targets that PoDoFo expects
                add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
                set_target_properties(OpenSSL::SSL PROPERTIES
                    IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
                )
                
                add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
                set_target_properties(OpenSSL::Crypto PROPERTIES
                    IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
                )
                
                # Set the variables that PoDoFo might check
                set(OPENSSL_FOUND TRUE)
                set(OPENSSL_LIBRARIES "${OPENSSL_SSL_LIBRARY};${OPENSSL_CRYPTO_LIBRARY}")
                
                message(STATUS "Created OpenSSL imported targets for PoDoFo (Windows)")
            else()
                # macOS and Linux: Use system OpenSSL
                find_package(OpenSSL QUIET)
                if(OPENSSL_FOUND)
                    message(STATUS "Found system OpenSSL: ${OPENSSL_LIBRARIES}")
                    # OpenSSL::SSL and OpenSSL::Crypto targets should be created by find_package
                    if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
                        # Create targets manually if find_package didn't create them
                        if(NOT TARGET OpenSSL::SSL)
                            add_library(OpenSSL::SSL UNKNOWN IMPORTED GLOBAL)
                            set_target_properties(OpenSSL::SSL PROPERTIES
                                IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
                                INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
                            )
                        endif()
                        if(NOT TARGET OpenSSL::Crypto)
                            add_library(OpenSSL::Crypto UNKNOWN IMPORTED GLOBAL)
                            set_target_properties(OpenSSL::Crypto PROPERTIES
                                IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
                                INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
                            )
                        endif()
                    endif()
                    message(STATUS "Created OpenSSL imported targets for PoDoFo (Unix)")
                else()
                    message(WARNING "OpenSSL not found on system. PoDoFo may not link correctly.")
                    # Create dummy targets to avoid linking errors
                    add_library(OpenSSL::SSL INTERFACE IMPORTED GLOBAL)
                    add_library(OpenSSL::Crypto INTERFACE IMPORTED GLOBAL)
                endif()
            endif()
        endif()
        
        # Temporarily override add_custom_target to ignore uninstall target
        function(add_custom_target target_name)
            if(target_name STREQUAL "uninstall")
                message(STATUS "Skipping duplicate uninstall target from PoDoFo")
            else()
                # Call the original add_custom_target via cmake_language
                cmake_language(CALL _add_custom_target ${target_name} ${ARGN})
            endif()
        endfunction()
        
        # Use a simpler approach: completely prevent PoDoFo installation using a cache variable
        # This approach blocks all install commands from the PoDoFo subdirectory only
        
        # Save the current CMAKE_SKIP_INSTALL_RULES state
        set(SAVED_SKIP_INSTALL ${CMAKE_SKIP_INSTALL_RULES})
        
        # Set CMAKE_SKIP_INSTALL_RULES to TRUE for PoDoFo only
        set(CMAKE_SKIP_INSTALL_RULES TRUE)

        # Ensure a 'freetype' target exists before configuring PoDoFo (PoDoFo expects target name freetype when using system libs)
        if(NOT TARGET freetype)
            if(FREETYPE_LIBRARIES)
                # Choose first library if list
                set(_FT_LIB_IMPORT "${FREETYPE_LIBRARIES}")
                if(_FT_LIB_IMPORT MATCHES ";")
                    list(GET FREETYPE_LIBRARIES 0 _FT_LIB_IMPORT)
                endif()
                add_library(freetype UNKNOWN IMPORTED GLOBAL)
                set_target_properties(freetype PROPERTIES
                    IMPORTED_LOCATION "${_FT_LIB_IMPORT}"
                    INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
                )
                message(STATUS "Pre-PoDoFo: Created imported freetype target at ${_FT_LIB_IMPORT}")
            elseif(TARGET Freetype::Freetype)
                add_library(freetype ALIAS Freetype::Freetype)
                message(STATUS "Pre-PoDoFo: Added alias freetype -> Freetype::Freetype")
            else()
                message(WARNING "Pre-PoDoFo: FREETYPE_LIBRARIES not set and Freetype::Freetype target missing; PoDoFo may fail. Install libfreetype6-dev.")
            endif()
        endif()
        
        add_subdirectory(external/podofo EXCLUDE_FROM_ALL)
        
        # Restore the original CMAKE_SKIP_INSTALL_RULES state
        set(CMAKE_SKIP_INSTALL_RULES ${SAVED_SKIP_INSTALL})
        
        # Clear any leftover variables
        unset(SAVED_SKIP_INSTALL)
        unset(add_custom_target)
        
        # Set target properties for position independent code
        if(TARGET podofo_static)
            set_target_properties(podofo_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
        
        message(STATUS "PoDoFo library added")
    else()
        message(WARNING "PoDoFo CMakeLists.txt not found. PoDoFo support will be disabled")
    endif()
elseif(NOT USE_PODOFO)
    message(STATUS "PoDoFo support disabled")
endif()

# Install rules for kolosal_server targets (after PoDoFo processing)
# Only install on Apple for DMG packaging
if(APPLE)
    install(TARGETS kolosal_server
        LIBRARY DESTINATION "Kolosal.app/Contents/Frameworks"
        ARCHIVE DESTINATION "Kolosal.app/Contents/Frameworks"
        COMPONENT Runtime
    )
    
    install(TARGETS kolosal_server_exe
        RUNTIME DESTINATION "Kolosal.app/Contents/MacOS"
        COMPONENT Runtime
    )
    
    message(STATUS "Added install rules for kolosal_server targets")
endif()

# Fix any targets that PoDoFo may have created incorrectly
# Do this AFTER PoDoFo subdirectory processing to override any problematic targets
if(USE_PODOFO AND REQUIRED_DEPS_FOUND)
    message(STATUS "Post-PoDoFo: Fixing target properties for linking...")
    
    # Fix freetype target if it's an alias pointing to an incorrect target
    if(TARGET freetype)
        get_target_property(FREETYPE_ALIASED freetype ALIASED_TARGET)
        if(FREETYPE_ALIASED AND TARGET ${FREETYPE_ALIASED})
            get_target_property(REAL_FREETYPE_LOCATION ${FREETYPE_ALIASED} IMPORTED_LOCATION)
            if(REAL_FREETYPE_LOCATION STREQUAL "freetype" OR NOT REAL_FREETYPE_LOCATION)
                message(STATUS "Post-PoDoFo: Fixing ${FREETYPE_ALIASED} target with proper library path")
                if(APPLE)
                    set(_KOLOSAL_FT_LIB "/opt/homebrew/lib/libfreetype.dylib")
                elseif(FREETYPE_LIBRARIES)
                    list(GET FREETYPE_LIBRARIES 0 _KOLOSAL_FT_LIB)
                endif()
                if(_KOLOSAL_FT_LIB)
                    set_target_properties(${FREETYPE_ALIASED} PROPERTIES
                        IMPORTED_LOCATION "${_KOLOSAL_FT_LIB}"
                        INTERFACE_LINK_LIBRARIES "${_KOLOSAL_FT_LIB}"
                    )
                endif()
            endif()
        endif()
    endif()
    
    message(STATUS "Post-PoDoFo: Target fixes completed")
endif()

# Add PugiXML
if(NOT TARGET pugixml)
    # Check if PugiXML has its own CMakeLists.txt
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/pugixml/CMakeLists.txt")
        # Configure PugiXML options
        set(PUGIXML_BUILD_SHARED_LIBS OFF CACHE BOOL "Build PugiXML as static library" FORCE)
        set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "Disable PugiXML tests" FORCE)
        set(PUGIXML_BUILD_PKGCONFIG OFF CACHE BOOL "Disable PugiXML pkgconfig" FORCE)
        
        add_subdirectory(external/pugixml EXCLUDE_FROM_ALL)
    else()
        # Build manually if no CMakeLists.txt
        add_library(pugixml STATIC external/pugixml/src/pugixml.cpp)
        target_include_directories(pugixml PUBLIC external/pugixml/src)
        set_target_properties(pugixml PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

# Add cppuprofile if GPU monitoring is enabled
if(USE_CPPUPROFILE_GPU)
    # Configure cppuprofile with NVIDIA support
    set(GPU_MONITOR_NVIDIA ON CACHE BOOL "Enable NVIDIA GPU monitoring in cppuprofile" FORCE)
    add_subdirectory(external/cppuprofile/lib)
    message(STATUS "cppuprofile GPU monitoring enabled")
endif()

# Link libraries - use the determined inference target
set(KOLOSAL_LINK_LIBRARIES yaml-cpp ${INFERENCE_TARGET} minizip_static pugixml)

# Note: zlibstatic is already linked through minizip_static, so no need to link it directly

# Add PoDoFo if available and enabled
if(USE_PODOFO AND TARGET podofo_static)
    list(APPEND KOLOSAL_LINK_LIBRARIES podofo_static)
    # Add OpenSSL libraries for PoDoFo only if targets exist
    if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
        list(APPEND KOLOSAL_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
        message(STATUS "PoDoFo library will be linked with OpenSSL")
    else()
        message(WARNING "OpenSSL targets not found, PoDoFo will be linked without OpenSSL")
    endif()
    message(STATUS "PoDoFo library will be linked")
        # Link libxml2 via imported target or variable instead of hardcoded path
        if(LIBXML2_FOUND)
            if(TARGET LibXml2::LibXml2)
                list(APPEND KOLOSAL_LINK_LIBRARIES LibXml2::LibXml2)
                message(STATUS "Linking PoDoFo with LibXml2::LibXml2 target")
            elseif(LIBXML2_LIBRARIES)
                list(APPEND KOLOSAL_LINK_LIBRARIES ${LIBXML2_LIBRARIES})
                message(STATUS "Linking PoDoFo with raw LIBXML2_LIBRARIES: ${LIBXML2_LIBRARIES}")
            endif()
        else()
            message(WARNING "PoDoFo enabled but LibXml2 not detected; build may fail. Install libxml2-dev / libxml2-devel.")
        endif()
endif()

# Add cppuprofile if enabled
if(USE_CPPUPROFILE_GPU)
    list(APPEND KOLOSAL_LINK_LIBRARIES cppuprofile)
endif()

# Ensure a 'freetype' target exists if PoDoFo (or other components) expect it but only raw library paths were found
if(USE_PODOFO AND NOT TARGET freetype)
    if(FREETYPE_LIBRARIES)
        add_library(freetype UNKNOWN IMPORTED)
        # If multiple libraries listed, take first for IMPORTED_LOCATION
        set(_FT_LIB_IMPORT "${FREETYPE_LIBRARIES}")
        if(FREETYPE_LIBRARIES MATCHES ";")
            list(GET FREETYPE_LIBRARIES 0 _FT_LIB_IMPORT)
        endif()
        set_target_properties(freetype PROPERTIES
            IMPORTED_LOCATION "${_FT_LIB_IMPORT}"
            INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
        )
        message(STATUS "Created fallback imported 'freetype' target at ${_FT_LIB_IMPORT}")
    else()
        message(WARNING "PoDoFo enabled but no FREETYPE_LIBRARIES found and 'freetype' target missing")
    endif()
endif()

target_link_libraries(kolosal_server PRIVATE ${KOLOSAL_LINK_LIBRARIES})

# Link CURL libraries and include directories
if(CURL_LIBRARY)
    target_include_directories(kolosal_server PRIVATE ${CURL_PATH}/include)
    target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARY})
endif()

# Add PoDoFo include directories if enabled
if(USE_PODOFO AND TARGET podofo_static)
    target_include_directories(kolosal_server PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/src
        ${CMAKE_CURRENT_BINARY_DIR}/external/podofo/src/podofo/podofo/auxiliary
    )
    # Define PODOFO_STATIC for static linking
    target_compile_definitions(kolosal_server PRIVATE PODOFO_STATIC)
endif()

# Find and link thread library for the main library.
find_package(Threads REQUIRED)
target_link_libraries(kolosal_server PRIVATE Threads::Threads)

# Platform-specific link libraries for the main library.
if(WIN32)
    target_link_libraries(kolosal_server PRIVATE ws2_32 wbemuuid)
elseif(APPLE)
    # For macOS, link with required system libraries
    target_link_libraries(kolosal_server PRIVATE dl pthread)
    
    # Add required macOS frameworks for system operations
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    if(COREFOUNDATION_FRAMEWORK)
        target_link_libraries(kolosal_server PRIVATE ${COREFOUNDATION_FRAMEWORK})
    endif()
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
    
elseif(APPLE)
    # macOS-specific configurations
    message(STATUS "Configuring for macOS")
    
    # Add macOS-specific compile definitions
    target_compile_definitions(kolosal_server PRIVATE 
        _DARWIN_C_SOURCE
        __APPLE__
    )
    
    # Enable large file support
    target_compile_definitions(kolosal_server PRIVATE
        _FILE_OFFSET_BITS=64
        _LARGEFILE_SOURCE
        _LARGEFILE64_SOURCE
    )
    
    # Set RPATH for proper library loading
    set_target_properties(kolosal_server_exe PROPERTIES
        INSTALL_RPATH "@executable_path"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH_USE_LINK_PATH TRUE
        MACOSX_RPATH TRUE
    )
    
    # Also add the other paths separately if needed
    set_property(TARGET kolosal_server_exe APPEND PROPERTY INSTALL_RPATH "@executable_path/../lib")
    set_property(TARGET kolosal_server_exe APPEND PROPERTY INSTALL_RPATH "/usr/local/lib")
    set_property(TARGET kolosal_server_exe APPEND PROPERTY INSTALL_RPATH "/opt/homebrew/lib")
    
    set_target_properties(kolosal_server PROPERTIES
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH_USE_LINK_PATH TRUE
        MACOSX_RPATH TRUE
    )
    
    # Also add the other paths separately for the library
    set_property(TARGET kolosal_server APPEND PROPERTY INSTALL_RPATH "@loader_path/../lib")
    set_property(TARGET kolosal_server APPEND PROPERTY INSTALL_RPATH "/usr/local/lib")
    set_property(TARGET kolosal_server APPEND PROPERTY INSTALL_RPATH "/opt/homebrew/lib")
    
endif()

# Build inference engines
message(STATUS "Building inference engines...")

# Note: inference subdirectory is already added earlier in the CMakeLists.txt

# Copy necessary libraries to the executable directory after inference target is defined
# This ensures that the executable can find the shared libraries at runtime
if(WIN32)
    # Copy the main server DLL
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:kolosal_server>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying kolosal_server.dll to executable directory"
    )
    
    # Copy inference engine DLL to executable directory
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${INFERENCE_TARGET}>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying ${INFERENCE_TARGET}.dll to executable directory"
    )
    
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
    
elseif(APPLE)
    # Copy the main server dylib for macOS
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:kolosal_server>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying libkolosal_server.dylib to executable directory"
    )
    
    # Copy the inference engine dylib for macOS
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${INFERENCE_TARGET}>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying ${INFERENCE_TARGET}.dylib to executable directory"
    )
    
    # Fix the install name of the dylib to use @rpath
    add_custom_command(TARGET kolosal_server POST_BUILD
        COMMAND install_name_tool -id "@rpath/libkolosal_server.dylib" "$<TARGET_FILE:kolosal_server>"
        COMMENT "Setting install name for libkolosal_server.dylib"
    )
    
elseif(UNIX)
    # Copy the main server shared library for Linux
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:kolosal_server>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying libkolosal_server.so to executable directory"
    )
    
    # Copy the inference engine shared library for Linux
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${INFERENCE_TARGET}>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying ${INFERENCE_TARGET}.so to executable directory"
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