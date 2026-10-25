/**
 * @file baton_user_config.h
 * @brief Configuration used by the unit tests, the bus simulation and the analysis targets.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BATON_USER_CONFIG_H
#define BATON_USER_CONFIG_H

#define BATON_MAX_NODES 8U
#define BATON_MAX_PAYLOAD 64U
#define BATON_MAX_MSGS_PER_FRAME 16U
#define BATON_QUEUE_HIGH_SIZE 256U
#define BATON_QUEUE_NORMAL_SIZE 512U
#define BATON_RX_BUF_SIZE 256U

/* Deviation DEV-04 (Rule 20.7): X is a macro name, it cannot be parenthesized. */
/* cppcheck-suppress misra-c2012-20.7 */
#define BATON_STATE_LIST(X) \
    X(ST_COUNTER, 0x01, 4)  \
    X(ST_SMALL, 0x02, 1)

/* cppcheck-suppress misra-c2012-20.7 */
#define BATON_EVENT_LIST(X) \
    X(EV_UNI, 0x10, 24)     \
    X(EV_BCAST, 0x11, 12)   \
    X(EV_EMPTY, 0x12, 1)

#endif /* BATON_USER_CONFIG_H */
