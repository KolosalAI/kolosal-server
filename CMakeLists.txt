cmake_minimum_required(VERSION 3.14)

# Set policies for compatibility
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(KolosalServer VERSION 1.0.0 LANGUAGES C CXX)

# Module paths
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/overrides")

# Windows-specific vcpkg configuration
if(WIN32)
    set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "Disable vcpkg applocal dependency copying" FORCE)
    set(VCPKG_PWSH_PATH "powershell.exe" CACHE STRING "Path to PowerShell for vcpkg applocal step" FORCE)
    # Also propagate settings into generated MSBuild projects to avoid pwsh.exe requirement
    # and to disable vcpkg's applocal copy step at build time.
    if(MSVC)
        set(CMAKE_VS_GLOBALS
            "${CMAKE_VS_GLOBALS};VcpkgApplocalDeps=false;VcpkgUsePowerShellCore=false"
        )
    endif()
endif()

# Output directory configuration
if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${_cfg} _CFG_UP)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFG_UP} "${CMAKE_BINARY_DIR}/${_cfg}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_CFG_UP} "${CMAKE_BINARY_DIR}/${_cfg}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_CFG_UP} "${CMAKE_BINARY_DIR}/${_cfg}")
    endforeach()
elseif(CMAKE_BUILD_TYPE)
    set(_single_dir "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
    file(MAKE_DIRECTORY "${_single_dir}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_single_dir}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_single_dir}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${_single_dir}")
endif()

# Runtime library management
include(${CMAKE_SOURCE_DIR}/cmake/ucm.cmake)
ucm_set_runtime(STATIC)  # Fixed: comment said static but was set to DYNAMIC

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable PIC unconditionally (was checking ARM vs x86 but enabling for both)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Platform definitions
if(MINGW)
    add_compile_definitions(_WIN32_WINNT=0x602)
endif()
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# Options
option(USE_CUDA   "Compile with CUDA support" OFF)
option(USE_VULKAN "Compile with VULKAN support" OFF)
option(USE_METAL  "Compile with Metal support (macOS only)" OFF)
option(USE_MPI    "Compile with MPI support" OFF)
option(DEBUG      "Compile with debugging information" OFF)
option(USE_PODOFO "Compile with PoDoFo PDF support" ON)
option(USE_FAISS  "Compile with FAISS support" ON)

# Auto-enable Metal on Apple
if(APPLE AND NOT DEFINED USE_METAL)
    set(USE_METAL ON CACHE BOOL "Enable Metal acceleration on Apple systems" FORCE)
endif()

# Core include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/inference/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/pugixml/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/maddy/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/zlib
    ${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/contrib/minizip
)

# Conditional includes
if(USE_PODOFO)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/src)
endif()
if(USE_FAISS)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/faiss)
endif()

# llama.cpp includes (simplified path detection)
set(LLAMA_PATH "${CMAKE_CURRENT_SOURCE_DIR}/inference/external/llama.cpp")
if(NOT EXISTS "${LLAMA_PATH}/include" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp/include")
    set(LLAMA_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/llama.cpp")
endif()
if(EXISTS "${LLAMA_PATH}/include")
    include_directories(
        ${LLAMA_PATH}/include
        ${LLAMA_PATH}/common
        ${LLAMA_PATH}/ggml/include
    )
endif()

# Source files
set(SOURCES
    src/server.cpp
    src/server_api.cpp    
    src/server_config.cpp
    src/logger.cpp
    src/download_utils.cpp
    src/download_manager.cpp
    src/node_manager.cpp
    src/inference_loader.cpp
    src/inference_interface_impl.cpp
    src/gpu_detection.cpp
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
    src/models/embedding_request_model.cpp
    src/models/embedding_response_model.cpp
    src/models/chunking_request_model.cpp
    src/models/chunking_response_model.cpp
    src/auth/rate_limiter.cpp
    src/auth/cors_handler.cpp
    src/auth/auth_middleware.cpp
)

# Retrieval sources are now built under retrieval/ subdirectory

# Create shared library
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD INFERENCE_EXPORTS)

# macOS library properties
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

# === DEPENDENCIES ===

# yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/yaml-cpp)

# Determine inference target
if(USE_CUDA)
    set(INFERENCE_TARGET "llama-cuda")
elseif(USE_VULKAN)
    set(INFERENCE_TARGET "llama-vulkan")
elseif(USE_METAL)
    set(INFERENCE_TARGET "llama-metal")
else()
    set(INFERENCE_TARGET "llama-cpu")
endif()
message(STATUS "Building inference engine: ${INFERENCE_TARGET}")

# Build inference engine
add_subdirectory(inference)
add_subdirectory(retrieval)

# CURL configuration
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    set(CURL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/curl")
    if(EXISTS "${CURL_PATH}/include/curl/curl.h")
        find_library(CURL_LIBRARY 
            NAMES curl libcurl
            PATHS "${CURL_PATH}/lib"
            PATH_SUFFIXES Release Debug
            NO_DEFAULT_PATH
        )
    endif()
endif()

# ZLIB
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_MINIZIP OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/zlib EXCLUDE_FROM_ALL)
    # Create a consistent imported target for consumers
    if(NOT TARGET ZLIB::ZLIB)
        if(TARGET zlibstatic)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        elseif(TARGET zlib)
            add_library(ZLIB::ZLIB ALIAS zlib)
        endif()
    endif()
    set(ZLIB_FOUND TRUE)
endif()

# Minizip
if(NOT TARGET minizip_static)
    set(MINIZIP_SOURCES
        external/zlib/contrib/minizip/ioapi.c
        external/zlib/contrib/minizip/mztools.c
        external/zlib/contrib/minizip/unzip.c
        external/zlib/contrib/minizip/zip.c
    )
    if(WIN32)
        list(APPEND MINIZIP_SOURCES external/zlib/contrib/minizip/iowin32.c)
    endif()
    
    add_library(minizip_static STATIC ${MINIZIP_SOURCES})
    target_include_directories(minizip_static PUBLIC 
        external/zlib/contrib/minizip 
        external/zlib
    )
    if(TARGET ZLIB::ZLIB)
        target_link_libraries(minizip_static PUBLIC ZLIB::ZLIB)
    endif()
    set_target_properties(minizip_static PROPERTIES
        OUTPUT_NAME minizip
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# PugiXML
if(NOT TARGET pugixml)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/pugixml/CMakeLists.txt")
        set(PUGIXML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(external/pugixml EXCLUDE_FROM_ALL)
    else()
        add_library(pugixml STATIC external/pugixml/src/pugixml.cpp)
        target_include_directories(pugixml PUBLIC external/pugixml/src)
        set_target_properties(pugixml PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

# PoDoFo (simplified)
if(USE_PODOFO AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/CMakeLists.txt")
    # Find dependencies
    find_package(Freetype QUIET)
    find_package(JPEG QUIET)
    find_package(PNG QUIET)
    find_package(TIFF QUIET)
    find_package(LibXml2 QUIET)
    find_package(OpenSSL QUIET)

    if(NOT OpenSSL_FOUND)
        message(WARNING "OpenSSL not found; disabling USE_PODOFO and using PDF parser stub.")
        set(USE_PODOFO OFF CACHE BOOL "Compile with PoDoFo PDF support" FORCE)
    else()
        # Configure PoDoFo
        set(PODOFO_BUILD_STATIC ON CACHE BOOL "" FORCE)
        set(PODOFO_BUILD_TEST OFF CACHE BOOL "" FORCE)
        set(PODOFO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(PODOFO_BUILD_UNSUPPORTED_TOOLS OFF CACHE BOOL "" FORCE)
        set(PODOFO_BUILD_LIB_ONLY ON CACHE BOOL "" FORCE)
        set(CMAKE_SKIP_INSTALL_RULES TRUE)
        add_subdirectory(external/podofo EXCLUDE_FROM_ALL)
        set(CMAKE_SKIP_INSTALL_RULES FALSE)

        if(TARGET podofo_static)
            set_target_properties(podofo_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()

        if(EXISTS "${CMAKE_BINARY_DIR}/external/podofo/src/podofo/podofo/auxiliary/podofo_config.h")
            target_include_directories(kolosal_server PRIVATE "${CMAKE_BINARY_DIR}/external/podofo/src/podofo/podofo/auxiliary")
        endif()
    endif()
endif()

# FAISS (simplified)
if(USE_FAISS)
    find_package(faiss QUIET)
    if(NOT faiss_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/faiss/CMakeLists.txt")
        # Handle OpenMP requirement (especially on macOS where it's not included by default)
        find_package(OpenMP QUIET)
        if(NOT OpenMP_FOUND)
            if(APPLE)
                # Check common homebrew locations for libomp
                find_path(OpenMP_INCLUDE_DIR
                    NAMES omp.h
                    PATHS /opt/homebrew/opt/libomp/include /usr/local/opt/libomp/include
                    NO_DEFAULT_PATH
                )
                find_library(OpenMP_libomp_LIBRARY
                    NAMES omp libomp
                    PATHS /opt/homebrew/opt/libomp/lib /usr/local/opt/libomp/lib
                    NO_DEFAULT_PATH
                )
                if(OpenMP_libomp_LIBRARY AND OpenMP_INCLUDE_DIR)
                    # Set OpenMP variables for CMake FindOpenMP
                    set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I${OpenMP_INCLUDE_DIR}" CACHE STRING "" FORCE)
                    set(OpenMP_C_LIB_NAMES "omp" CACHE STRING "" FORCE)
                    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${OpenMP_INCLUDE_DIR}" CACHE STRING "" FORCE)
                    set(OpenMP_CXX_LIB_NAMES "omp" CACHE STRING "" FORCE)
                    set(OpenMP_omp_LIBRARY ${OpenMP_libomp_LIBRARY} CACHE FILEPATH "" FORCE)
                    set(OpenMP_C_LIBRARIES ${OpenMP_libomp_LIBRARY} CACHE STRING "" FORCE)
                    set(OpenMP_CXX_LIBRARIES ${OpenMP_libomp_LIBRARY} CACHE STRING "" FORCE)
                    set(OpenMP_FOUND TRUE CACHE BOOL "" FORCE)
                    
                    # Also set global compile flags to ensure all FAISS files get the include path
                    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${OpenMP_INCLUDE_DIR}")
                    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${OpenMP_INCLUDE_DIR}")
                    
                    message(STATUS "Found libomp: ${OpenMP_libomp_LIBRARY}")
                    message(STATUS "Found omp.h: ${OpenMP_INCLUDE_DIR}")
                else()
                    message(FATAL_ERROR "OpenMP (libomp) is required for FAISS but not found.\n"
                        "Please install it with: brew install libomp\n"
                        "Or disable FAISS with: -DUSE_FAISS=OFF")
                endif()
            else()
                message(FATAL_ERROR "OpenMP is required for FAISS but not found.\n"
                    "On Ubuntu/Debian: apt install libomp-dev\n"
                    "On RHEL/CentOS: yum install libomp-devel\n"
                    "Or disable FAISS with: -DUSE_FAISS=OFF")
            endif()
        endif()
        
        # FAISS build options
        set(FAISS_ENABLE_GPU OFF CACHE BOOL "" FORCE)
        set(FAISS_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(FAISS_OPT_LEVEL generic CACHE STRING "Use generic optimization" FORCE)
        
        # Ensure BLAS is available (required for FAISS)
        find_package(BLAS QUIET)
        if(NOT BLAS_FOUND AND APPLE)
            # Use Accelerate framework on macOS
            find_library(ACCELERATE_FRAMEWORK Accelerate)
            if(ACCELERATE_FRAMEWORK)
                set(BLAS_LIBRARIES ${ACCELERATE_FRAMEWORK})
                set(BLAS_FOUND TRUE)
                message(STATUS "Using Accelerate framework for BLAS")
            endif()
        endif()
        
        if(NOT BLAS_FOUND)
            message(FATAL_ERROR "BLAS is required for FAISS. On macOS: comes with Accelerate. On Linux: sudo apt-get install libblas-dev liblapack-dev")
        endif()
        
        add_subdirectory(external/faiss EXCLUDE_FROM_ALL)
    endif()
endif()

# === COMPILE DEFINITIONS ===
target_compile_definitions(kolosal_server PUBLIC
    $<$<BOOL:${USE_CUDA}>:USE_CUDA>
    $<$<BOOL:${USE_VULKAN}>:USE_VULKAN>
    $<$<BOOL:${USE_MPI}>:USE_MPI>
    $<$<BOOL:${DEBUG}>:DEBUG>
    $<$<BOOL:${USE_PODOFO}>:USE_PODOFO>
    $<$<BOOL:${USE_FAISS}>:USE_FAISS>
)

# === LINKING ===
set(KOLOSAL_LINK_LIBRARIES 
    yaml-cpp 
    ${INFERENCE_TARGET} 
    kolosal-retrieval
    minizip_static 
    pugixml
)

if(USE_PODOFO AND TARGET podofo_static)
    list(APPEND KOLOSAL_LINK_LIBRARIES podofo_static)
    if(TARGET OpenSSL::SSL)
        list(APPEND KOLOSAL_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    endif()
    target_compile_definitions(kolosal_server PRIVATE PODOFO_STATIC)
endif()

if(USE_FAISS AND TARGET faiss)
    list(APPEND KOLOSAL_LINK_LIBRARIES faiss)
endif()

if(CURL_LIBRARY)
    list(APPEND KOLOSAL_LINK_LIBRARIES ${CURL_LIBRARY})
endif()

# Link core libraries
target_link_libraries(kolosal_server PRIVATE ${KOLOSAL_LINK_LIBRARIES})
target_link_libraries(kolosal_server PUBLIC ${INFERENCE_TARGET})
if(TARGET llama)
    target_link_libraries(kolosal_server PRIVATE llama)
endif()

# System libraries
find_package(Threads REQUIRED)
target_link_libraries(kolosal_server PRIVATE Threads::Threads)

# Link OpenMP for targets (FAISS / headers may include <omp.h>)
find_package(OpenMP QUIET)
if(OpenMP_FOUND AND TARGET OpenMP::OpenMP_CXX)
    target_link_libraries(kolosal_server PRIVATE OpenMP::OpenMP_CXX)
endif()

if(WIN32)
    target_link_libraries(kolosal_server PRIVATE ws2_32 wbemuuid)
elseif(APPLE)
    target_link_libraries(kolosal_server PRIVATE dl pthread)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    if(COREFOUNDATION_FRAMEWORK)
        target_link_libraries(kolosal_server PRIVATE ${COREFOUNDATION_FRAMEWORK})
    endif()
else()
    target_link_libraries(kolosal_server PRIVATE dl rt pthread)
endif()

# === OPTIMIZATION FLAGS ===
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(kolosal_server PRIVATE /O2 /Ob2)
    else()
        target_compile_options(kolosal_server PRIVATE -O3)
        option(ENABLE_NATIVE_OPTIMIZATION "Enable native CPU optimization" OFF)
        if(ENABLE_NATIVE_OPTIMIZATION)
            target_compile_options(kolosal_server PRIVATE -march=native -mtune=native)
        endif()
    endif()
endif()

# === PLATFORM CONFIGURATION ===
if(UNIX AND NOT APPLE)
    target_compile_definitions(kolosal_server PRIVATE 
        _GNU_SOURCE 
        __LINUX__ 
        _FILE_OFFSET_BITS=64
        _LARGEFILE_SOURCE
    )
    set_target_properties(kolosal_server kolosal_server_exe PROPERTIES
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif(APPLE)
    target_compile_definitions(kolosal_server PRIVATE 
        _DARWIN_C_SOURCE 
        __APPLE__
        _FILE_OFFSET_BITS=64
        _LARGEFILE_SOURCE
    )
    set_target_properties(kolosal_server_exe PROPERTIES
        INSTALL_RPATH "@executable_path:@executable_path/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
        MACOSX_RPATH TRUE
    )
endif()

# === POST-BUILD COPY ===
# Copy shared libraries to executable directory
add_custom_command(TARGET kolosal_server_exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:kolosal_server>"
    "$<TARGET_FILE_DIR:kolosal_server_exe>/"
    COMMENT "Copying kolosal_server library to executable directory"
)

add_custom_command(TARGET kolosal_server_exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:${INFERENCE_TARGET}>"
    "$<TARGET_FILE_DIR:kolosal_server_exe>/"
    COMMENT "Copying ${INFERENCE_TARGET} to executable directory"
)

# === SELF-CONTAINED DIST PACKAGE ===
# Create a fully self-contained "dist" directory with executable, libs, shaders, configs and dependent dylibs.
set(KOLOSAL_FULL_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
add_custom_target(dist
    DEPENDS kolosal_server_exe
    COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE_PATH="$<TARGET_FILE:kolosal_server_exe>"
        -DLIB_SERVER_PATH="$<TARGET_FILE:kolosal_server>"
        -DLIB_INFERENCE_PATH="$<TARGET_FILE:${INFERENCE_TARGET}>"
        -DDIST_DIR="${KOLOSAL_FULL_DIST_DIR}"
        -DMETAL_BACKEND=$<BOOL:${USE_METAL}>
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/overrides/MakeDist.cmake"
    COMMENT "Assembling self-contained distribution in dist/"
)

# === INSTALLATION ===
if(APPLE)
    install(TARGETS kolosal_server
        LIBRARY DESTINATION "Kolosal.app/Contents/Frameworks"
        COMPONENT Runtime
    )
    install(TARGETS kolosal_server_exe
        RUNTIME DESTINATION "Kolosal.app/Contents/MacOS"
        COMPONENT Runtime
    )
endif()

option(INSTALL_HEADERS "Install header files for development" OFF)
if(INSTALL_HEADERS)
    install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()