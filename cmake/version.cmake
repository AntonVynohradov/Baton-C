################################################################################
# intercom: deterministic master/slave messaging over a single-wire UART
#
# Project:     intercom - C99 static library for half-duplex UART buses
# Description: Extracts the library version from the latest Git tag (vX.Y.Z)
#              and exposes it as INTERCOM_VERSION_* compile definitions.
#
# Author:      Anton Vynohradov
# Copyright:   (c) 2026 Anton Vynohradov
#
# References:
#   - CMake Documentation: https://cmake.org/documentation/
#
# Last Updated: October 2026
#
################################################################################

# ==============================================================================
# DEFAULT VERSION
# ==============================================================================

# Used when the source tree has no Git metadata or no vX.Y.Z tag yet; keep it in
# sync with the fallback values in include/Intercom.h.
set(INTERCOM_DEFAULT_VERSION "1.0.0")

# ==============================================================================
# VERSION EXTRACTION
# ==============================================================================

find_package(Git QUIET)

set(INTERCOM_GIT_TAG "")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
        RESULT_VARIABLE INTERCOM_GIT_DESCRIBE_RESULT
        ERROR_VARIABLE INTERCOM_GIT_DESCRIBE_ERROR
        OUTPUT_VARIABLE INTERCOM_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
endif()

# ==============================================================================
# VERSION PARSING
# ==============================================================================

string(REGEX MATCH "^[Vv]([0-9]+)\\.([0-9]+)\\.([0-9]+)$" INTERCOM_VERSION_MATCH "${INTERCOM_GIT_TAG}")

if(NOT INTERCOM_VERSION_MATCH)
    message(WARNING "No vX.Y.Z Git tag found (${INTERCOM_GIT_DESCRIBE_ERROR}); "
                    "using default version ${INTERCOM_DEFAULT_VERSION}")
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" INTERCOM_VERSION_MATCH "${INTERCOM_DEFAULT_VERSION}")
endif()

# ==============================================================================
# VERSION COMPONENTS
# ==============================================================================

set(INTERCOM_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(INTERCOM_VERSION_MINOR "${CMAKE_MATCH_2}")
set(INTERCOM_VERSION_PATCH "${CMAKE_MATCH_3}")

set(INTERCOM_VERSION "${INTERCOM_VERSION_MAJOR}.${INTERCOM_VERSION_MINOR}.${INTERCOM_VERSION_PATCH}")

message(STATUS "intercom library is version: ${INTERCOM_VERSION}")

# ==============================================================================
# CONFIGURE VERSION DEFINITIONS
# ==============================================================================

list(APPEND INTERCOM_COMPILE_DEFINITIONS
    INTERCOM_VERSION_MAJOR=${INTERCOM_VERSION_MAJOR}
    INTERCOM_VERSION_MINOR=${INTERCOM_VERSION_MINOR}
    INTERCOM_VERSION_PATCH=${INTERCOM_VERSION_PATCH}
)
