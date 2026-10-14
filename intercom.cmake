################################################################################
# intercom: deterministic master/slave messaging over a single-wire UART
#
# Project:     intercom - C99 static library for half-duplex UART buses
# Description: Library target definition. Can be included directly by an
#              integrating project: include(path/to/intercom.cmake)
#              INTERCOM_CONFIG_DIR must point to the folder with the
#              application's IntercomConfig.h before this file is included.
#
# Author:      Anton Vynohradov
# Copyright:   (c) 2026 Anton Vynohradov
#
# References:
#   - Integration guide: docs/getting-started.md
#   - CMake Documentation: https://cmake.org/documentation/
#
# Last Updated: October 2026
#
################################################################################

# ==============================================================================
# CMAKE CONFIGURATION
# ==============================================================================

list(APPEND CMAKE_MODULE_PATH

    ${CMAKE_CURRENT_LIST_DIR}/cmake
)

# ==============================================================================
# VERSION EXTRACTION
# ==============================================================================

include(version) # Include the version extraction and parsing logic

# ==============================================================================
# USER CONFIGURATION
# ==============================================================================

# Folder that contains the application's IntercomConfig.h (-DINTERCOM_CONFIG_DIR=...
# or set(INTERCOM_CONFIG_DIR ...) in the integrating project)
if(NOT INTERCOM_CONFIG_DIR OR NOT EXISTS "${INTERCOM_CONFIG_DIR}/IntercomConfig.h")
    message(FATAL_ERROR
        "intercom: set INTERCOM_CONFIG_DIR to the folder with your IntercomConfig.h "
        "(copy config/IntercomConfig_template.h there). Current value: '${INTERCOM_CONFIG_DIR}'")
endif()

# ==============================================================================
# INCLUDE DIRECTORIES
# ==============================================================================

# Private headers in src/ are included relatively by the .c files and are not
# exported to the application.
list(APPEND INTERCOM_HEADERS

    ${CMAKE_CURRENT_LIST_DIR}/include
    ${INTERCOM_CONFIG_DIR}
)

# ==============================================================================
# SOURCE FILES
# ==============================================================================

list(APPEND INTERCOM_SOURCES

    ${CMAKE_CURRENT_LIST_DIR}/src/Intercom.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ic_core.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ic_queue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ic_crc.c
)

set_source_files_properties(${INTERCOM_SOURCES} PROPERTIES LANGUAGE C)

# ==============================================================================
# PROJECT CONFIGURATION
# ==============================================================================

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND INTERCOM_COMPILE_DEFINITIONS "INTERCOM_DEBUG")
endif()

set(INTERCOM_LIBRARY_TYPE "STATIC")        # Static library, linked into the firmware
set(INTERCOM_LIBRARY_INTERFACE "PUBLIC")   # Interface scope for consumers of the library

# ==============================================================================
# LIBRARY TARGET
# ==============================================================================

set(INTERCOM_C "intercom")
add_library(${INTERCOM_C} ${INTERCOM_LIBRARY_TYPE} ${INTERCOM_SOURCES})
target_include_directories(${INTERCOM_C} ${INTERCOM_LIBRARY_INTERFACE} ${INTERCOM_HEADERS})
target_compile_definitions(${INTERCOM_C} ${INTERCOM_LIBRARY_INTERFACE} ${INTERCOM_COMPILE_DEFINITIONS})
set_target_properties(${INTERCOM_C} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

if(INTERCOM_COMPILE_OPTIONS) # Add any additional compile options if defined
    target_compile_options(${INTERCOM_C} PRIVATE ${INTERCOM_COMPILE_OPTIONS})
endif()
