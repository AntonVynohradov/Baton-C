################################################################################
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Host-buildable examples: the bare-metal slave on a simulated bus
#              (a self-checking CTest test) and the serial port master.
#              The Zephyr example is the sample of the Zephyr module, built
#              with west: zephyr/samples/baton_node.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
# License:     Apache License 2.0
# Copyright:   (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#
################################################################################

# ==============================================================================
# BARE METAL (SIMULATED BOARD)
# ==============================================================================

add_executable(baton_bare_metal_sim
    bare_metal/main.c
    bare_metal/board_host.c
)
target_link_libraries(baton_bare_metal_sim PRIVATE baton)

add_test(NAME example_bare_metal COMMAND baton_bare_metal_sim)
set_tests_properties(example_bare_metal PROPERTIES
    LABELS "example"
    PASS_REGULAR_EXPRESSION "EXAMPLE PASSED"
    )

# ==============================================================================
# HOST SERIAL PORT
# ==============================================================================

if(WIN32)
    set(BATON_SERIAL_PORT_SOURCE host_serial/serial_port_win32.c)
else()
    set(BATON_SERIAL_PORT_SOURCE host_serial/serial_port_posix.c)
endif()

add_executable(baton_master
    host_serial/main.c
    ${BATON_SERIAL_PORT_SOURCE}
)
target_link_libraries(baton_master PRIVATE baton)
