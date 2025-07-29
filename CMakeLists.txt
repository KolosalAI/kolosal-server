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
option(USE_METAL  "Compile with Metal support (macOS only)" OFF)
option(USE_MPI    "Compile with MPI support" OFF)
option(DEBUG      "Compile with debugging information" OFF)
option(USE_PODOFO "Compile with PoDoFo PDF support" ON)

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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/contrib/minizip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/maddy/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/zlib)

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
    src/routes/oai_completions_route.cpp
    src/routes/completion_route.cpp
    src/routes/models_route.cpp
    src/routes/engines_route.cpp
    src/routes/health_status_route.cpp    
    src/routes/auth_config_route.cpp    
    src/routes/server_logs_route.cpp    
    src/routes/embedding_route.cpp
    src/routes/internet_search_route.cpp
    src/routes/parse_document_route.cpp
    src/routes/retrieve_route.cpp
    src/routes/documents_route.cpp
    src/routes/downloads_route.cpp
    src/routes/chunking_route.cpp
    
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

# Copy necessary libraries to the executable directory
if(WIN32)
    # Copy the main server DLL
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:kolosal_server>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying kolosal_server.dll to executable directory"
    )
elseif(APPLE)
    # Copy the main server dylib for macOS
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:kolosal_server>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying libkolosal_server.dylib to executable directory"
    )
    
    # Fix the install name of the dylib to use @rpath
    add_custom_command(TARGET kolosal_server POST_BUILD
        COMMAND install_name_tool -id "@rpath/libkolosal_server.dylib" "$<TARGET_FILE:kolosal_server>"
        COMMENT "Setting install name for libkolosal_server.dylib"
    )
endif()

# Copy additional libraries for Windows
if(WIN32)
    # Copy inference engine DLL to executable directory
    add_custom_command(TARGET kolosal_server_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:llama-cpu>"
        "$<TARGET_FILE_DIR:kolosal_server_exe>/"
        COMMENT "Copying llama-cpu.dll to executable directory"
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

# Add zlib (disable minizip for now due to config issues)
if(NOT TARGET zlibstatic)
    # Configure zlib options
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable zlib tests" FORCE)
    set(ZLIB_BUILD_MINIZIP OFF CACHE BOOL "Disable building libminizip contrib library" FORCE)
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Build zlib as static library" FORCE)
    set(ZLIB_BUILD_STATIC ON CACHE BOOL "Build zlib static library" FORCE)
    
    # Set ZLIB installation to OFF since we're building in-tree
    set(ZLIB_INSTALL OFF CACHE BOOL "Disable zlib installation" FORCE)
    
    add_subdirectory(external/zlib EXCLUDE_FROM_ALL)
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
    target_link_libraries(minizip_static PUBLIC zlibstatic)
    
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
if(TARGET zlibstatic)
    # Set ZLIB variables so find_package(ZLIB) works for dependencies
    get_target_property(ZLIB_INCLUDE_DIR zlibstatic INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT ZLIB_INCLUDE_DIR)
        set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib;${CMAKE_CURRENT_BINARY_DIR}/external/zlib")
    endif()
    set(ZLIB_LIBRARY zlibstatic)
    set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
    set(ZLIB_FOUND TRUE)
    set(ZLIB_VERSION_STRING "1.2.11") # Adjust if needed
    
    # Also set for the ZLIB target to be found
    set_target_properties(zlibstatic PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
    )
    
    # Create ZLIB::ZLIB imported target for modern CMake
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
    
    message(STATUS "ZLIB variables set for PoDoFo: ${ZLIB_INCLUDE_DIR}")
endif()

# Add PoDoFo
if(USE_PODOFO AND NOT TARGET podofo_shared AND NOT TARGET podofo_static)
    # Check if PoDoFo has its own CMakeLists.txt
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/podofo/CMakeLists.txt")
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
            set(PODOFO_LIB_SUBDIR "macosx-x86_64")
        else()
            set(PODOFO_LIB_SUBDIR "linux-x86_64")
        endif()
        
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
        if(NOT TARGET Freetype::Freetype)
            add_library(Freetype::Freetype STATIC IMPORTED)
            set_target_properties(Freetype::Freetype PROPERTIES
                IMPORTED_LOCATION ${FREETYPE_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_INCLUDE_DIRS}"
            )
        endif()
        
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
        if(NOT TARGET JPEG::JPEG)
            add_library(JPEG::JPEG STATIC IMPORTED)
            set_target_properties(JPEG::JPEG PROPERTIES
                IMPORTED_LOCATION ${JPEG_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIR}"
            )
        endif()
        
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
        if(NOT TARGET PNG::PNG)
            add_library(PNG::PNG STATIC IMPORTED)
            set_target_properties(PNG::PNG PROPERTIES
                IMPORTED_LOCATION ${PNG_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES "${PNG_INCLUDE_DIR}"
            )
        endif()
        
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
        if(NOT TARGET TIFF::TIFF)
            add_library(TIFF::TIFF STATIC IMPORTED)
            set_target_properties(TIFF::TIFF PROPERTIES
                IMPORTED_LOCATION ${TIFF_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES "${TIFF_INCLUDE_DIR}"
            )
        endif()
        
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
        if(NOT TARGET LibXml2::LibXml2)
            add_library(LibXml2::LibXml2 STATIC IMPORTED)
            set_target_properties(LibXml2::LibXml2 PROPERTIES
                IMPORTED_LOCATION ${LIBXML2_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIR}"
            )
        endif()
        
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
        if(NOT TARGET Fontconfig::Fontconfig)
            add_library(Fontconfig::Fontconfig STATIC IMPORTED)
            set_target_properties(Fontconfig::Fontconfig PROPERTIES
                IMPORTED_LOCATION ${Fontconfig_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES "${Fontconfig_INCLUDE_DIR}"
            )
        endif()
        
        # Set found flags
        if(EXISTS "${FREETYPE_LIBRARY}")
            set(FREETYPE_FOUND TRUE)
            message(STATUS "Found PoDoFo Freetype: ${FREETYPE_LIBRARY}")
        endif()
        
        if(EXISTS "${JPEG_LIBRARY}")
            set(JPEG_FOUND TRUE)
            message(STATUS "Found PoDoFo JPEG: ${JPEG_LIBRARY}")
        endif()
        
        if(EXISTS "${PNG_LIBRARY}")
            set(PNG_FOUND TRUE)
            message(STATUS "Found PoDoFo PNG: ${PNG_LIBRARY}")
        endif()
        
        if(EXISTS "${TIFF_LIBRARY}")
            set(TIFF_FOUND TRUE)
            message(STATUS "Found PoDoFo TIFF: ${TIFF_LIBRARY}")
        endif()
        
        if(EXISTS "${LIBXML2_LIBRARY}")
            set(LIBXML2_FOUND TRUE)
            message(STATUS "Found PoDoFo LibXml2: ${LIBXML2_LIBRARY}")
        endif()
        
        if(EXISTS "${Fontconfig_LIBRARY}")
            set(Fontconfig_FOUND TRUE)
            message(STATUS "Found PoDoFo Fontconfig: ${Fontconfig_LIBRARY}")
        endif()
        
        # Configure PoDoFo options
        set(PODOFO_BUILD_STATIC ON CACHE BOOL "Build PoDoFo as static library" FORCE)
        set(PODOFO_BUILD_TEST OFF CACHE BOOL "Disable PoDoFo tests" FORCE)
        set(PODOFO_BUILD_EXAMPLES OFF CACHE BOOL "Disable PoDoFo examples" FORCE)
        set(PODOFO_BUILD_UNSUPPORTED_TOOLS OFF CACHE BOOL "Disable PoDoFo tools" FORCE)
        set(PODOFO_BUILD_LIB_ONLY ON CACHE BOOL "Build only PoDoFo library" FORCE)
        
        # Disable installation and exports to avoid target export issues
        set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Skip install rules for PoDoFo" FORCE)
        set(PODOFO_BUILD_INSTALL OFF CACHE BOOL "Disable PoDoFo installation" FORCE)
        
        # Temporarily override add_custom_target to ignore uninstall target
        function(add_custom_target target_name)
            if(target_name STREQUAL "uninstall")
                message(STATUS "Skipping duplicate uninstall target from PoDoFo")
            else()
                # Call the original add_custom_target
                _add_custom_target(${target_name} ${ARGN})
            endif()
        endfunction()
        
        # Temporarily override install function to prevent export issues
        function(install)
            # Ignore all install commands from PoDoFo to avoid export conflicts
            message(STATUS "Skipping install command from PoDoFo: ${ARGN}")
        endfunction()
        
        add_subdirectory(external/podofo EXCLUDE_FROM_ALL)
        
        # Restore original functions by unsetting our overrides
        unset(add_custom_target)
        unset(install)
        
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

# Link libraries - llama components are handled by inference library
set(KOLOSAL_LINK_LIBRARIES yaml-cpp llama-cpu zlibstatic minizip_static pugixml)

# Add PoDoFo if available and enabled
if(USE_PODOFO AND TARGET podofo_static)
    list(APPEND KOLOSAL_LINK_LIBRARIES podofo_static)
    message(STATUS "PoDoFo library will be linked")
endif()

# Add cppuprofile if enabled
if(USE_CPPUPROFILE_GPU)
    list(APPEND KOLOSAL_LINK_LIBRARIES cppuprofile)
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

# On Apple systems, enable Metal by default
if(APPLE AND NOT DEFINED USE_METAL)
    message(STATUS "Apple system detected - enabling Metal acceleration by default")
    set(USE_METAL ON CACHE BOOL "Enable Metal acceleration on Apple systems" FORCE)
endif()

# Note: inference subdirectory is already added earlier in the CMakeLists.txt

# Install header files only for development packages
option(INSTALL_HEADERS "Install header files for development" OFF)
if(INSTALL_HEADERS)
    install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()