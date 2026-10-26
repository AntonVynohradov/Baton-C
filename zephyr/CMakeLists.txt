# Baton-C Zephyr module
#
# Copyright (c) 2026 Anton Vynohradov
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BATON)

  set(BATON_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

  # baton_user_config.h is part of the application: the message lists must be
  # identical on every node of the bus.
  set(BATON_CONFIG_DIR ${CONFIG_BATON_USER_CONFIG_DIR})
  if(NOT IS_ABSOLUTE "${BATON_CONFIG_DIR}")
    set(BATON_CONFIG_DIR ${APPLICATION_SOURCE_DIR}/${BATON_CONFIG_DIR})
  endif()
  if(NOT EXISTS ${BATON_CONFIG_DIR}/baton_user_config.h)
    message(FATAL_ERROR
      "Baton: ${BATON_CONFIG_DIR}/baton_user_config.h not found. Copy "
      "config/baton_user_config_template.h from Baton-C there or set "
      "CONFIG_BATON_USER_CONFIG_DIR.")
  endif()

  zephyr_include_directories(
    ${BATON_ROOT}
    ${BATON_ROOT}/inc
    ${BATON_CONFIG_DIR}
    include
  )

  zephyr_library_named(baton)
  zephyr_library_sources(
    ${BATON_ROOT}/src/baton_protocol.c
    ${BATON_ROOT}/src/baton_link.c
    ${BATON_ROOT}/src/baton_queue.c
    ${BATON_ROOT}/src/baton_node.c
    baton_zephyr.c
  )

endif()
