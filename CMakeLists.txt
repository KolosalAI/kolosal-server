cmake_minimum_required(VERSION 3.14)

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

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/overrides")

if(WIN32)
    set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "Disable vcpkg applocal dependency copying" FORCE)
    set(VCPKG_PWSH_PATH "powershell.exe" CACHE STRING "Path to PowerShell for vcpkg applocal step" FORCE)
endif()

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

include(${CMAKE_SOURCE_DIR}/cmake/ucm.cmake)
ucm_set_runtime(STATIC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MINGW)
    add_compile_definitions(_WIN32_WINNT=0x602)
endif()
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

option(USE_CUDA   "Compile with CUDA support" OFF)
option(USE_VULKAN "Compile with VULKAN support" ON)
option(USE_METAL  "Compile with Metal support (macOS only)" OFF)
option(USE_MPI    "Compile with MPI support" OFF)
option(DEBUG      "Compile with debugging information" OFF)
option(USE_PODOFO "Compile with PoDoFo PDF support" OFF)
option(USE_FAISS  "Compile with FAISS support" ON)

if(APPLE AND NOT DEFINED USE_METAL)
    set(USE_METAL ON CACHE BOOL "Enable Metal acceleration on Apple systems" FORCE)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/inference/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/pugixml/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/maddy/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/zlib
    ${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/contrib/minizip
    ${CMAKE_CURRENT_SOURCE_DIR}/external/curl/include
)

if(USE_PODOFO)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/src)
    # Include the build directory where podofo_config.h is generated
    include_directories(${CMAKE_BINARY_DIR}/external/podofo/src)
endif()
if(USE_FAISS)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/faiss)
endif()

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

set(SOURCES
    src/server.cpp
    src/server_api.cpp    
    src/server_config.cpp
    src/logger.cpp
    src/download_utils.cpp
    src/download_manager.cpp
    src/faiss_client.cpp
    src/qdrant_client.cpp
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
    src/routes/ui_routes.cpp    
    src/routes/retrieval/embedding_route.cpp
    src/routes/retrieval/internet_search_route.cpp
    src/routes/retrieval/parse_document_route.cpp
    src/routes/retrieval/documents_route.cpp
    src/routes/downloads_route.cpp
    src/routes/retrieval/chunking_route.cpp
    src/retrieval/add_document_types.cpp
    src/retrieval/remove_document_types.cpp
    src/retrieval/document_list_types.cpp
    src/retrieval/retrieve_types.cpp
    src/retrieval/document_service.cpp
    src/retrieval/parse_docx.cpp
    src/retrieval/parse_html.cpp
    src/retrieval/chunking_types.cpp
    src/models/embedding_request_model.cpp
    src/models/embedding_response_model.cpp
    src/models/chunking_request_model.cpp
    src/models/chunking_response_model.cpp
    src/auth/rate_limiter.cpp
    src/auth/cors_handler.cpp
    src/auth/auth_middleware.cpp
)

if(USE_PODOFO)
    list(APPEND SOURCES src/retrieval/parse_pdf.cpp)
else()
    list(APPEND SOURCES src/retrieval/parse_pdf_stub.cpp)
endif()

add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD INFERENCE_EXPORTS)

if(APPLE)
    set_target_properties(kolosal_server PROPERTIES
        MACOSX_RPATH TRUE
        INSTALL_NAME_DIR "@rpath"
    )
endif()

add_executable(kolosal_server_exe src/main.cpp)
target_link_libraries(kolosal_server_exe PRIVATE kolosal_server)
set_target_properties(kolosal_server_exe PROPERTIES OUTPUT_NAME "kolosal-server")

# On Windows, ensure console is shown and DLL loading works properly
if(WIN32)
    # Force use the standard entry point and console subsystem
    if(MSVC)
        set_target_properties(kolosal_server_exe PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup"
            WIN32_EXECUTABLE FALSE
        )
    else()
        set_target_properties(kolosal_server_exe PROPERTIES
            LINK_FLAGS "-mconsole"
            WIN32_EXECUTABLE FALSE
        )
    endif()
    
    # Ensure the executable can find DLLs in the same directory
    # This is the key to making DLLs load from the executable directory
    target_compile_definitions(kolosal_server_exe PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# === DEPENDENCIES ===

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/yaml-cpp)

# Build multiple inference targets for comprehensive hardware support
set(INFERENCE_TARGETS "llama-cpu")
set(PRIMARY_INFERENCE_TARGET "llama-cpu")

# Always build CPU version as base
message(STATUS "Including CPU inference target: llama-cpu")

# Add accelerated targets based on options
if(USE_VULKAN)
    list(APPEND INFERENCE_TARGETS "llama-vulkan")
    set(PRIMARY_INFERENCE_TARGET "llama-vulkan")
    message(STATUS "Including Vulkan inference target: llama-vulkan")
endif()

if(USE_CUDA)
    list(APPEND INFERENCE_TARGETS "llama-cuda") 
    set(PRIMARY_INFERENCE_TARGET "llama-cuda")
    message(STATUS "Including CUDA inference target: llama-cuda")
endif()

if(USE_METAL)
    list(APPEND INFERENCE_TARGETS "llama-metal")
    set(PRIMARY_INFERENCE_TARGET "llama-metal") 
    message(STATUS "Including Metal inference target: llama-metal")
endif()

# Set main target for backward compatibility
set(INFERENCE_TARGET ${PRIMARY_INFERENCE_TARGET})

message(STATUS "Primary inference engine: ${PRIMARY_INFERENCE_TARGET}")
message(STATUS "All inference engines to build: ${INFERENCE_TARGETS}")

# When building accelerated backends, ensure CPU backend is available as fallback
if(USE_VULKAN OR USE_CUDA OR USE_METAL)
    set(GGML_CPU ON CACHE BOOL "Enable CPU backend as fallback" FORCE)
    message(STATUS "CPU backend enabled as fallback within accelerated targets")
endif()

# === ZLIB DEPENDENCY (needed by PoDoFo) ===
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_MINIZIP OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/zlib EXCLUDE_FROM_ALL)
    set(ZLIB_FOUND TRUE)
    set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib;${CMAKE_CURRENT_BINARY_DIR}/external/zlib")
    set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}")
    if(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
        set(ZLIB_LIBRARIES ZLIB::ZLIB)
        set(ZLIB_LIBRARY zlibstatic)  # Set the raw target name that find_package expects
    endif()
endif()

# Pass the list of targets to build to inference CMakeLists.txt
set(INFERENCE_TARGETS_TO_BUILD ${INFERENCE_TARGETS})
add_subdirectory(inference)

find_package(CURL QUIET)
if(NOT CURL_FOUND)
    set(CURL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/curl")
    if(EXISTS "${CURL_PATH}/include/curl/curl.h")
        set(CURL_INCLUDE_DIR "${CURL_PATH}/include")
        find_library(CURL_LIBRARY 
            NAMES curl libcurl
            PATHS "${CURL_PATH}/lib"
            PATH_SUFFIXES Release Debug
            NO_DEFAULT_PATH
        )
        if(CURL_LIBRARY)
            set(CURL_FOUND TRUE)
            message(STATUS "Found curl library: ${CURL_LIBRARY}")
        endif()
    endif()
endif()

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

if(USE_PODOFO AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/CMakeLists.txt")
    find_package(Freetype QUIET)
    find_package(JPEG QUIET)
    find_package(PNG QUIET)
    find_package(TIFF QUIET)
    find_package(LibXml2 QUIET)
    find_package(OpenSSL QUIET)

    if(OPENSSL_FOUND)
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

        # Include the generated podofo_config.h directory
        target_include_directories(kolosal_server PRIVATE "${CMAKE_BINARY_DIR}/external/podofo/src")
        message(STATUS "PoDoFo PDF support enabled with OpenSSL")
    else()
        message(WARNING "OpenSSL not found - PoDoFo PDF support will be disabled")
        set(USE_PODOFO OFF CACHE BOOL "Disabled due to missing OpenSSL" FORCE)
    endif()
endif()

if(USE_FAISS)
    find_package(faiss QUIET)
    if(NOT faiss_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/faiss/CMakeLists.txt")
        find_package(OpenMP QUIET)
        if(NOT OpenMP_FOUND)
            if(APPLE)
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
                    set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I${OpenMP_INCLUDE_DIR}" CACHE STRING "" FORCE)
                    set(OpenMP_C_LIB_NAMES "omp" CACHE STRING "" FORCE)
                    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${OpenMP_INCLUDE_DIR}" CACHE STRING "" FORCE)
                    set(OpenMP_CXX_LIB_NAMES "omp" CACHE STRING "" FORCE)
                    set(OpenMP_omp_LIBRARY ${OpenMP_libomp_LIBRARY} CACHE FILEPATH "" FORCE)
                    set(OpenMP_C_LIBRARIES ${OpenMP_libomp_LIBRARY} CACHE STRING "" FORCE)
                    set(OpenMP_CXX_LIBRARIES ${OpenMP_libomp_LIBRARY} CACHE STRING "" FORCE)
                    set(OpenMP_FOUND TRUE CACHE BOOL "" FORCE)

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

        set(FAISS_ENABLE_GPU OFF CACHE BOOL "" FORCE)
        set(FAISS_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(FAISS_OPT_LEVEL generic CACHE STRING "Use generic optimization" FORCE)

        find_package(BLAS QUIET)
        if(NOT BLAS_FOUND AND APPLE)
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
    ${PRIMARY_INFERENCE_TARGET}
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

if(CURL_FOUND OR CURL_LIBRARY)
    list(APPEND KOLOSAL_LINK_LIBRARIES ${CURL_LIBRARY})
    message(STATUS "Linking curl library: ${CURL_LIBRARY}")
endif()

target_link_libraries(kolosal_server PRIVATE ${KOLOSAL_LINK_LIBRARIES})
target_link_libraries(kolosal_server PUBLIC ${PRIMARY_INFERENCE_TARGET})
if(TARGET llama)
    target_link_libraries(kolosal_server PRIVATE llama)
endif()

find_package(Threads REQUIRED)
target_link_libraries(kolosal_server PRIVATE Threads::Threads)

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
elseif(WIN32)
    # Set Windows-specific DLL search path behavior
    # This helps the executable find DLLs in the same directory first
    # Note: WIN32_EXECUTABLE is already set to FALSE above
    
    # Add manifest to specify DLL search path behavior (optional)
    if(MSVC)
        set_target_properties(kolosal_server_exe PROPERTIES
            LINK_FLAGS "/MANIFEST:EMBED"
        )
    endif()
endif()

# === POST-BUILD COPY ===
add_custom_command(TARGET kolosal_server_exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:kolosal_server_exe>/bin"
    COMMENT "Creating bin directory for DLLs"
)

add_custom_command(TARGET kolosal_server_exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:kolosal_server>"
    "$<TARGET_FILE_DIR:kolosal_server_exe>/"
    COMMENT "Copying kolosal_server library to executable directory"
)

# Copy all inference engines to executable directory  
foreach(INFERENCE_ENGINE ${INFERENCE_TARGETS})
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${INFERENCE_ENGINE}>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying ${INFERENCE_ENGINE} to executable directory"
    )
endforeach()

# Also copy to bin directory for portable packaging
add_custom_command(TARGET kolosal_server_exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:kolosal_server>"
    "$<TARGET_FILE_DIR:kolosal_server_exe>/bin/"
    COMMENT "Copying kolosal_server library to bin directory"
)

# Copy all inference engines to bin directory for portable packaging
foreach(INFERENCE_ENGINE ${INFERENCE_TARGETS})
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${INFERENCE_ENGINE}>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/bin/"
        COMMENT "Copying ${INFERENCE_ENGINE} to bin directory"
    )
endforeach()

# Copy runtime dependencies for Windows
if(WIN32)
    # Find and copy all required DLLs
    set(RUNTIME_DLL_SEARCH_PATHS
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_BINARY_DIR}/Release"
        "${CMAKE_BINARY_DIR}/Debug"
        "${CMAKE_BINARY_DIR}/external/faiss/faiss"
        "${CMAKE_BINARY_DIR}/external/faiss"
        "${CMAKE_BINARY_DIR}/inference/Release"
        "${CMAKE_BINARY_DIR}/inference/Debug"
        "C:/msys64/mingw64/bin"
        "C:/msys64/ucrt64/bin"
        "$ENV{VCPKG_ROOT}/installed/x64-windows/bin"
        "C:/vcpkg/installed/x64-windows/bin"
        "$ENV{SystemRoot}/System32"
        "$ENV{WINDIR}/System32"
    )
    
    # Also search in PATH
    if(DEFINED ENV{PATH})
        string(REPLACE ";" ";" PATH_DIRS "$ENV{PATH}")
        list(APPEND RUNTIME_DLL_SEARCH_PATHS ${PATH_DIRS})
    endif()
    
    # Common runtime DLLs that might be needed
    set(REQUIRED_RUNTIME_DLLS
        libopenblas.dll
        libgfortran-5.dll
        libquadmath-0.dll
        libgcc_s_seh-1.dll
        VCOMP140.DLL
        vcomp140.dll
        libcurl.dll
        libcurl-x64.dll
        vcruntime140.dll
        msvcp140.dll
        libstdc++-6.dll
        libwinpthread-1.dll
        libgomp-1.dll
        zlib1.dll
    )
    
    # Copy all found DLLs to the executable directory (primary location)
    foreach(dll_name ${REQUIRED_RUNTIME_DLLS})
        set(dll_found FALSE)
        foreach(search_path ${RUNTIME_DLL_SEARCH_PATHS})
            # Handle paths with backslashes
            file(TO_CMAKE_PATH "${search_path}" norm_search_path)
            if(EXISTS "${norm_search_path}/${dll_name}")
                # Copy to main directory for direct execution (this is the key fix!)
                add_custom_command(TARGET kolosal_server_exe POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${norm_search_path}/${dll_name}"
                    "$<TARGET_FILE_DIR:kolosal_server_exe>/"
                    COMMENT "Copying ${dll_name} to executable directory"
                )
                # Also copy to bin directory for portable packaging
                add_custom_command(TARGET kolosal_server_exe POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${norm_search_path}/${dll_name}"
                    "$<TARGET_FILE_DIR:kolosal_server_exe>/bin/"
                    COMMENT "Copying ${dll_name} to bin directory"
                )
                message(STATUS "Found ${dll_name} at: ${norm_search_path}")
                set(dll_found TRUE)
                break()
            endif()
        endforeach()
        if(NOT dll_found)
            if(dll_name MATCHES "libopenblas.dll")
                message(WARNING "OpenBLAS DLL not found: ${dll_name}\n"
                    "========================================\n"
                    "FAISS functionality will NOT work without OpenBLAS!\n"
                    "To fix this:\n"
                    "  1. Install MSYS2 from https://www.msys2.org/\n"
                    "  2. Run: pacman -S mingw-w64-x86_64-openblas\n"
                    "  3. Copy libopenblas.dll from C:/msys64/mingw64/bin/ to build/Release/\n"
                    "Or download from: https://github.com/xianyi/OpenBLAS/releases\n"
                    "========================================")
            else()
                message(STATUS "Optional DLL not found: ${dll_name}")
            endif()
        endif()
    endforeach()
    
    # Enhanced OpenBLAS handling - create compatibility DLLs
    file(WRITE "${CMAKE_BINARY_DIR}/copy_openblas.cmake" "
        set(SOURCE_DIR \"\${SOURCE_DIR}\")
        
        # Check for libopenblas.dll in the executable directory first
        if(EXISTS \"\${SOURCE_DIR}/libopenblas.dll\")
            message(STATUS \"Found libopenblas.dll, creating compatibility DLLs...\")
            
            # Create openblas.dll symlink/copy
            execute_process(COMMAND \${CMAKE_COMMAND} -E copy_if_different
                \"\${SOURCE_DIR}/libopenblas.dll\"
                \"\${SOURCE_DIR}/openblas.dll\"
            )
            
            # Create liblapack.dll symlink/copy  
            execute_process(COMMAND \${CMAKE_COMMAND} -E copy_if_different
                \"\${SOURCE_DIR}/libopenblas.dll\"
                \"\${SOURCE_DIR}/liblapack.dll\"
            )
            
            # Also copy to bin directory if it exists
            if(EXISTS \"\${SOURCE_DIR}/bin\")
                execute_process(COMMAND \${CMAKE_COMMAND} -E copy_if_different
                    \"\${SOURCE_DIR}/libopenblas.dll\"
                    \"\${SOURCE_DIR}/bin/libopenblas.dll\"
                )
                execute_process(COMMAND \${CMAKE_COMMAND} -E copy_if_different
                    \"\${SOURCE_DIR}/openblas.dll\"
                    \"\${SOURCE_DIR}/bin/openblas.dll\"
                )
                execute_process(COMMAND \${CMAKE_COMMAND} -E copy_if_different
                    \"\${SOURCE_DIR}/liblapack.dll\"
                    \"\${SOURCE_DIR}/bin/liblapack.dll\"
                )
            endif()
            
            message(STATUS \"Created OpenBLAS compatibility DLLs: openblas.dll and liblapack.dll\")
        else()
            message(STATUS \"libopenblas.dll not found in \${SOURCE_DIR}, skipping OpenBLAS compatibility DLLs\")
        endif()
    ")
    
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} 
            -DSOURCE_DIR="$<TARGET_FILE_DIR:kolosal_server_exe>"
            -P "${CMAKE_BINARY_DIR}/copy_openblas.cmake"
        COMMENT "Creating OpenBLAS compatibility DLLs if available"
    )
    
    # Copy any additional DLLs that might be in the build directory
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying additional DLLs from build directory..."
        COMMAND ${CMAKE_COMMAND} -E true
        COMMENT "Checking for additional runtime DLLs"
    )
    
    # Create a script to copy any DLL found in the build output directory
    file(WRITE "${CMAKE_BINARY_DIR}/copy_additional_dlls.cmake" "
        file(GLOB FOUND_DLLS \"\${BUILD_DIR}/*.dll\")
        foreach(dll_file \${FOUND_DLLS})
            get_filename_component(dll_name \"\${dll_file}\" NAME)
            # Skip if it's already our main executables/libraries
            if(NOT dll_name MATCHES \"kolosal.*\\\\.(exe|dll)\" AND NOT dll_name MATCHES \"llama-.*\\\\.dll\")
                execute_process(COMMAND \${CMAKE_COMMAND} -E copy_if_different
                    \"\${dll_file}\"
                    \"\${TARGET_DIR}/\"
                )
                # Also copy to bin directory
                if(EXISTS \"\${TARGET_DIR}/bin\")
                    execute_process(COMMAND \${CMAKE_COMMAND} -E copy_if_different
                        \"\${dll_file}\"
                        \"\${TARGET_DIR}/bin/\"
                    )
                endif()
                message(STATUS \"Copied additional DLL: \${dll_name}\")
            endif()
        endforeach()
    ")
    
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR="${CMAKE_BINARY_DIR}"
            -DTARGET_DIR="$<TARGET_FILE_DIR:kolosal_server_exe>"
            -P "${CMAKE_BINARY_DIR}/copy_additional_dlls.cmake"
        COMMENT "Copying additional runtime DLLs"
    )
    
    # Create a DLL verification script to help debug DLL loading issues
    file(WRITE "${CMAKE_BINARY_DIR}/verify_dlls.cmake" "
        set(TARGET_DIR \"\${TARGET_DIR}\")
        set(EXECUTABLE_NAME \"kolosal-server.exe\")
        
        message(STATUS \"=== DLL Verification Report ===\")
        message(STATUS \"Executable directory: \${TARGET_DIR}\")
        
        # Check if executable exists
        if(EXISTS \"\${TARGET_DIR}/\${EXECUTABLE_NAME}\")
            message(STATUS \"✓ Executable found: \${EXECUTABLE_NAME}\")
        else()
            message(STATUS \"✗ Executable NOT found: \${EXECUTABLE_NAME}\")
        endif()
        
        # List all DLLs in the directory
        file(GLOB DLLS_IN_DIR \"\${TARGET_DIR}/*.dll\")
        message(STATUS \"DLLs in executable directory:\")
        foreach(dll_file \${DLLS_IN_DIR})
            get_filename_component(dll_name \"\${dll_file}\" NAME)
            message(STATUS \"  ✓ \${dll_name}\")
        endforeach()
        
        # Check bin directory
        if(EXISTS \"\${TARGET_DIR}/bin\")
            file(GLOB DLLS_IN_BIN \"\${TARGET_DIR}/bin/*.dll\")
            message(STATUS \"DLLs in bin subdirectory:\")
            foreach(dll_file \${DLLS_IN_BIN})
                get_filename_component(dll_name \"\${dll_file}\" NAME)
                message(STATUS \"  ✓ bin/\${dll_name}\")
            endforeach()
        else()
            message(STATUS \"No bin subdirectory found\")
        endif()
        
        # Check for critical DLLs
        set(CRITICAL_DLLS kolosal_server.dll)
        foreach(engine ${INFERENCE_TARGETS})
            list(APPEND CRITICAL_DLLS \${engine}.dll)
        endforeach()
        message(STATUS \"Critical DLL check:\")
        foreach(critical_dll \${CRITICAL_DLLS})
            if(EXISTS \"\${TARGET_DIR}/\${critical_dll}\")
                message(STATUS \"  ✓ \${critical_dll} - Found in executable directory\")
            elseif(EXISTS \"\${TARGET_DIR}/bin/\${critical_dll}\")
                message(STATUS \"  ⚠ \${critical_dll} - Found only in bin directory (may cause issues)\")
            else()
                message(STATUS \"  ✗ \${critical_dll} - NOT FOUND\")
            endif()
        endforeach()
        
        message(STATUS \"=== End DLL Verification ===\")
    ")
    
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_DIR="$<TARGET_FILE_DIR:kolosal_server_exe>"
            -P "${CMAKE_BINARY_DIR}/verify_dlls.cmake"
        COMMENT "Verifying DLL setup"
    )
endif()

# Copy static web files to build directory for development
add_custom_command(TARGET kolosal_server_exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/static"
    "$<TARGET_FILE_DIR:kolosal_server_exe>/static"
    COMMENT "Copying static web files to build directory"
)

# === SELF-CONTAINED DIST PACKAGE ===
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

# === PORTABLE ZIP PACKAGE ===
set(KOLOSAL_PORTABLE_DIR "${CMAKE_BINARY_DIR}/portable")
set(KOLOSAL_PORTABLE_ZIP "${CMAKE_BINARY_DIR}/KolosalServer-${PROJECT_VERSION}-portable.zip")
add_custom_target(portable
    DEPENDS kolosal_server_exe
    COMMAND ${CMAKE_COMMAND}
        -DEXECUTABLE_PATH="$<TARGET_FILE:kolosal_server_exe>"
        -DLIB_SERVER_PATH="$<TARGET_FILE:kolosal_server>"
        -DLIB_INFERENCE_PATH="$<TARGET_FILE:${INFERENCE_TARGET}>"
        -DSOURCE_DIR="$<TARGET_FILE_DIR:kolosal_server_exe>"
        -DZIP_DIR="${KOLOSAL_PORTABLE_DIR}"
        -DZIP_FILE="${KOLOSAL_PORTABLE_ZIP}"
        -DSOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        -DPROJECT_VERSION="${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MakePortableZip.cmake"
    COMMENT "Creating portable ZIP package with all dependencies"
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

# === CPACK INSTALLER CONFIGURATION ===

# Install targets for CPack - executables and all DLLs to root directory for proper DLL loading
install(TARGETS kolosal_server_exe
    RUNTIME DESTINATION .
    COMPONENT Runtime
)

# Install main kolosal_server.dll to bin/ directory only (target installs don't seem to work for root)
install(TARGETS kolosal_server
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT Runtime
)

# Install all inference libraries to bin/ directory only
foreach(INFERENCE_ENGINE ${INFERENCE_TARGETS})
    install(TARGETS ${INFERENCE_ENGINE}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        COMPONENT Runtime
    )
endforeach()

# Install all discovered runtime DLLs to root directory alongside executable
if(WIN32)
    # Get list of DLLs at configuration time
    file(GLOB RUNTIME_DLLS "${CMAKE_BINARY_DIR}/Release/*.dll")
    
    # Install each DLL individually to both root and bin directories
    foreach(dll_file ${RUNTIME_DLLS})
        get_filename_component(dll_name "${dll_file}" NAME)
        # Skip the main executable's DLL as it's handled by target installation
        if(NOT dll_name MATCHES "kolosal-server")
            # Install to root directory for direct loading
            install(FILES "${dll_file}"
                DESTINATION .
                COMPONENT Runtime
                OPTIONAL
            )
            message(STATUS "Will install DLL to root: ${dll_name}")
        endif()
        # Install to bin directory for consistency
        install(FILES "${dll_file}"
            DESTINATION bin
            COMPONENT Runtime
            OPTIONAL
        )
    endforeach()
    
    # Also check for DLLs in Release/bin directory
    file(GLOB BIN_RUNTIME_DLLS "${CMAKE_BINARY_DIR}/Release/bin/*.dll")
    foreach(dll_file ${BIN_RUNTIME_DLLS})
        get_filename_component(dll_name "${dll_file}" NAME)
        # Install to root directory for direct loading
        install(FILES "${dll_file}"
            DESTINATION .
            COMPONENT Runtime
            OPTIONAL
        )
        # Install to bin directory for consistency
        install(FILES "${dll_file}"
            DESTINATION bin
            COMPONENT Runtime
            OPTIONAL
        )
        message(STATUS "Will install bin DLL to root: ${dll_name}")
    endforeach()
    
    # Install additional executables to bin/ (excluding main executable)
    file(GLOB RUNTIME_EXES "${CMAKE_BINARY_DIR}/Release/*.exe")
    foreach(exe_file ${RUNTIME_EXES})
        get_filename_component(exe_name "${exe_file}" NAME)
        if(NOT exe_name STREQUAL "kolosal-server.exe")
            install(FILES "${exe_file}"
                DESTINATION bin
                COMPONENT Runtime
                OPTIONAL
            )
            message(STATUS "Will install executable to bin: ${exe_name}")
        endif()
    endforeach()
endif()

# Install configuration files
install(FILES 
    "${CMAKE_SOURCE_DIR}/configs/config.yaml"
    "${CMAKE_SOURCE_DIR}/configs/config.json"
    DESTINATION config
    COMPONENT Configuration
    OPTIONAL
)

# Install documentation
install(FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/LICENSE"
    DESTINATION .
    COMPONENT Documentation
    OPTIONAL
)

# Install assets
install(FILES
    "${CMAKE_SOURCE_DIR}/assets/icon.ico"
    "${CMAKE_SOURCE_DIR}/assets/logo.png"
    DESTINATION assets
    COMPONENT Assets
    OPTIONAL
)

# Install documentation directory
install(DIRECTORY "${CMAKE_SOURCE_DIR}/docs/"
    DESTINATION docs
    COMPONENT Documentation
    OPTIONAL
)

# Install static web files
install(DIRECTORY "${CMAKE_SOURCE_DIR}/static/"
    DESTINATION static
    COMPONENT Runtime
    OPTIONAL
)

# Create directories for runtime data
install(DIRECTORY DESTINATION data/faiss_index COMPONENT Runtime)
install(DIRECTORY DESTINATION logs COMPONENT Runtime)
install(DIRECTORY DESTINATION models COMPONENT Runtime)

# Basic CPack configuration
set(CPACK_PACKAGE_NAME "KolosalServer")
set(CPACK_PACKAGE_VENDOR "Kolosal AI")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance inference server for large language models")
set(CPACK_PACKAGE_DESCRIPTION "Kolosal Server - A high-performance inference server for large language models with OpenAI-compatible API endpoints")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/KolosalAI/kolosal-server")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Kolosal Server")
set(CPACK_PACKAGE_EXECUTABLES "kolosal-server;Kolosal Server")

# License and readme
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Components
set(CPACK_COMPONENTS_ALL Runtime Configuration Documentation Assets)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Kolosal Server Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Core executable and libraries")
set(CPACK_COMPONENT_RUNTIME_REQUIRED TRUE)

set(CPACK_COMPONENT_CONFIGURATION_DISPLAY_NAME "Configuration Files")
set(CPACK_COMPONENT_CONFIGURATION_DESCRIPTION "Sample configuration files")
set(CPACK_COMPONENT_CONFIGURATION_DEPENDS Runtime)

set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "User and developer documentation")

set(CPACK_COMPONENT_ASSETS_DISPLAY_NAME "Assets")
set(CPACK_COMPONENT_ASSETS_DESCRIPTION "Icons and images")

# Platform-specific configuration
if(WIN32)
    # Set default generators - ZIP is always available
    set(CPACK_GENERATOR "ZIP")
    
    # Configure ZIP to use portable package structure
    set(CPACK_ZIP_COMPONENT_INSTALL OFF)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-win64")
    
    # Add NSIS if available
    find_program(NSIS_MAKE NAMES makensis)
    if(NSIS_MAKE)
        message(STATUS "NSIS found: ${NSIS_MAKE}")
        list(APPEND CPACK_GENERATOR "NSIS")
    else()
        message(WARNING "NSIS not found. NSIS installer will not be generated. Install from: https://nsis.sourceforge.io/")
    endif()
    
    # Add WiX if available
    find_program(WIX_CANDLE NAMES candle)
    if(WIX_CANDLE)
        message(STATUS "WiX found: ${WIX_CANDLE}")
        list(APPEND CPACK_GENERATOR "WIX")
    else()
        message(WARNING "WiX not found. MSI installer will not be generated. Install from: https://wixtoolset.org/")
    endif()
    
    # Enhanced Windows DLL discovery and installation
    # Install all DLLs from the Release directory to bin/ (excluding main targets which go to root)
    file(GLOB WIN_DLLS "${CMAKE_BINARY_DIR}/Release/*.dll")
    foreach(dll_file ${WIN_DLLS})
        get_filename_component(dll_name "${dll_file}" NAME)
        # Skip main targets as they're handled separately
        # Skip if it's our main executables/libraries or any inference target
        set(skip_dll FALSE)
        if(dll_name MATCHES "kolosal.*\\.(exe|dll)")
            set(skip_dll TRUE)
        endif()
        foreach(engine_name ${INFERENCE_TARGETS})
            if(dll_name MATCHES "\${engine_name}\\.dll")
                set(skip_dll TRUE)
                break()
            endif()
        endforeach()
        if(NOT skip_dll)
            install(FILES "${dll_file}"
                DESTINATION bin
                COMPONENT Runtime
            )
        endif()
    endforeach()
    
    # Install additional required DLLs if found in system paths
    set(REQUIRED_DLLS
        libcurl.dll
        libcurl-x64.dll
        openblas.dll
        libopenblas.dll
        liblapack.dll
        libgfortran-5.dll
        libquadmath-0.dll
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
        libgomp-1.dll
        zlib1.dll
        libssl-3-x64.dll
        libcrypto-3-x64.dll
        libssl-1_1-x64.dll
        libcrypto-1_1-x64.dll
        VCOMP140.DLL
        vcomp140.dll
        vcruntime140.dll
        msvcp140.dll
    )
    
    # Search for DLLs in common locations
    set(DLL_SEARCH_PATHS
        "${CMAKE_BINARY_DIR}/Release"
        "${CMAKE_BINARY_DIR}"
        "$ENV{VCPKG_ROOT}/installed/x64-windows/bin"
        "$ENV{VCPKG_ROOT}/installed/x64-windows-static/bin"
        "C:/msys64/mingw64/bin"
        "C:/msys64/ucrt64/bin"
        "C:/msys64/clang64/bin"
        "C:/msys32/mingw32/bin"
        "C:/MinGW/bin"
        "C:/MinGW/x86_64-w64-mingw32/bin"
        "C:/Program Files/mingw-w64/bin"
        "C:/Program Files/mingw-w64/x86_64-w64-mingw32/bin"
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/*/Redist/MSVC/*/x64/Microsoft.VC*.CRT"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/*/Redist/MSVC/*/x64/Microsoft.VC*.CRT"
        "C:/Program Files/Microsoft Visual Studio/2019/*/Redist/MSVC/*/x64/Microsoft.VC*.CRT"
        "C:/Program Files/Microsoft Visual Studio/2022/*/Redist/MSVC/*/x64/Microsoft.VC*.CRT"
        "$ENV{SystemRoot}/System32"
    )
    
    # Also search in PATH
    if(DEFINED ENV{PATH})
        string(REPLACE ";" ";" PATH_DIRS "$ENV{PATH}")
        list(APPEND DLL_SEARCH_PATHS ${PATH_DIRS})
    endif()
    
    # Add System32 for Visual C++ runtime
    if(DEFINED ENV{SystemRoot})
        list(APPEND DLL_SEARCH_PATHS "$ENV{SystemRoot}/System32")
    endif()
    
    foreach(dll_name ${REQUIRED_DLLS})
        set(dll_found FALSE)
        foreach(search_path ${DLL_SEARCH_PATHS})
            # Normalize path to use forward slashes
            string(REPLACE "\\" "/" normalized_search_path "${search_path}")
            if(EXISTS "${normalized_search_path}/${dll_name}")
                # Install to ROOT directory for DLL loading by executable
                install(FILES "${normalized_search_path}/${dll_name}"
                    DESTINATION .
                    COMPONENT Runtime
                    OPTIONAL
                )
                # Also install to bin/ subdirectory for consistency with portable structure
                install(FILES "${normalized_search_path}/${dll_name}"
                    DESTINATION bin
                    COMPONENT Runtime
                    OPTIONAL
                )
                set(dll_found TRUE)
                message(STATUS "Found and will install DLL to root and bin/: ${dll_name} from ${normalized_search_path}")
                break()
            endif()
        endforeach()
        if(NOT dll_found)
            message(STATUS "Optional DLL not found: ${dll_name}")
        endif()
    endforeach()
    
    # Create symlinks for OpenBLAS compatibility using separate script
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/openblas-symlinks.cmake.in"
                   "${CMAKE_BINARY_DIR}/openblas-symlinks.cmake" @ONLY)
    install(SCRIPT "${CMAKE_BINARY_DIR}/openblas-symlinks.cmake"
        COMPONENT Runtime
    )
    
    # Install launcher script that adds bin/ to PATH
    # Use a separate CMake file to avoid quoting issues
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/launcher-script-template.bat.in"
                   "${CMAKE_BINARY_DIR}/start-kolosal-server.bat" @ONLY)
    install(FILES "${CMAKE_BINARY_DIR}/start-kolosal-server.bat"
        DESTINATION .
        COMPONENT Runtime
    )
    
    # Install README for installer package
    install(CODE "
        file(WRITE \"\${CMAKE_INSTALL_PREFIX}/README.txt\" 
            \"Kolosal Server Portable Package\\r\\n\"
            \"\\r\\n\"
            \"QUICK START:\\r\\n\"
            \"------------\\r\\n\"
            \"Run start-kolosal-server.bat to start the server.\\r\\n\"
            \"Alternatively, you can run kolosal-server.exe directly from this directory.\\r\\n\"
            \"All required DLL files are now in the same directory as the executable.\\r\\n\"
            \"\\r\\n\"
            \"STRUCTURE:\\r\\n\"
            \"----------\\r\\n\"
            \"kolosal-server.exe - Main executable\\r\\n\"
            \"*.dll          - All required DLL dependencies (in root for direct loading)\\r\\n\"
            \"bin/           - Additional executables and backup DLL copies\\r\\n\"
            \"config/        - Configuration files\\r\\n\"
            \"models/        - Place your model files here\\r\\n\"
            \"logs/          - Server logs will be written here\\r\\n\"
            \"data/          - Runtime data and indexes\\r\\n\"
            \"docs/          - Documentation\\r\\n\"
            \"static/        - Web UI files\\r\\n\"
            \"\\r\\n\"
            \"NOTES:\\r\\n\"
            \"------\\r\\n\"
            \"- All required DLL files are in the root directory for immediate loading\\r\\n\"
            \"- Backup copies of DLLs are also in the bin/ directory\\r\\n\"
            \"- The launcher script is provided for convenience but not required\\r\\n\"
            \"- Multiple inference engines available: CPU and optionally Vulkan/CUDA/Metal\\r\\n\"
            \"- The server automatically selects the best available engine for your hardware\\r\\n\"
            \"\\r\\n\"
        )
        message(STATUS \"Created README.txt for installer package\")
    " COMPONENT Runtime)
    
    # NSIS Configuration (only used if NSIS is available)
    set(CPACK_NSIS_PACKAGE_NAME "Kolosal Server")
    set(CPACK_NSIS_DISPLAY_NAME "Kolosal Server ${PROJECT_VERSION}")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "kolosal-server.exe")
    set(CPACK_NSIS_HELP_LINK "https://github.com/KolosalAI/kolosal-server")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/KolosalAI/kolosal-server")
    set(CPACK_NSIS_CONTACT "support@kolosalai.com")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    
    # Create start menu shortcuts
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Kolosal Server.lnk' '$INSTDIR\\\\kolosal-server.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Kolosal Server.lnk'"
    )
    
    # Additional NSIS commands - create directories matching ZIP structure
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        CreateDirectory '$INSTDIR\\\\bin'
        CreateDirectory '$INSTDIR\\\\logs'
        CreateDirectory '$INSTDIR\\\\data'
        CreateDirectory '$INSTDIR\\\\data\\\\faiss_index'
        CreateDirectory '$INSTDIR\\\\models'
        CreateDirectory '$INSTDIR\\\\config'
        CreateDirectory '$INSTDIR\\\\docs'
        CreateDirectory '$INSTDIR\\\\static'
    ")
    
    # WiX Configuration (only used if WiX is available)
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/assets/logo.png")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Kolosal Server")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/KolosalAI/kolosal-server")
    
elseif(APPLE)
    # Bundle and DMG for macOS
    set(CPACK_GENERATOR "Bundle;DragNDrop")
    set(CPACK_BUNDLE_NAME "Kolosal Server")
    set(CPACK_BUNDLE_PLIST "${CMAKE_SOURCE_DIR}/installer/Info.plist")
    set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/assets/icon.icns")
    set(CPACK_DMG_VOLUME_NAME "Kolosal Server ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/assets/dmg_background.png")
    
elseif(UNIX)
    # DEB and RPM for Linux
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    
    # Debian package configuration
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kolosal AI <support@kolosalai.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcurl4, libc6 (>= 2.27)")
    set(CPACK_DEBIAN_PACKAGE_SUGGESTS "nvidia-cuda-toolkit")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/KolosalAI/kolosal-server")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    
    # RPM package configuration
    set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/KolosalAI/kolosal-server")
    set(CPACK_RPM_PACKAGE_REQUIRES "libcurl")
    set(CPACK_RPM_PACKAGE_SUGGESTS "cuda")
    set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
    
    # Post-install script for Linux
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
        "${CMAKE_SOURCE_DIR}/installer/postinst;${CMAKE_SOURCE_DIR}/installer/prerm")
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/installer/postinst")
    set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/installer/prerm")
endif()

# Source package configuration
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\\\.git/"
    "/\\\\.github/"
    "/\\\\.vscode/"
    "/build/"
    "/dist/"
    "\\\\.gitignore"
    "\\\\.DS_Store"
    ".*~$"
)

# Include CPack module
include(CPack)