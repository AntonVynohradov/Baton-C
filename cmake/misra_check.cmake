################################################################################
# intercom: deterministic master/slave messaging over a single-wire UART
#
# Project:     intercom - C99 static library for half-duplex UART buses
# Description: Script (cmake -P) behind the 'misra-analyze' target: dumps the
#              library sources with Cppcheck, runs the MISRA C:2012 addon on the
#              dumps, writes the report and removes the dump files.
#
# Author:      Anton Vynohradov
# Copyright:   (c) 2026 Anton Vynohradov
#
# References:
#   - MISRA C:2012 Guidelines for the use of the C language in critical systems
#   - Cppcheck MISRA addon: https://cppcheck.sourceforge.io/misra.php
#   - CMake Documentation: https://cmake.org/documentation/
#
# Last Updated: October 2026
#
# Inputs (-D): CPPCHECK, PYTHON, MISRA_SCRIPT, SOURCE_DIR, CONFIG_DIR, OUTPUT,
#              RULE_TEXTS (optional)
#
################################################################################

# ==============================================================================
# SOURCE FILES
# ==============================================================================

file(GLOB MISRA_SOURCES ${SOURCE_DIR}/src/*.c)

# ==============================================================================
# DUMP
# ==============================================================================

# Cppcheck writes <file>.dump and <file>.ctu-info next to each source file
execute_process(
    COMMAND ${CPPCHECK}
            --dump
            --quiet
            --std=c99
            --suppress=missingIncludeSystem
            -I ${SOURCE_DIR}/include
            -I ${SOURCE_DIR}/src
            -I ${CONFIG_DIR}
            ${MISRA_SOURCES}
    RESULT_VARIABLE MISRA_DUMP_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT MISRA_DUMP_RESULT EQUAL 0)
    message(FATAL_ERROR "Cppcheck --dump failed with code ${MISRA_DUMP_RESULT}")
endif()

# ==============================================================================
# MISRA ADDON
# ==============================================================================

set(MISRA_DUMPS)
foreach(MISRA_SOURCE IN LISTS MISRA_SOURCES)
    list(APPEND MISRA_DUMPS ${MISRA_SOURCE}.dump)
endforeach()

set(MISRA_ARGS)
if(RULE_TEXTS)
    list(APPEND MISRA_ARGS --rule-texts=${RULE_TEXTS})
endif()

execute_process(
    COMMAND ${PYTHON} ${MISRA_SCRIPT} ${MISRA_ARGS} ${MISRA_DUMPS}
    RESULT_VARIABLE MISRA_RESULT
    OUTPUT_VARIABLE MISRA_STDOUT
    ERROR_VARIABLE MISRA_REPORT
)

# ==============================================================================
# CLEANUP & REPORT
# ==============================================================================

foreach(MISRA_SOURCE IN LISTS MISRA_SOURCES)
    file(REMOVE ${MISRA_SOURCE}.dump ${MISRA_SOURCE}.ctu-info)
endforeach()

file(WRITE ${OUTPUT} "${MISRA_REPORT}${MISRA_STDOUT}")
message("${MISRA_REPORT}${MISRA_STDOUT}")

string(REGEX MATCHALL "\\[misra-c2012-[0-9.]+\\]" MISRA_VIOLATIONS "${MISRA_REPORT}")
list(LENGTH MISRA_VIOLATIONS MISRA_VIOLATION_COUNT)
message(STATUS "MISRA C:2012 violations: ${MISRA_VIOLATION_COUNT} (report: ${OUTPUT})")
