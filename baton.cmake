################################################################################
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Library target definition. Can be included directly by an
#              integrating project: include(path/to/baton.cmake)
#              BATON_CONFIG_DIR must point to the folder with the application's
#              baton_user_config.h before this file is included.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
# License:     Apache License 2.0
# Copyright:   (c) 2026 Anton Vynohradov
#
# References:
#   - Integration guide: docs/integration.md
#   - CMake Documentation: https://cmake.org/documentation/
#
# Last Updated: October 2026
# SPDX-License-Identifier: Apache-2.0
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

# Folder that contains the application's baton_user_config.h
# (-DBATON_CONFIG_DIR=... or set(BATON_CONFIG_DIR ...) in the integrating project)
if(NOT BATON_CONFIG_DIR OR NOT EXISTS "${BATON_CONFIG_DIR}/baton_user_config.h")
    message(FATAL_ERROR
        "Baton: set BATON_CONFIG_DIR to the folder with your baton_user_config.h "
        "(copy config/baton_user_config_template.h there). Current value: '${BATON_CONFIG_DIR}'")
endif()

# ==============================================================================
# INCLUDE DIRECTORIES
# ==============================================================================

list(APPEND BATON_HEADERS

    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/inc
    ${BATON_CONFIG_DIR}
)

# ==============================================================================
# SOURCE FILES
# ==============================================================================

list(APPEND BATON_SOURCES

    ${CMAKE_CURRENT_LIST_DIR}/src/baton_protocol.c
    ${CMAKE_CURRENT_LIST_DIR}/src/baton_link.c
    ${CMAKE_CURRENT_LIST_DIR}/src/baton_queue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/baton_node.c
)

set_source_files_properties(${BATON_SOURCES} PROPERTIES LANGUAGE C)

# Library sources only (BATON_SOURCES also receives the test sources below)
set(BATON_LIBRARY_SOURCES ${BATON_SOURCES})

# ==============================================================================
# PROJECT CONFIGURATION
# ==============================================================================

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND BATON_COMPILE_DEFINITIONS "BATON_DEBUG")
endif()

set(BATON_LIBRARY_TYPE "STATIC")        # Default to static library, can be overridden by CMake presets
set(BATON_LIBRARY_INTERFACE "PUBLIC")   # Interface scope for consumers of the library

# ==============================================================================
# UNIT TESTS
# ==============================================================================

if(BATON_UNIT_TESTS)

    list(APPEND BATON_UNIT_TEST_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_crc16.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_defs.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_parser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_encode.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_link.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_queue.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_node_init.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_node_api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_node_slave.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_node_master.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/gtest_baton_bus_sim.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit/baton_test_support.cpp
        ${CMAKE_CURRENT_LIST_DIR}/tests/sim/baton_bus_sim.c
    )

    list(APPEND BATON_UNIT_TEST_HEADERS
        ${CMAKE_CURRENT_LIST_DIR}/tests/unit
        ${CMAKE_CURRENT_LIST_DIR}/tests/sim
    )

    list(APPEND BATON_HEADERS ${BATON_UNIT_TEST_HEADERS})
    list(APPEND BATON_SOURCES ${BATON_UNIT_TEST_SOURCES})

    set(BATON_LIBRARY_TYPE "INTERFACE")         # Sources are compiled into the test executable (coverage)
    set(BATON_LIBRARY_INTERFACE "INTERFACE")    # Consumers (the test executable) link against this target

endif()

# ==============================================================================
# LIBRARY TARGET
# ==============================================================================

set(BATON_C "baton")
add_library(${BATON_C} ${BATON_LIBRARY_TYPE} ${BATON_SOURCES})
target_include_directories(${BATON_C} ${BATON_LIBRARY_INTERFACE} ${BATON_HEADERS})
target_compile_definitions(${BATON_C} ${BATON_LIBRARY_INTERFACE} ${BATON_COMPILE_DEFINITIONS})

if(BATON_COMPILE_OPTIONS) # Add any additional compile options if defined
    target_compile_options(${BATON_C} ${BATON_LIBRARY_INTERFACE} ${BATON_COMPILE_OPTIONS})
endif()
