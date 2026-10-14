/* Configuration used by the host simulation tests. */
#ifndef INTERCOM_CONFIG_H
#define INTERCOM_CONFIG_H

#include <stdint.h>

#define INTERCOM_MAX_NODES            8
#define INTERCOM_MAX_PAYLOAD          64
#define INTERCOM_MAX_MSGS_PER_FRAME   16
#define INTERCOM_QUEUE_HIGH_SIZE      256
#define INTERCOM_QUEUE_NORMAL_SIZE    512
#define INTERCOM_RX_BUF_SIZE          256

#define INTERCOM_STATE_LIST(X)        \
    X(ST_COUNTER, 0x01, 4)            \
    X(ST_SMALL,   0x02, 1)

#define INTERCOM_EVENT_LIST(X)        \
    X(EV_UNI,     0x10, 24)           \
    X(EV_BCAST,   0x11, 12)           \
    X(EV_EMPTY,   0x12, 1)

#endif
