cmake_minimum_required(VERSION 3.14)
project(KolosalServer VERSION 1.0.0 LANGUAGES CXX)

# Use C++17 for this project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# On Windows, define _WIN32_WINNT to ensure compatibility.
if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Define include directories.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann)

# Define source files.
set(SOURCES
    src/server.cpp
    src/server_api.cpp
    src/logger.cpp
    src/routes/chat_completion_route.cpp
)

# Create shared library instead of static library.
add_library(kolosal_server SHARED ${SOURCES})
target_compile_definitions(kolosal_server PRIVATE KOLOSAL_SERVER_BUILD)

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
    
    find_package(Threads REQUIRED)
    target_link_libraries(kolosal_example PRIVATE Threads::Threads)
endif()

# Find and link thread library for the main library.
find_package(Threads REQUIRED)
target_link_libraries(kolosal_server PRIVATE Threads::Threads)

# Platform-specific link libraries for the main library.
if(WIN32)
    target_link_libraries(kolosal_server PRIVATE ws2_32)
endif()
