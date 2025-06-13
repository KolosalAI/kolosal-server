cmake_minimum_required(VERSION 3.14)
project(KolosalServer VERSION 1.0.0 LANGUAGES CXX)

# Use C++17 for this project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# On Windows, define _WIN32_WINNT to ensure compatibility.
if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Options for inference engine
option(USE_CUDA   "Compile with CUDA support" OFF)
option(USE_VULKAN "Compile with VULKAN support" OFF)
option(DEBUG      "Compile with debugging information" OFF)

# Define include directories.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann)

# Define source files.
set(SOURCES
    src/server.cpp
    src/server_api.cpp
    src/logger.cpp
    src/routes/chat_completion_route.cpp
    src/routes/completion_route.cpp
    src/routes/models_route.cpp
    src/inference.cpp
)

# Create shared library instead of static library.
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD INFERENCE_EXPORTS)

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

# Define inference-related compile definitions
target_compile_definitions(kolosal_server PUBLIC
  $<$<BOOL:${USE_CUDA}>:USE_CUDA>
  $<$<BOOL:${USE_VULKAN}>:USE_VULKAN>
  $<$<BOOL:${DEBUG}>:DEBUG>
)

# Add llama.cpp subdirectory
add_subdirectory(${LLAMA_CPP_PATH})

# Link llama.cpp libraries
target_link_libraries(kolosal_server PRIVATE llama common ggml toolcall)

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
install(TARGETS kolosal_server
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install header files.
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Optional: Create example application.
option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
    add_executable(kolosal_example examples/simple.cpp)
    target_link_libraries(kolosal_example PRIVATE kolosal_server)
    
    # Platform-specific link libraries.
    if(WIN32)
        target_link_libraries(kolosal_example PRIVATE ws2_32)
    endif()
    
    target_link_libraries(kolosal_example PRIVATE Threads::Threads)
endif()
