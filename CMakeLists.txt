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
    src/routes/embedding_route.cpp
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
    src/routes/cancel_all_downloads_route.cpp    src/routes/parse_pdf_route.cpp
    src/routes/parse_docx_route.cpp
    src/routes/add_documents_route.cpp
    src/routes/models_route.cpp
    src/metrics_converter.cpp
    src/auth/rate_limiter.cpp
    src/auth/cors_handler.cpp
    src/auth/auth_middleware.cpp    src/node_manager.cpp    src/inference.cpp
    src/models/embedding_request_model.cpp
    src/models/embedding_response_model.cpp
    src/models/add_documents_model.cpp
    src/qdrant_client.cpp
    src/document_service.cpp
    src/retrieval/parse_pdf.cpp
    src/retrieval/parse_docx.cpp
    # Minizip sources for DOCX parsing
    external/mupdf/thirdparty/zlib/contrib/minizip/unzip.c
    external/mupdf/thirdparty/zlib/contrib/minizip/ioapi.c
    external/mupdf/thirdparty/zlib/contrib/minizip/iowin32.c
)

# Create shared library instead of static library.
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD INFERENCE_EXPORTS)

# Force Release build to match MuPDF (prebuilt Release library)
if(WIN32 AND MSVC)
    # MuPDF is only available as Release build, so force Release mode
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
    message(STATUS "Forcing Release build to match MuPDF prebuilt library")
endif()

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

# Link libraries
set(KOLOSAL_LINK_LIBRARIES llama common ggml toolcall yaml-cpp pugixml)

# Add cppuprofile if enabled
if(USE_CPPUPROFILE_GPU)
    list(APPEND KOLOSAL_LINK_LIBRARIES cppuprofile)
endif()

target_link_libraries(kolosal_server PRIVATE ${KOLOSAL_LINK_LIBRARIES})

# Link CURL libraries and include directories
target_include_directories(kolosal_server PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(kolosal_server PRIVATE ${CURL_LIBRARIES})

# Configure MuPDF for the main server
set(MUPDF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/mupdf")
target_include_directories(kolosal_server PRIVATE 
    ${MUPDF_PATH}/include
    ${MUPDF_PATH}/thirdparty/zlib/contrib/minizip
    ${MUPDF_PATH}/thirdparty/zlib
)

# Configure pugixml
set(PUGIXML_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/pugixml")
if(NOT EXISTS "${PUGIXML_PATH}/CMakeLists.txt")
  message(FATAL_ERROR "pugixml not found at ${PUGIXML_PATH}. Please clone it or adjust PUGIXML_PATH.")
endif()

# Configure pugixml options
set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "Disable pugixml tests" FORCE)
set(PUGIXML_INSTALL OFF CACHE BOOL "Disable pugixml installation" FORCE)

# Add pugixml subdirectory
add_subdirectory(${PUGIXML_PATH})

# Include pugixml headers
target_include_directories(kolosal_server PRIVATE ${PUGIXML_PATH}/src)

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

# Include llama.cpp directories
target_include_directories(kolosal_server PUBLIC
  ${LLAMA_CPP_PATH}/include
  ${LLAMA_CPP_PATH}/common
  ${LLAMA_CPP_PATH}/ggml/include
  ${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann
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
    # For Linux, we might need additional libraries for system monitoring
    target_link_libraries(kolosal_server PRIVATE)
endif()

# Try to find and link NVML for GPU monitoring
find_library(NVML_LIBRARY nvidia-ml 
    HINTS 
    "C:/Program Files/NVIDIA Corporation/NVSMI"
    "C:/Windows/System32"
    "/usr/lib/x86_64-linux-gnu"
    "/usr/lib64"
    "/usr/local/cuda/lib64"
    NO_DEFAULT_PATH
)

if(NVML_LIBRARY)
    target_link_libraries(kolosal_server PRIVATE ${NVML_LIBRARY})
    message(STATUS "Found NVML: ${NVML_LIBRARY}")
    target_compile_definitions(kolosal_server PRIVATE NVML_AVAILABLE)
else()
    message(STATUS "NVML not found - GPU monitoring will be limited")
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