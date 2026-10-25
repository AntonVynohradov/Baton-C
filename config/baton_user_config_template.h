/**
 * @file baton_user_config.h (template)
 * @brief Application configuration of the Baton library: message lists and sizes.
 *
 * HOW TO USE
 *   1. Copy this file into your application project and rename it to
 *      "baton_user_config.h".
 *   2. Add its folder to the include path of BOTH the application and the
 *      library build (CMake: set BATON_CONFIG_DIR before include(baton.cmake)).
 *   3. Use the SAME message lists on every node of the bus.
 *
 * Do not edit this template inside the library repository.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BATON_USER_CONFIG_H
#define BATON_USER_CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------------ */
/* Sizes (optional; defaults in baton_config.h). All memory is inside        */
/* struct baton_node, owned by the application.                              */
/* ------------------------------------------------------------------------ */

#define BATON_MAX_NODES 8U           /* nodes including the master, 2..32          */
#define BATON_MAX_PAYLOAD 64U        /* frame payload, 16..250; a message = len + 2 */
#define BATON_MAX_MSGS_PER_FRAME 16U /* messages packed into one frame, 1..64      */
#define BATON_QUEUE_HIGH_SIZE 256U   /* bytes; an event uses 16 + len rounded to 4 */
#define BATON_QUEUE_NORMAL_SIZE 512U /* bytes                                      */
#define BATON_RX_BUF_SIZE 128U       /* receive ring, power of two, 16..4096       */

/* ------------------------------------------------------------------------ */
/* Message definitions                                                        */
/*                                                                            */
/*   X(NAME, ID, MAX_LEN)                                                     */
/*     NAME    -> BATON_MSG_<NAME> and BATON_MSG_<NAME>_MAX_LEN               */
/*     ID      -> 0..255, unique across BOTH lists, identical on all nodes    */
/*     MAX_LEN -> 1..BATON_MAX_MSG_LEN                                        */
/*                (BATON_MAX_PAYLOAD - 3 - (BATON_MAX_NODES + 7) / 8)         */
/*                                                                            */
/*   STATE: latest value wins, sent to every other node when it changes.      */
/*   EVENT: queued, acknowledged, delivered exactly once.                     */
/* ------------------------------------------------------------------------ */

/* Deviation DEV-04 (Rule 20.7): X is a macro name, it cannot be parenthesized. */
/* cppcheck-suppress misra-c2012-20.7 */
#define BATON_STATE_LIST(X) \
    X(NODE_STATUS, 0x01, 4) \
    X(TEMPERATURE, 0x02, 2)

/* cppcheck-suppress misra-c2012-20.7 */
#define BATON_EVENT_LIST(X) \
    X(CMD_START, 0x10, 8)   \
    X(CMD_STOP, 0x11, 1)    \
    X(ALARM, 0x12, 16)

/* ------------------------------------------------------------------------ */
/* Optional: payload types shared by all nodes. Use fixed-width types and    */
/* an explicit layout; sizeof() <= MAX_LEN.                                  */
/* ------------------------------------------------------------------------ */

struct app_cmd_start
{
    uint16_t speed;
    uint8_t mode;
    uint8_t flags;
};

#endif /* BATON_USER_CONFIG_H */
