################################################################################
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: CMake configuration for GoogleTest unit tests, coverage and the
#              bus simulation runner
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

enable_language(CXX)

find_package(GTest REQUIRED)

if(NOT GTest_FOUND)
    message(FATAL_ERROR "Google Test not found. Please install Google Test to build unit tests.")
endif()

# ==============================================================================
# PROJECT CONFIGURATION
# ==============================================================================

include(GoogleTest)
include(CTest)

# ==============================================================================
# TESTING
# ==============================================================================

enable_testing()

set(BATON_GTEST ${CMAKE_PROJECT_NAME}_gtest)

add_executable(${BATON_GTEST} ${BATON_SOURCES})

set_target_properties(${BATON_GTEST} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${BATON_GTEST} PRIVATE -O0 -g --coverage)
    target_link_options(${BATON_GTEST} PRIVATE --coverage)
endif()

target_link_libraries(${BATON_GTEST}
                    GTest::gtest
                    GTest::gtest_main
                    baton
                    )

gtest_discover_tests(${BATON_GTEST} PROPERTIES LABELS "unit" DISCOVERY_TIMEOUT 60)

# The library sources must stay warning-free: turn every warning into an error
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${BATON_LIBRARY_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Wconversion;-Werror")
endif()

# ==============================================================================
# BUS SIMULATION RUNNER
# ==============================================================================

# The same simulation as the BusSim unit tests, optimized, for many seeds:
#   baton_sim [seed]      tests/run_seeds.sh <build-dir> <count>
set(BATON_SIM baton_sim)

add_executable(${BATON_SIM}
    ${CMAKE_SOURCE_DIR}/tests/sim/baton_sim_main.c
    ${CMAKE_SOURCE_DIR}/tests/sim/baton_bus_sim.c
    ${BATON_LIBRARY_SOURCES}
    )

target_include_directories(${BATON_SIM} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/tests/sim
    ${BATON_CONFIG_DIR}
    )

target_compile_options(${BATON_SIM} PRIVATE -O2)

if(BATON_SANITIZERS)
    set(BATON_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_compile_options(${BATON_SIM} PRIVATE ${BATON_SANITIZER_FLAGS})
    target_link_options(${BATON_SIM} PRIVATE ${BATON_SANITIZER_FLAGS})
endif()

add_test(NAME ${BATON_SIM} COMMAND ${BATON_SIM})
set_tests_properties(${BATON_SIM} PROPERTIES
    LABELS "sim"
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    )

# ==============================================================================
# COVERAGE
# ==============================================================================

set(BATON_COVERAGE_MIN 100 CACHE STRING "Minimum line and branch coverage (%) required by the 'coverage' target")

find_program(GCOVR_EXECUTABLE gcovr)
if(GCOVR_EXECUTABLE)
    set(COVERAGE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/Coverage)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_OUTPUT_DIR}
        COMMAND ${GCOVR_EXECUTABLE}
            -r ${CMAKE_SOURCE_DIR}
            --object-directory ${CMAKE_BINARY_DIR}
            --filter ${CMAKE_SOURCE_DIR}/src
            --exclude ${CMAKE_SOURCE_DIR}/tests
            --lcov ${COVERAGE_OUTPUT_DIR}/coverage.info
            --html-details=${COVERAGE_OUTPUT_DIR}/coverage.html
            --print-summary
            --delete                    # fresh counters for the next run
            --fail-under-line ${BATON_COVERAGE_MIN}
            --fail-under-branch ${BATON_COVERAGE_MIN}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
        )
else()
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E echo "gcovr not found. Install gcovr to generate coverage reports."
        )
endif()
