################################################################################
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Extracts the library version from the latest Git tag (vX.Y.Z)
#              and exposes it as BATON_VERSION_* compile definitions.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
# License:     Apache License 2.0
# Copyright:   (c) 2026 Anton Vynohradov
#
# References:
#   - Protocol description: docs/protocol.md
#   - MISRA C:2012 Guidelines for the use of the C language in critical systems
#   - CMake Documentation: https://cmake.org/documentation/
#
# Last Updated: October 2026
# SPDX-License-Identifier: Apache-2.0
#
################################################################################

# ==============================================================================
# DEFAULT VERSION
# ==============================================================================

# Used when the source tree has no Git metadata or no vX.Y.Z tag yet; keep it in
# sync with the fallback values in baton.h.
set(BATON_DEFAULT_VERSION "1.0.0")

# ==============================================================================
# VERSION EXTRACTION
# ==============================================================================

find_package(Git QUIET)

set(BATON_GIT_TAG "")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
        RESULT_VARIABLE BATON_GIT_DESCRIBE_RESULT
        ERROR_VARIABLE BATON_GIT_DESCRIBE_ERROR
        OUTPUT_VARIABLE BATON_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
endif()

# ==============================================================================
# VERSION PARSING
# ==============================================================================

string(REGEX MATCH "^[Vv]([0-9]+)\\.([0-9]+)\\.([0-9]+)$" BATON_VERSION_MATCH "${BATON_GIT_TAG}")

if(NOT BATON_VERSION_MATCH)
    message(WARNING "No vX.Y.Z Git tag found (${BATON_GIT_DESCRIBE_ERROR}); "
                    "using default version ${BATON_DEFAULT_VERSION}")
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" BATON_VERSION_MATCH "${BATON_DEFAULT_VERSION}")
endif()

# ==============================================================================
# VERSION COMPONENTS
# ==============================================================================

set(BATON_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(BATON_VERSION_MINOR "${CMAKE_MATCH_2}")
set(BATON_VERSION_PATCH "${CMAKE_MATCH_3}")

set(BATON_VERSION "${BATON_VERSION_MAJOR}.${BATON_VERSION_MINOR}.${BATON_VERSION_PATCH}")

message(STATUS "BATON library is version: ${BATON_VERSION}")

# ==============================================================================
# CONFIGURE VERSION DEFINITIONS
# ==============================================================================

list(APPEND BATON_COMPILE_DEFINITIONS
    BATON_VERSION_MAJOR=${BATON_VERSION_MAJOR}
    BATON_VERSION_MINOR=${BATON_VERSION_MINOR}
    BATON_VERSION_PATCH=${BATON_VERSION_PATCH}
    BATON_VERSION="${BATON_VERSION}"
)
