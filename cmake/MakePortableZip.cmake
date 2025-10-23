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
file(MAKE_DIRECTORY "${ROOT_DIR}/bin")
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

# First, copy all DLL files from SOURCE_DIR to bin directory
message(STATUS "Copying all DLLs from build directory to bin...")
file(GLOB _ALL_DLLS "${SOURCE_DIR}/*.dll")
if(NOT _ALL_DLLS)
    message(STATUS "  No DLLs found in ${SOURCE_DIR}, this is expected for non-Windows or if DLLs are in a subdirectory")
endif()
foreach(_dll IN LISTS _ALL_DLLS)
    get_filename_component(_dll_name "${_dll}" NAME)
    file(COPY "${_dll}" DESTINATION "${ROOT_DIR}/bin")
    message(STATUS "  Copied DLL to bin: ${_dll_name}")
endforeach()

# Copy libraries (DLLs) to bin directory
foreach(_f IN ITEMS LIB_SERVER_PATH LIB_INFERENCE_PATH)
    if(DEFINED ${_f} AND EXISTS "${${_f}}")
        file(COPY "${${_f}}" DESTINATION "${ROOT_DIR}/bin")
        message(STATUS "  Copied to bin: ${${_f}}")
    endif()
endforeach()

# CRITICAL: Also copy essential DLLs to root directory for direct .exe execution
# This allows kolosal-server.exe to run without the launcher script
message(STATUS "Copying critical DLLs to root directory for direct execution...")

# Copy kolosal_server.dll to root
if(DEFINED LIB_SERVER_PATH AND EXISTS "${LIB_SERVER_PATH}")
    file(COPY "${LIB_SERVER_PATH}" DESTINATION "${ROOT_DIR}")
    get_filename_component(_lib_name "${LIB_SERVER_PATH}" NAME)
    message(STATUS "  Copied ${_lib_name} to root from ${LIB_SERVER_PATH}")
else()
    message(WARNING "  LIB_SERVER_PATH not found: ${LIB_SERVER_PATH}")
endif()

# Copy all inference engine DLLs to root (llama-cpu.dll, llama-vulkan.dll, etc.)
file(GLOB _INFERENCE_DLLS "${SOURCE_DIR}/llama-*.dll")
if(_INFERENCE_DLLS)
    foreach(_dll IN LISTS _INFERENCE_DLLS)
        get_filename_component(_dll_name "${_dll}" NAME)
        file(COPY "${_dll}" DESTINATION "${ROOT_DIR}")
        message(STATUS "  Copied ${_dll_name} to root")
    endforeach()
else()
    message(STATUS "  No inference DLLs (llama-*.dll) found in ${SOURCE_DIR}")
endif()

# Copy all runtime DLLs from SOURCE_DIR to root (for direct execution)
if(_ALL_DLLS)
    foreach(_dll IN LISTS _ALL_DLLS)
        get_filename_component(_dll_name "${_dll}" NAME)
        # Skip the main DLLs we already copied explicitly
        if(NOT _dll_name MATCHES "^(kolosal_server|llama-.*)\\.dll$")
            file(COPY "${_dll}" DESTINATION "${ROOT_DIR}")
            message(STATUS "  Copied runtime DLL to root: ${_dll_name}")
        endif()
    endforeach()
else()
    message(STATUS "  No additional runtime DLLs found to copy to root")
endif()

# Copy all executable files from SOURCE_DIR (test executables, examples, etc.) to bin
message(STATUS "Copying all executables from build directory...")
file(GLOB _ALL_EXES "${SOURCE_DIR}/*.exe")
foreach(_exe IN LISTS _ALL_EXES)
    get_filename_component(_exe_name "${_exe}" NAME)
    # Main executable goes to root, others to bin
    if(_exe_name STREQUAL "kolosal-server.exe")
        if(NOT EXISTS "${ROOT_DIR}/${_exe_name}")
            file(COPY "${_exe}" DESTINATION "${ROOT_DIR}")
            message(STATUS "  Copied executable to root: ${_exe_name}")
        endif()
    else()
        if(NOT EXISTS "${ROOT_DIR}/bin/${_exe_name}")
            file(COPY "${_exe}" DESTINATION "${ROOT_DIR}/bin")
            message(STATUS "  Copied executable to bin: ${_exe_name}")
        endif()
    endif()
endforeach()

# Search for required DLLs in common locations and copy to both bin and root
set(_REQUIRED_DLLS libgfortran-5.dll libquadmath-0.dll libgcc_s_seh-1.dll libwinpthread-1.dll libgomp-1.dll libopenblas.dll)
set(_SEARCH_PATHS "C:/msys64/mingw64/bin" "C:/msys64/ucrt64/bin")

foreach(_dll IN LISTS _REQUIRED_DLLS)
    if(NOT EXISTS "${ROOT_DIR}/bin/${_dll}")
        foreach(_path IN LISTS _SEARCH_PATHS)
            if(EXISTS "${_path}/${_dll}")
                file(COPY "${_path}/${_dll}" DESTINATION "${ROOT_DIR}/bin")
                message(STATUS "Found and copied to bin: ${_dll} from ${_path}")
                # Also copy to root for direct execution
                file(COPY "${_path}/${_dll}" DESTINATION "${ROOT_DIR}")
                message(STATUS "Found and copied to root: ${_dll} from ${_path}")
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

# Copy documentation directory
if(EXISTS "${SOURCE_ROOT}/docs")
    file(COPY "${SOURCE_ROOT}/docs" DESTINATION "${ROOT_DIR}")
endif()

# Copy static web files
if(EXISTS "${SOURCE_ROOT}/static")
    file(COPY "${SOURCE_ROOT}/static" DESTINATION "${ROOT_DIR}")
endif()

# Create a launcher script that adds bin to PATH
file(WRITE "${ROOT_DIR}/start-kolosal-server.bat" 
    "@echo off\r\n"
    "REM Kolosal Server Launcher\r\n"
    "REM This script ensures DLLs in the bin folder are found\r\n"
    "\r\n"
    "set \"SCRIPT_DIR=%~dp0\"\r\n"
    "set \"PATH=%SCRIPT_DIR%bin;%PATH%\"\r\n"
    "\r\n"
    "echo Starting Kolosal Server...\r\n"
    "echo DLL search path: %SCRIPT_DIR%bin\r\n"
    "\r\n"
    "\"%SCRIPT_DIR%kolosal-server.exe\" %*\r\n"
)

# Create README
file(WRITE "${ROOT_DIR}/README.txt" 
    "Kolosal Server Portable Package\r\n"
    "\r\n"
    "QUICK START:\r\n"
    "------------\r\n"
    "Simply run kolosal-server.exe directly!\r\n"
    "Alternatively, use start-kolosal-server.bat.\r\n"
    "\r\n"
    "STRUCTURE:\r\n"
    "----------\r\n"
    "kolosal-server.exe    - Main executable\r\n"
    "*.dll                 - All required DLLs in root for easy access\r\n"
    "bin/                  - Backup copy of DLL dependencies\r\n"
    "config/               - Configuration files\r\n"
    "models/               - Place your model files here\r\n"
    "logs/                 - Server logs will be written here\r\n"
    "data/                 - Runtime data and indexes\r\n"
    "docs/                 - Documentation\r\n"
    "static/               - Web UI files\r\n"
    "\r\n"
    "NOTES:\r\n"
    "------\r\n"
    "- All required DLL files are in the root directory alongside the executable\r\n"
    "- Backup copies are also available in the bin/ directory\r\n"
    "- The server works out of the box - just run kolosal-server.exe\r\n"
    "- Multiple inference engines available: CPU and optionally Vulkan/CUDA/Metal\r\n"
    "- The server automatically selects the best available engine for your hardware\r\n"
    "\r\n"
)

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
