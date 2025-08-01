# Examples for Inference Engine

# Add grammar example
add_executable(grammar_example grammar_example.cpp)

# Add JSON schema example
add_executable(json_schema_example json_schema_example.cpp)

# Link against the inference library
# The target name depends on the acceleration backend
if(TARGET llama-cuda)
    target_link_libraries(grammar_example PRIVATE llama-cuda)
    target_link_libraries(json_schema_example PRIVATE llama-cuda)
elseif(TARGET llama-vulkan)
    target_link_libraries(grammar_example PRIVATE llama-vulkan)
    target_link_libraries(json_schema_example PRIVATE llama-vulkan)
elseif(TARGET llama-metal)
    target_link_libraries(grammar_example PRIVATE llama-metal)
    target_link_libraries(json_schema_example PRIVATE llama-metal)
else()
    target_link_libraries(grammar_example PRIVATE llama-cpu)
    target_link_libraries(json_schema_example PRIVATE llama-cpu)
endif()

# Include directories
target_include_directories(grammar_example PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(json_schema_example PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/../external/nlohmann  # For json.hpp
)

# Set C++ standard
set_target_properties(grammar_example PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
set_target_properties(json_schema_example PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Platform-specific settings
if(MSVC)
    # Use the same runtime library as the main project
    set_target_properties(grammar_example PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
    set_target_properties(json_schema_example PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
endif()

# Install examples
install(TARGETS grammar_example json_schema_example
    RUNTIME DESTINATION bin/examples
)

# Install example sources for reference
install(FILES grammar_example.cpp json_schema_example.cpp README.md
    DESTINATION share/kolosal/examples
)