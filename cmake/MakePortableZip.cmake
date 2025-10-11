# MakePortableZip.cmake - Creates a portable ZIP package with all dependencies

# Arguments (passed via -D):
# EXECUTABLE_PATH, LIB_SERVER_PATH, LIB_INFERENCE_PATH, SOURCE_DIR, ZIP_DIR, ZIP_FILE, SOURCE_ROOT, PROJECT_VERSION

if(NOT DEFINED ZIP_DIR OR NOT DEFINED ZIP_FILE)
    message(FATAL_ERROR "ZIP_DIR and ZIP_FILE must be set")
endif()

# Clean and create staging directory with kolosal-server root
file(REMOVE_RECURSE "${ZIP_DIR}")
file(MAKE_DIRECTORY "${ZIP_DIR}")

# Create kolosal-server root directory inside the staging area
set(ROOT_DIR "${ZIP_DIR}/kolosal-server")
file(MAKE_DIRECTORY "${ROOT_DIR}")
file(MAKE_DIRECTORY "${ROOT_DIR}/config")
file(MAKE_DIRECTORY "${ROOT_DIR}/data/faiss_index")
file(MAKE_DIRECTORY "${ROOT_DIR}/logs")
file(MAKE_DIRECTORY "${ROOT_DIR}/models")
file(MAKE_DIRECTORY "${ROOT_DIR}/docs")
file(MAKE_DIRECTORY "${ROOT_DIR}/static")

# Copy main executable to kolosal-server root directory
message(STATUS "Copying main artifacts...")
if(DEFINED EXECUTABLE_PATH AND EXISTS "${EXECUTABLE_PATH}")
    file(COPY "${EXECUTABLE_PATH}" DESTINATION "${ROOT_DIR}")
    message(STATUS "  Copied main executable to root: ${EXECUTABLE_PATH}")
endif()

# Copy libraries (DLLs) to root directory
foreach(_f IN ITEMS LIB_SERVER_PATH LIB_INFERENCE_PATH)
    if(DEFINED ${_f} AND EXISTS "${${_f}}")
        file(COPY "${${_f}}" DESTINATION "${ROOT_DIR}")
        message(STATUS "  Copied: ${${_f}}")
    endif()
endforeach()

# Copy all executable files from SOURCE_DIR (test executables, examples, etc.)
message(STATUS "Copying all executables from build directory...")
file(GLOB _ALL_EXES "${SOURCE_DIR}/*.exe")
foreach(_exe IN LISTS _ALL_EXES)
    get_filename_component(_exe_name "${_exe}" NAME)
    # Skip if already copied (all executables go to root)
    if(NOT EXISTS "${ROOT_DIR}/${_exe_name}")
        file(COPY "${_exe}" DESTINATION "${ROOT_DIR}")
        message(STATUS "  Copied executable: ${_exe_name}")
    endif()
endforeach()

# Copy all DLL files from SOURCE_DIR to root directory
message(STATUS "Copying all DLLs from build directory...")
file(GLOB _ALL_DLLS "${SOURCE_DIR}/*.dll")
foreach(_dll IN LISTS _ALL_DLLS)
    get_filename_component(_dll_name "${_dll}" NAME)
    # Skip if already copied
    if(NOT EXISTS "${ROOT_DIR}/${_dll_name}")
        file(COPY "${_dll}" DESTINATION "${ROOT_DIR}")
        message(STATUS "  Copied DLL: ${_dll_name}")
    endif()
endforeach()

# Search for required DLLs in common locations and copy to root
set(_REQUIRED_DLLS libgfortran-5.dll libquadmath-0.dll libgcc_s_seh-1.dll libwinpthread-1.dll libgomp-1.dll libopenblas.dll)
set(_SEARCH_PATHS "C:/msys64/mingw64/bin" "C:/msys64/ucrt64/bin")

foreach(_dll IN LISTS _REQUIRED_DLLS)
    if(NOT EXISTS "${ROOT_DIR}/${_dll}")
        foreach(_path IN LISTS _SEARCH_PATHS)
            if(EXISTS "${_path}/${_dll}")
                file(COPY "${_path}/${_dll}" DESTINATION "${ROOT_DIR}")
                message(STATUS "Found and copied: ${_dll} from ${_path}")
                break()
            endif()
        endforeach()
    endif()
endforeach()

# Copy configuration files
if(EXISTS "${SOURCE_ROOT}/configs")
    file(GLOB _CONFIGS "${SOURCE_ROOT}/configs/*.yaml" "${SOURCE_ROOT}/configs/*.json")
    foreach(_cfg IN LISTS _CONFIGS)
        file(COPY "${_cfg}" DESTINATION "${ROOT_DIR}/config")
    endforeach()
endif()

# Copy documentation
foreach(_doc IN ITEMS README.md LICENSE)
    if(EXISTS "${SOURCE_ROOT}/${_doc}")
        file(COPY "${SOURCE_ROOT}/${_doc}" DESTINATION "${ROOT_DIR}")
    endif()
endforeach()

# Create README
file(WRITE "${ROOT_DIR}/README.txt" "Kolosal Server Portable Package\n\nRun kolosal-server.exe from this directory to start the server.\nAll required DLL files are included in this directory.\n\nConfiguration files are in the config/ directory.\nModels should be placed in the models/ directory.\nLogs will be written to the logs/ directory.\n")

# Create ZIP file
message(STATUS "Creating ZIP archive: ${ZIP_FILE}")
file(REMOVE "${ZIP_FILE}")

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    file(ARCHIVE_CREATE OUTPUT "${ZIP_FILE}" PATHS "${ZIP_DIR}" FORMAT zip)
else()
    find_program(POWERSHELL_EXEC pwsh powershell)
    if(POWERSHELL_EXEC)
        execute_process(COMMAND ${POWERSHELL_EXEC} -Command "Compress-Archive -Path '${ZIP_DIR}/*' -DestinationPath '${ZIP_FILE}' -Force")
    endif()
endif()

if(EXISTS "${ZIP_FILE}")
    message(STATUS "Successfully created portable package: ${ZIP_FILE}")
endif()
