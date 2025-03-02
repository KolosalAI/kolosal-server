cmake_minimum_required(VERSION 3.10)
project(KolosalServer)

# Use C++11 for this project.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# On Windows, define _WIN32_WINNT to ensure compatibility.
if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Include directories.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann)

# Add the executable.
add_executable(KolosalServer
    src/server_main.cpp
    src/server.cpp
    src/routes/greet_route.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(KolosalServer PRIVATE Threads::Threads)