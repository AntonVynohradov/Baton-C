/**
 * @file    IntercomConfig.h  (template)
 * @brief   Compile-time configuration of the Intercom library.
 *
 * HOW TO USE
 *   1. Copy this file into your application project and rename it to
 *      "IntercomConfig.h".
 *   2. Add the folder containing it to the include path of BOTH the
 *      application and the Intercom library build.
 *   3. Use the SAME message lists on every node of the bus.
 *
 * Do not edit this template inside the submodule.
 */
#ifndef INTERCOM_CONFIG_H
#define INTERCOM_CONFIG_H

/* ------------------------------------------------------------------------ */
/* Sizes (all memory is allocated statically inside the library)             */
/* ------------------------------------------------------------------------ */

/** Number of nodes on the bus including the master. 2..32 */
#define INTERCOM_MAX_NODES            8

/** Maximum frame payload in bytes. 16..250. One message (TLV) needs len+2. */
#define INTERCOM_MAX_PAYLOAD          64

/** Maximum number of messages packed into one frame. 1..64 */
#define INTERCOM_MAX_MSGS_PER_FRAME   16

/** Event queue sizes in bytes (each event uses 16 + len rounded up to 4). */
#define INTERCOM_QUEUE_HIGH_SIZE      256
#define INTERCOM_QUEUE_NORMAL_SIZE    512

/** RX ring buffer filled from the UART ISR. Power of two, 16..4096. */
#define INTERCOM_RX_BUF_SIZE          128

/* ------------------------------------------------------------------------ */
/* Message definitions                                                        */
/*                                                                            */
/*   X(NAME, ID, MAX_LEN)                                                     */
/*     NAME    -> generates INTERCOM_MSG_<NAME> and INTERCOM_MSG_<NAME>_MAX_LEN */
/*     ID      -> 0..255, unique across BOTH lists, identical on all nodes   */
/*     MAX_LEN -> 1..(INTERCOM_MAX_PAYLOAD - 3 - (INTERCOM_MAX_NODES+7)/8)    */
/*                                                                            */
/*   STATE: latest value wins, sent to every other node when it changes.     */
/*   EVENT: queued, acknowledged, delivered exactly once.                    */
/* ------------------------------------------------------------------------ */

#define INTERCOM_STATE_LIST(X)                \
    X(NODE_STATUS,   0x01, 4)                 \
    X(TEMPERATURE,   0x02, 2)

#define INTERCOM_EVENT_LIST(X)                \
    X(CMD_START,     0x10, 8)                 \
    X(CMD_STOP,      0x11, 1)                 \
    X(ALARM,         0x12, 16)

/* ------------------------------------------------------------------------ */
/* Optional: payload types shared by all nodes can live here too.            */
/* Use fixed-width types and packed/explicit layout, sizeof() <= MAX_LEN.    */
/* ------------------------------------------------------------------------ */
#include <stdint.h>

typedef struct
{
    uint16_t speed;
    uint8_t  mode;
    uint8_t  flags;
} IntercomCmdStart;

#endif /* INTERCOM_CONFIG_H */
