# FindOpenBLAS.cmake
# Find OpenBLAS library and headers
#
# This module defines:
#  OpenBLAS_FOUND - system has OpenBLAS
#  OpenBLAS_INCLUDE_DIRS - the OpenBLAS include directories
#  OpenBLAS_LIBRARIES - link these to use OpenBLAS
#  OpenBLAS_DLL - the OpenBLAS DLL file (Windows only)

if(WIN32)
    # Try to find OpenBLAS in common locations
    find_path(OpenBLAS_INCLUDE_DIR
        NAMES cblas.h openblas_config.h
        PATHS
            "$ENV{OPENBLAS_HOME}/include"
            "C:/Program Files/OpenBLAS/include"
            "C:/Program Files (x86)/OpenBLAS/include"
            "C:/openblas/include"
            "${CMAKE_SOURCE_DIR}/external/openblas/include"
        DOC "OpenBLAS include directory"
    )

    find_library(OpenBLAS_LIBRARY
        NAMES openblas libopenblas
        PATHS
            "$ENV{OPENBLAS_HOME}/lib"
            "C:/Program Files/OpenBLAS/lib"
            "C:/Program Files (x86)/OpenBLAS/lib"
            "C:/openblas/lib"
            "${CMAKE_SOURCE_DIR}/external/openblas/lib"
        DOC "OpenBLAS library"
    )

    # Find the DLL file
    find_file(OpenBLAS_DLL
        NAMES openblas.dll libopenblas.dll
        PATHS
            "$ENV{OPENBLAS_HOME}/bin"
            "C:/Program Files/OpenBLAS/bin"
            "C:/Program Files (x86)/OpenBLAS/bin"
            "C:/openblas/bin"
            "${CMAKE_SOURCE_DIR}/external/openblas/bin"
        DOC "OpenBLAS DLL file"
    )
else()
    # Linux/macOS
    find_path(OpenBLAS_INCLUDE_DIR
        NAMES cblas.h openblas_config.h
        PATHS
            /usr/include
            /usr/local/include
            /usr/include/openblas
            /usr/local/opt/openblas/include
            /opt/homebrew/opt/openblas/include
        DOC "OpenBLAS include directory"
    )

    find_library(OpenBLAS_LIBRARY
        NAMES openblas
        PATHS
            /usr/lib
            /usr/local/lib
            /usr/lib/x86_64-linux-gnu
            /usr/local/opt/openblas/lib
            /opt/homebrew/opt/openblas/lib
        DOC "OpenBLAS library"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenBLAS
    REQUIRED_VARS OpenBLAS_LIBRARY OpenBLAS_INCLUDE_DIR
)

if(OpenBLAS_FOUND)
    set(OpenBLAS_LIBRARIES ${OpenBLAS_LIBRARY})
    set(OpenBLAS_INCLUDE_DIRS ${OpenBLAS_INCLUDE_DIR})
    
    if(NOT TARGET OpenBLAS::OpenBLAS)
        add_library(OpenBLAS::OpenBLAS UNKNOWN IMPORTED)
        set_target_properties(OpenBLAS::OpenBLAS PROPERTIES
            IMPORTED_LOCATION "${OpenBLAS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenBLAS_INCLUDE_DIR}"
        )
    endif()
    
    message(STATUS "Found OpenBLAS: ${OpenBLAS_LIBRARY}")
    if(OpenBLAS_DLL)
        message(STATUS "Found OpenBLAS DLL: ${OpenBLAS_DLL}")
    endif()
else()
    message(STATUS "OpenBLAS not found. CPU inference will use default implementation.")
endif()

mark_as_advanced(
    OpenBLAS_INCLUDE_DIR
    OpenBLAS_LIBRARY
    OpenBLAS_DLL
)
