################################################################################
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: The bus simulation runner baton_sim (library-specific addition to
#              unit_tests.cmake): the BusSim scenarios of the unit tests, optimized,
#              for many seeds and optionally with ASan/UBSan.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
# License:     Apache License 2.0
# Copyright:   (c) 2026 Anton Vynohradov
#
# References:
#   - Protocol description: docs/protocol.md
#   - CMake Documentation: https://cmake.org/documentation/
#
# Last Updated: October 2026
# SPDX-License-Identifier: Apache-2.0
#
################################################################################

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
