################################################################################
# intercom: deterministic master/slave messaging over a single-wire UART
#
# Project:     intercom - C99 static library for half-duplex UART buses
# Description: Top-level CMake file: compiler settings, library, tests and
#              static analysis targets.
#
# Author:      Anton Vynohradov
# Copyright:   (c) 2026 Anton Vynohradov
#
# References:
#   - Protocol description: docs/protocol.md
#   - CMake Documentation: https://cmake.org/documentation/
#
# Last Updated: October 2026
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

project(INTERCOM
    DESCRIPTION "Deterministic master/slave messaging over a single-wire UART"
    LANGUAGES C
    )

if(CMAKE_HOST_WIN32)
    set(CMAKE_COLOR_MAKEFILE OFF)
endif()

# ==============================================================================
# COMPILER FLAGS & OPTIONS
# ==============================================================================

# The library is plain C99 (no extensions), see CLAUDE.md / docs
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler warning flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS_DEBUG "-Wall -Wextra -Wpedantic -Wconversion -g")
    set(CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -Wpedantic -Wconversion -Os -DNDEBUG")
endif()

# ==============================================================================
# LIBRARY/EXECUTABLE TARGETS
# ==============================================================================

# Simulation tests are optional and can be enabled with a CMake option
option(INTERCOM_BUILD_TESTS "Enable intercom host simulation tests" OFF)
option(INTERCOM_SANITIZERS "Build the simulation tests with ASan/UBSan (GCC/Clang, not MinGW)" OFF)

# Folder that contains IntercomConfig.h. When this repository is built on its
# own (not as a submodule), fall back to the test configuration or to a copy of
# the template, so the library can be built and analysed without a project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT INTERCOM_CONFIG_DIR)
    if(INTERCOM_BUILD_TESTS)
        set(INTERCOM_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/config")
    else()
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/IntercomConfig_template.h
                       ${CMAKE_BINARY_DIR}/config/IntercomConfig.h COPYONLY)
        set(INTERCOM_CONFIG_DIR "${CMAKE_BINARY_DIR}/config")
    endif()
    message(STATUS "intercom: using IntercomConfig.h from ${INTERCOM_CONFIG_DIR}")
endif()

# The static library target is defined in intercom.cmake
include(intercom)

# ==============================================================================
# SIMULATION TESTS
# ==============================================================================

if(INTERCOM_BUILD_TESTS)
    # Bus simulator executable, CTest registration and the 'sim_tests' target
    include(sim_tests)
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
                --std=c99
                --force
                --template=gcc
                --suppress=missingIncludeSystem
                --suppress=unusedFunction   # public library API is called by integrators, not by the library
                -I ${CMAKE_SOURCE_DIR}/include
                -I ${CMAKE_SOURCE_DIR}/src
                -I ${INTERCOM_CONFIG_DIR}
                ${CPPCHECK_EXCLUDES}
                --output-file=${CMAKE_BINARY_DIR}/cppcheck.txt
                ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_BINARY_DIR}/cppcheck.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Cppcheck static analysis..."
        VERBATIM
    )

    # Cppcheck MISRA C:2012 addon target
    # Runs the addon directly on Cppcheck dump files (see cmake/misra_check.cmake),
    # because not every Cppcheck package can locate and launch its addons. Rule
    # texts are licensed by MISRA and are not shipped with Cppcheck; pass them with
    # -DINTERCOM_MISRA_RULE_TEXTS=<file> to get readable messages instead of rule numbers.

    set(INTERCOM_MISRA_RULE_TEXTS "" CACHE FILEPATH "Optional MISRA C:2012 rule texts file for Cppcheck")

    get_filename_component(CPPCHECK_BIN_DIR ${CPPCHECK_EXECUTABLE} DIRECTORY)
    find_file(INTERCOM_MISRA_SCRIPT misra.py
        PATHS ${CPPCHECK_BIN_DIR}/addons
              ${CPPCHECK_BIN_DIR}/../share/cppcheck/addons
              /usr/share/cppcheck/addons
              /usr/local/share/cppcheck/addons
        NO_DEFAULT_PATH)
    find_package(Python3 QUIET COMPONENTS Interpreter)

    if(INTERCOM_MISRA_SCRIPT AND Python3_Interpreter_FOUND)
        add_custom_target(misra-analyze
            COMMAND ${CMAKE_COMMAND}
                    -DCPPCHECK=${CPPCHECK_EXECUTABLE}
                    -DPYTHON=${Python3_EXECUTABLE}
                    -DMISRA_SCRIPT=${INTERCOM_MISRA_SCRIPT}
                    -DRULE_TEXTS=${INTERCOM_MISRA_RULE_TEXTS}
                    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                    -DCONFIG_DIR=${INTERCOM_CONFIG_DIR}
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

    # Note: This target intentionally builds only the core library; the
    # simulation tests are excluded to keep the analysis focused.

    if(WIN32)
        set(CODEQL_BUILD_SCRIPT "${CMAKE_BINARY_DIR}/codeql_build.cmd")
        file(WRITE "${CODEQL_BUILD_SCRIPT}" "\"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --target intercom\r\n")
        set(CODEQL_BUILD_COMMAND "${CODEQL_BUILD_SCRIPT}")
    else()
        set(CODEQL_BUILD_COMMAND "${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target intercom")
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

# install(TARGETS intercom ...)
