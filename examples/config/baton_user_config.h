/**
 * @file baton_user_config.h
 * @brief Message lists shared by every node of the example bus.
 *
 * Project: Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Author: Anton Vynohradov
 * Email: avynohradov@systemfromscratch.com
 *
 * @copyright Copyright (c) 2026 Anton Vynohradov
 * @license Apache License 2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BATON_USER_CONFIG_H
#define BATON_USER_CONFIG_H

/* Example bus: master 0x01, slaves 0x11 and 0x12. */
#define BATON_MAX_NODES 4U
#define BATON_MAX_PAYLOAD 64U
#define BATON_MAX_MSGS_PER_FRAME 8U
#define BATON_QUEUE_HIGH_SIZE 128U
#define BATON_QUEUE_NORMAL_SIZE 256U
#define BATON_RX_BUF_SIZE 128U

/*
 * STATE NODE_STATUS    uptime in seconds (u32, little endian)
 * EVENT CMD_SET_OUTPUT output index, value              (to a slave)
 * EVENT OUTPUT_REPORT  output index, value now applied  (back to the sender)
 */

/* Deviation DEV-04 (Rule 20.7): X is a macro name, it cannot be parenthesized. */
/* cppcheck-suppress misra-c2012-20.7 */
#define BATON_STATE_LIST(X) X(NODE_STATUS, 0x01, 4)

/* cppcheck-suppress misra-c2012-20.7 */
#define BATON_EVENT_LIST(X)    \
    X(CMD_SET_OUTPUT, 0x10, 2) \
    X(OUTPUT_REPORT, 0x11, 2)

#endif /* BATON_USER_CONFIG_H */
