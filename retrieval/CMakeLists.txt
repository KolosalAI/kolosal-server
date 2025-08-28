cmake_minimum_required(VERSION 3.14)
project(RetrievalEngine VERSION 1.0.0 LANGUAGES CXX)

# Kolosal Retrieval Module (static library)
#
# This builds all retrieval-related functionality outside of src/ into a
# separate module, similar to the inference layout. Routes remain in src/.

# Include UCM for consistency with the root project
include(${CMAKE_SOURCE_DIR}/cmake/ucm.cmake)
ucm_set_runtime(DYNAMIC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|ARM64|arm64|armv8|armv7)")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Options are inherited from the parent (USE_PODOFO, USE_FAISS)

set(RETRIEVAL_SOURCES
    src/document_service.cpp
    src/add_document_types.cpp
    src/remove_document_types.cpp
    src/document_list_types.cpp
    src/retrieve_types.cpp
    src/chunking_types.cpp
    src/parse_docx.cpp
    src/parse_html.cpp
    src/qdrant_client.cpp
    src/faiss_client.cpp
)

# PoDoFo parser or stub
if(USE_PODOFO)
    list(APPEND RETRIEVAL_SOURCES src/parse_pdf.cpp)
else()
    list(APPEND RETRIEVAL_SOURCES src/parse_pdf_stub.cpp)
endif()

add_library(kolosal-retrieval STATIC ${RETRIEVAL_SOURCES})
target_compile_definitions(kolosal-retrieval PUBLIC KOLOSAL_SERVER_BUILD)

# Public headers live under ${CMAKE_SOURCE_DIR}/include; external headers are
# already added globally in the parent CMake via include_directories().
target_include_directories(kolosal-retrieval PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Also install public headers from retrieval/include when INSTALL_HEADERS is enabled at root
if(INSTALL_HEADERS)
    install(DIRECTORY include/
        DESTINATION include/kolosal/retrieval
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()

# Thread library for non-Windows platforms (harmless elsewhere)
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(kolosal-retrieval PUBLIC Threads::Threads)
endif()

# Consumers (kolosal_server) already link CURL, pugixml, minizip_static, podofo, faiss
# in the root CMake. This library is STATIC, so we don't duplicate those here.

ucm_print_flags()
