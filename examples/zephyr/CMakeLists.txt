################################################################################
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Zephyr application of the Zephyr integration example.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
# License:     Apache License 2.0
# Copyright:   (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#
# Build:  west build -b <board> examples/zephyr -- -DDTC_OVERLAY_FILE=<board>.overlay
#
################################################################################

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(baton_zephyr_slave LANGUAGES C)

# The example bus shares one message list with the other examples
set(BATON_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../config)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../baton.cmake)

# The library is a plain CMake target: link zephyr_interface so it is compiled
# with the board's architecture flags, defines and include paths.
target_link_libraries(baton PUBLIC zephyr_interface)

target_sources(app PRIVATE src/main.c)
target_link_libraries(app PRIVATE baton)
