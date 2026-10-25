################################################################################
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Native bridge of Baton Inspector: the Baton-C library built with
#              the baton_user_config.h of the system under test, as a shared
#              library for Python ctypes.
#
#   cmake -S tools/baton_inspector/native -B <build> -DBATON_CONFIG_DIR=<folder>
#   cmake --build <build> --config Release
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
# License:     Apache License 2.0
# Copyright:   (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#
################################################################################

cmake_minimum_required(VERSION 3.20)
project(baton_inspector_native LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT BATON_CONFIG_DIR)
    message(FATAL_ERROR "Set BATON_CONFIG_DIR to the folder with baton_user_config.h")
endif()

set(BATON_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)

add_library(baton_inspector_native SHARED
    inspector_bridge.c
    ${BATON_ROOT}/src/baton_protocol.c
    ${BATON_ROOT}/src/baton_link.c
    ${BATON_ROOT}/src/baton_queue.c
    ${BATON_ROOT}/src/baton_node.c
)
target_include_directories(baton_inspector_native PRIVATE
    ${BATON_ROOT} ${BATON_ROOT}/inc ${BATON_CONFIG_DIR})
set_target_properties(baton_inspector_native PROPERTIES
    PREFIX ""
    C_VISIBILITY_PRESET hidden
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG)
    set_target_properties(baton_inspector_native PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFG} ${CMAKE_BINARY_DIR}/lib
        LIBRARY_OUTPUT_DIRECTORY_${CFG} ${CMAKE_BINARY_DIR}/lib)
endforeach()
if(MINGW)
    # a DLL that needs no MinGW runtime next to it
    target_link_options(baton_inspector_native PRIVATE -static -static-libgcc)
endif()
