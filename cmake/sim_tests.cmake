################################################################################
# intercom: deterministic master/slave messaging over a single-wire UART
#
# Project:     intercom - C99 static library for half-duplex UART buses
# Description: CMake configuration for the host bus simulation tests
#
# Author:      Anton Vynohradov
# Copyright:   (c) 2026 Anton Vynohradov
#
# References:
#   - Test description: docs/testing.md
#   - CMake Documentation: https://cmake.org/documentation/
#
# Last Updated: October 2026
#
################################################################################

# ==============================================================================
# COMPILER FLAGS
# ==============================================================================

# The library must stay warning-free; tests turn every warning into an error
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(intercom PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
endif()

if(INTERCOM_SANITIZERS)
    set(INTERCOM_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(intercom PRIVATE ${INTERCOM_SANITIZER_FLAGS})
endif()

# ==============================================================================
# TESTING
# ==============================================================================

enable_testing()

set(INTERCOM_SIM_TEST intercom_sim_test)

add_executable(${INTERCOM_SIM_TEST} ${CMAKE_CURRENT_LIST_DIR}/../tests/sim_test.c)

# The simulator drives several ic_t instances directly, so it needs the private headers
target_include_directories(${INTERCOM_SIM_TEST} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src)
target_link_libraries(${INTERCOM_SIM_TEST} PRIVATE intercom)

if(INTERCOM_SANITIZERS)
    target_compile_options(${INTERCOM_SIM_TEST} PRIVATE ${INTERCOM_SANITIZER_FLAGS})
    target_link_options(${INTERCOM_SIM_TEST} PRIVATE ${INTERCOM_SANITIZER_FLAGS})
endif()

add_test(NAME ${INTERCOM_SIM_TEST} COMMAND ${INTERCOM_SIM_TEST})
set_tests_properties(${INTERCOM_SIM_TEST} PROPERTIES
    LABELS "sim"
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    )

# Convenience target: build and run the simulation through CTest
add_custom_target(sim_tests
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L sim
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
)
