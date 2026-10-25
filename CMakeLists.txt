################################################################################
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Top-level CMake file: compiler settings, library, tests and
#              static analysis targets.
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
# CMAKE CONFIGURATION
# ==============================================================================

cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

# ==============================================================================
# PROJECT CONFIGURATION
# ==============================================================================

project(BATON
    DESCRIPTION "Deterministic master/slave messaging over a single-wire UART"
    LANGUAGES C
    )

if(CMAKE_HOST_WIN32)
    set(CMAKE_COLOR_MAKEFILE OFF)
endif()

# ==============================================================================
# COMPILER FLAGS & OPTIONS
# ==============================================================================

# C11 is the newest language revision covered by MISRA C:2012 (Amendment 2)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler warning flags
set(CMAKE_C_FLAGS_DEBUG "-Wall -Wextra -Wpedantic -Wconversion -g")
set(CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -Wpedantic -Wconversion -O2 -DNDEBUG")

# ==============================================================================
# LIBRARY/EXECUTABLE TARGETS
# ==============================================================================

# Unit and integration tests are optional and can be enabled with CMake options
option(BATON_UNIT_TESTS "Enable Baton unit tests (GoogleTest) and the bus simulation" OFF)
option(BATON_INTEGRATION_TESTS "Enable Baton integration tests (pytest)" OFF)
option(BATON_EXAMPLES "Build the host-buildable examples (bare metal simulation, serial port master)" OFF)
option(BATON_SANITIZERS "Build the bus simulation runner with ASan/UBSan (GCC/Clang, not MinGW)" OFF)

# Folder with baton_user_config.h. When this repository is built on its own (not
# included by an application), use the test configuration for the tests and the
# analysis, the example configuration for the examples, or a copy of the template.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT BATON_CONFIG_DIR)
    if(BATON_UNIT_TESTS OR BATON_INTEGRATION_TESTS)
        set(BATON_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/config")
    elseif(BATON_EXAMPLES)
        set(BATON_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/examples/config")
    else()
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/baton_user_config_template.h
                       ${CMAKE_BINARY_DIR}/config/baton_user_config.h COPYONLY)
        set(BATON_CONFIG_DIR "${CMAKE_BINARY_DIR}/config")
    endif()
    message(STATUS "Baton: using baton_user_config.h from ${BATON_CONFIG_DIR}")
endif()

# The static library target is defined in baton.cmake
include(baton)

# ==============================================================================
# UNIT TESTS
# ==============================================================================

if(BATON_UNIT_TESTS)
    # GTest integration, test discovery, the coverage target and the bus simulation runner
    include(unit_tests)
endif()

# ==============================================================================
# INTEGRATION TESTS
# ==============================================================================

if(BATON_INTEGRATION_TESTS)
    # Python codec tests and the C/Python cross-check, run through CTest
    include(integration_tests)
endif()

if(BATON_EXAMPLES)
    # The bare-metal example runs against a simulated bus as a CTest test
    enable_testing()
    add_subdirectory(examples)
endif()

# ==============================================================================
# STATIC ANALYSIS TARGETS
# ==============================================================================

# Cppcheck static analysis target
# Runs Cppcheck with all checks enabled and writes the result to cppcheck.txt in
# the build directory and to the console. Paths listed in 'cppcheck-excludes.txt'
# are excluded. The target exists only if Cppcheck is found on the system.

find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)

    set(CPPCHECK_EXCLUDES)
    if(EXISTS "${CMAKE_SOURCE_DIR}/cppcheck-excludes.txt")
        file(STRINGS "${CMAKE_SOURCE_DIR}/cppcheck-excludes.txt" CPPCHECK_EXCLUDE_PATHS)
        foreach(CPPCHECK_EXCLUDE_PATH IN LISTS CPPCHECK_EXCLUDE_PATHS)
            # Absolute paths, because Cppcheck matches them against the absolute source paths below
            if(NOT CPPCHECK_EXCLUDE_PATH STREQUAL "" AND NOT CPPCHECK_EXCLUDE_PATH MATCHES "^#")
                list(APPEND CPPCHECK_EXCLUDES -i "${CMAKE_SOURCE_DIR}/${CPPCHECK_EXCLUDE_PATH}")
            endif()
        endforeach()
    endif()

    add_custom_target(cppcheck-analyze
        COMMAND ${CPPCHECK_EXECUTABLE}
                --enable=all
                --check-level=exhaustive
                --inconclusive
                --verbose
                --quiet
                --std=c11
                --force
                --template=gcc
                --suppress=missingIncludeSystem
                --suppress=unusedFunction   # public library API is called by integrators, not by the library
                --suppress=checkersReport
                -I ${CMAKE_SOURCE_DIR}
                -I ${CMAKE_SOURCE_DIR}/inc
                -I ${CMAKE_SOURCE_DIR}/tests/config
                ${CPPCHECK_EXCLUDES}
                --output-file=${CMAKE_BINARY_DIR}/cppcheck.txt
                ${CMAKE_SOURCE_DIR}/src
        COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/cppcheck.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Cppcheck static analysis..."
        VERBATIM
    )

    # Cppcheck MISRA C:2012 addon target
    # Runs the addon directly on Cppcheck dump files (see cmake/misra_check.cmake),
    # because not every Cppcheck package can locate and launch its addons. Rule
    # texts are licensed by MISRA and are not shipped with Cppcheck; pass them with
    # -DBATON_MISRA_RULE_TEXTS=<file> to get readable messages instead of rule numbers.

    set(BATON_MISRA_RULE_TEXTS "" CACHE FILEPATH "Optional MISRA C:2012 rule texts file for Cppcheck")
    option(BATON_MISRA_FAIL_ON_VIOLATION "Make 'misra-analyze' fail when the report has violations" ON)

    get_filename_component(CPPCHECK_BIN_DIR ${CPPCHECK_EXECUTABLE} DIRECTORY)
    find_file(BATON_MISRA_SCRIPT misra.py
        PATHS ${CPPCHECK_BIN_DIR}/addons
              ${CPPCHECK_BIN_DIR}/../share/cppcheck/addons
              /usr/share/cppcheck/addons
              /usr/local/share/cppcheck/addons
              /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/cppcheck/addons   # Debian/Ubuntu packages
        NO_DEFAULT_PATH)
    find_package(Python3 QUIET COMPONENTS Interpreter)

    if(BATON_MISRA_SCRIPT AND Python3_Interpreter_FOUND)
        add_custom_target(misra-analyze
            COMMAND ${CMAKE_COMMAND}
                    -DCPPCHECK=${CPPCHECK_EXECUTABLE}
                    -DPYTHON=${Python3_EXECUTABLE}
                    -DMISRA_SCRIPT=${BATON_MISRA_SCRIPT}
                    -DRULE_TEXTS=${BATON_MISRA_RULE_TEXTS}
                    -DFAIL_ON_VIOLATION=${BATON_MISRA_FAIL_ON_VIOLATION}
                    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                    -DCONFIG_DIR=${CMAKE_SOURCE_DIR}/tests/config
                    -DOUTPUT=${CMAKE_BINARY_DIR}/misra.txt
                    -P ${CMAKE_SOURCE_DIR}/cmake/misra_check.cmake
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running Cppcheck MISRA C:2012 analysis..."
            VERBATIM
        )
    else()
        message(STATUS "Cppcheck misra.py addon or Python 3 not found. Target 'misra-analyze' will be unavailable.")
    endif()
else()
    message(STATUS "Cppcheck not found. Static analysis targets 'cppcheck-analyze' and 'misra-analyze' will be unavailable.")
endif()

# CodeQL analysis target
# Creates a CodeQL database in the build directory, runs the cpp-queries suite
# and writes a SARIF report (codeql-report.sarif). The target exists only if the
# CodeQL CLI is found on the system.

find_program(CODEQL_EXECUTABLE codeql)
if(CODEQL_EXECUTABLE)

    # Note: This target intentionally builds only the core library; examples and
    # tests are excluded to keep the analysis focused.

    if(WIN32)
        set(CODEQL_BUILD_SCRIPT "${CMAKE_BINARY_DIR}/codeql_build.cmd")
        file(WRITE "${CODEQL_BUILD_SCRIPT}" "\"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --target baton\r\n")
        set(CODEQL_BUILD_COMMAND "${CODEQL_BUILD_SCRIPT}")
    else()
        set(CODEQL_BUILD_COMMAND "${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target baton")
    endif()

    add_custom_target(codeql-analyze
        COMMAND ${CODEQL_EXECUTABLE} database create
            ${CMAKE_BINARY_DIR}/codeql-db
            --language=c
            --overwrite
            --source-root=${CMAKE_SOURCE_DIR}
            --command "${CODEQL_BUILD_COMMAND}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Creating CodeQL database..."

        COMMAND ${CODEQL_EXECUTABLE} database analyze
                ${CMAKE_BINARY_DIR}/codeql-db
                codeql/cpp-queries
                --format=sarif-latest
                --output=${CMAKE_BINARY_DIR}/codeql-report.sarif
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running CodeQL analysis..."
        VERBATIM
    )
else()
    message(STATUS "CodeQL not found. Static analysis target 'codeql-analyze' will be unavailable.")
endif()

# ==============================================================================
# INSTALLATION
# ==============================================================================

# install(TARGETS baton ...)
