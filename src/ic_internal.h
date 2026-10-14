/**
 * @file    ic_internal.h
 * @brief   Private definitions of the Intercom library. Not for application use.
 *
 * The implementation is instance based (ic_t) so it can be unit tested with
 * several nodes in one process. The public API (Intercom.c) wraps a single
 * static instance.
 */
#ifndef IC_INTERNAL_H
#define IC_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "Intercom.h"

/* ------------------------------------------------------------------------ */
/* Configuration checks                                                       */
/* ------------------------------------------------------------------------ */
#if !defined(INTERCOM_MAX_NODES) || (INTERCOM_MAX_NODES < 2) || (INTERCOM_MAX_NODES > 32)
#error "INTERCOM_MAX_NODES must be 2..32"
#endif
#if !defined(INTERCOM_MAX_PAYLOAD) || (INTERCOM_MAX_PAYLOAD < 16) || (INTERCOM_MAX_PAYLOAD > 250)
#error "INTERCOM_MAX_PAYLOAD must be 16..250"
#endif
#if !defined(INTERCOM_MAX_MSGS_PER_FRAME) || (INTERCOM_MAX_MSGS_PER_FRAME < 1) || (INTERCOM_MAX_MSGS_PER_FRAME > 64)
#error "INTERCOM_MAX_MSGS_PER_FRAME must be 1..64"
#endif
#if !defined(INTERCOM_QUEUE_HIGH_SIZE) || (INTERCOM_QUEUE_HIGH_SIZE < 64) || (INTERCOM_QUEUE_HIGH_SIZE > 65532)
#error "INTERCOM_QUEUE_HIGH_SIZE must be 64..65532"
#endif
#if !defined(INTERCOM_QUEUE_NORMAL_SIZE) || (INTERCOM_QUEUE_NORMAL_SIZE < 64) || (INTERCOM_QUEUE_NORMAL_SIZE > 65532)
#error "INTERCOM_QUEUE_NORMAL_SIZE must be 64..65532"
#endif
#if !defined(INTERCOM_RX_BUF_SIZE) || (INTERCOM_RX_BUF_SIZE < 16) || (INTERCOM_RX_BUF_SIZE > 4096) || \
    ((INTERCOM_RX_BUF_SIZE & (INTERCOM_RX_BUF_SIZE - 1)) != 0)
#error "INTERCOM_RX_BUF_SIZE must be a power of two, 16..4096"
#endif

/* ------------------------------------------------------------------------ */
/* Frame format                                                               */
/*   SYNC | DST | SRC | CTRL | LEN | PAYLOAD[LEN] | CRC16 (big endian)      */
/*   CRC16-CCITT (poly 0x1021, init 0xFFFF) over DST..PAYLOAD                */
/*   CTRL = TYPE[7:6] | FIRST[5] | reserved[4] | SEQ[3:0]                    */
/* ------------------------------------------------------------------------ */
#define IC_SYNC          0xA5u
#define IC_HDR_LEN       5u
#define IC_CRC_LEN       2u
#define IC_OVERHEAD      (IC_HDR_LEN + IC_CRC_LEN)
#define IC_FRAME_MAX     (IC_OVERHEAD + INTERCOM_MAX_PAYLOAD)

#define IC_FT_POLL       0u
#define IC_FT_RESP       1u
#define IC_FT_DATA       2u
#define IC_FT_ACK        3u

#define IC_CTRL(type, first, seq) \
    ((uint8_t)(((type) << 6) | ((first) ? 0x20u : 0u) | ((seq) & 0x0Fu)))
#define IC_CTRL_TYPE(c)  ((uint8_t)((c) >> 6))
#define IC_CTRL_FIRST(c) (((c) & 0x20u) != 0u)
#define IC_CTRL_SEQ(c)   ((uint8_t)((c) & 0x0Fu))

/* Payload prefixes */
#define IC_ACKB_VALID    0x80u   /* ack byte: bit7 valid, bits3..0 acked seq */
#define IC_MASK_BYTES    ((INTERCOM_MAX_NODES + 7u) / 8u)
#define IC_POLL_PREFIX   1u                      /* ack byte                  */
#define IC_RESP_PREFIX   (1u + IC_MASK_BYTES)    /* ack byte + NACK node mask */
#define IC_TLV_HDR       2u                      /* id + len                  */
#define IC_MAX_MSG_LEN   (INTERCOM_MAX_PAYLOAD - IC_RESP_PREFIX - IC_TLV_HDR)

/* ------------------------------------------------------------------------ */
/* Message descriptors generated from the user lists                         */
/* ------------------------------------------------------------------------ */
#define IC_X_COUNT_(name, id, maxLen) + 1
#define IC_STATE_COUNT   (0 INTERCOM_STATE_LIST(IC_X_COUNT_))
#define IC_EVENT_COUNT   (0 INTERCOM_EVENT_LIST(IC_X_COUNT_))

#define IC_X_STATE_FIELD_(name, id, maxLen) uint8_t name[maxLen];
typedef struct
{
    uint8_t ic_reserved_; /* keeps the struct non-empty when there are no states */
    INTERCOM_STATE_LIST(IC_X_STATE_FIELD_)
} ic_state_data_t;

typedef struct
{
    uint8_t  id;
    uint8_t  maxLen;
    uint16_t offset;      /* states only: offset in ic_state_data_t */
} ic_msg_desc_t;

/* ------------------------------------------------------------------------ */
/* Event queue: byte ring of variable size records, never split at the end  */
/* ------------------------------------------------------------------------ */
typedef uint32_t ic_mask_t;   /* one bit per node index */

typedef struct
{
    ic_mask_t pending;   /* nodes that have not acknowledged it yet         */
    ic_mask_t inflight;  /* nodes with an unacknowledged frame containing it */
    uint32_t  stamp;     /* ms, time of Intercom_Send()                     */
    uint8_t   msgId;
    uint8_t   len;
    uint8_t   dst;       /* original destination address (for onDrop)       */
    uint8_t   flags;     /* IC_REC_*                                        */
} ic_rec_t;              /* followed by len data bytes, padded to 4         */

#define IC_REC_VALID     0x01u
#define IC_REC_SKIP      0x02u
#define IC_REC_HDR       ((uint16_t)sizeof(ic_rec_t))

typedef struct
{
    uint32_t *mem;
    uint16_t  size;      /* bytes, multiple of 4 */
    uint16_t  head;      /* oldest record (consumer: Intercom_Process)   */
    uint16_t  tail;      /* next free byte (producer: Intercom_Send)     */
} ic_queue_t;

void      ic_q_init(ic_queue_t *q, uint32_t *mem, uint16_t sizeBytes);
uint16_t  ic_q_rec_size(uint8_t len);
ic_rec_t *ic_q_rec(const ic_queue_t *q, uint16_t off);
uint8_t  *ic_q_data(ic_rec_t *rec);
bool      ic_q_alloc(ic_queue_t *q, uint16_t need, uint16_t *off);   /* caller locks */
uint16_t  ic_q_first(const ic_queue_t *q, uint16_t head, uint16_t tail); /* head+tail snapshot taken under lock */
uint16_t  ic_q_next(const ic_queue_t *q, uint16_t off, uint16_t tail);
uint16_t  ic_q_used(const ic_queue_t *q);

/* ------------------------------------------------------------------------ */
/* Per node channel: stop-and-wait state in both directions                  */
/* ------------------------------------------------------------------------ */
/* A reference to a message held in an unacknowledged frame:                 */
/*   bits 15..14 kind, bits 13..0 queue offset / 4 or state index            */
#define IC_REF_HIGH      0u
#define IC_REF_NORMAL    1u
#define IC_REF_STATE     2u
#define IC_REF(kind, v)  ((uint16_t)(((unsigned)(kind) << 14) | ((unsigned)(v) & 0x3FFFu)))
#define IC_REF_KIND(r)   ((uint16_t)((r) >> 14))
#define IC_REF_VAL(r)    ((uint16_t)((r) & 0x3FFFu))

typedef struct
{
    uint16_t inflight[INTERCOM_MAX_MSGS_PER_FRAME];
    uint8_t  nInflight;  /* > 0: last content frame not acknowledged yet */
    uint8_t  txSeq;
    uint8_t  txFirst;    /* FIRST flag until the first ack after init     */
    uint8_t  rxSeq;
    uint8_t  rxValid;
    uint8_t  rxLastFirst;
    uint8_t  respSeq;    /* master: last RESP seq received from this slave */
    uint8_t  respValid;
    uint8_t  failStreak; /* consecutive failures (slave: ACK, master: RESP) */
} ic_chan_t;

/* A node failing this many times in a row is treated as unreachable:       */
/* slaves try it last and without retries, the master polls it only every   */
/* IC_DEAD_POLL_DIV-th cycle.                                               */
#define IC_DEAD_STREAK   3u
#define IC_DEAD_POLL_DIV 4u

/* ------------------------------------------------------------------------ */
/* Instance                                                                   */
/* ------------------------------------------------------------------------ */
typedef enum
{
    IC_TX_IDLE = 0,
    IC_TX_WAIT_GAP,
    IC_TX_BUSY
} ic_tx_state_t;

typedef enum
{
    IC_L_IDLE = 0,
    IC_S_DATA_TX,
    IC_S_WAIT_ACK,
    IC_S_RESP_TX,
    IC_S_ACK_TX,
    IC_M_POLL_TX,
    IC_M_WAIT_RESP
} ic_link_state_t;

typedef enum
{
    IC_P_SYNC = 0,
    IC_P_HDR,
    IC_P_PAYLOAD,
    IC_P_CRC
} ic_parse_state_t;

typedef struct
{
    IntercomCfg cfg;
    bool        initialized;

    /* topology: index 0 = master, 1..n = slaves in polling order */
    uint8_t   nodeAddr[INTERCOM_MAX_NODES];
    uint8_t   nodeCount;
    uint8_t   selfIdx;
    ic_mask_t othersMask;

    /* RX ring (ISR producer, Process consumer) */
    volatile uint8_t  rxBuf[INTERCOM_RX_BUF_SIZE];
    volatile uint16_t rxHead;
    volatile uint16_t rxTail;
    volatile uint32_t rxOverflows;

    /* parser */
    ic_parse_state_t pState;
    uint16_t pIdx;
    uint32_t pLastMs;
    uint8_t  pFrame[IC_FRAME_MAX];   /* DST SRC CTRL LEN PAYLOAD CRC */

    /* transmitter */
    ic_tx_state_t    txState;
    const uint8_t   *txPtr;
    uint16_t         txLen;
    uint16_t         echoIdx;
    bool             echoErr;
    volatile uint8_t txDoneFlag;
    uint32_t         txStartMs;
    uint32_t         lastTxEndMs;
    uint32_t         lastRxMs;
    uint8_t          txFrame[IC_FRAME_MAX];
    uint8_t          ackFrame[IC_OVERHEAD];

    /* event queues */
    ic_queue_t q[2];                 /* [IC_REF_HIGH], [IC_REF_NORMAL] */
    uint32_t   qMemHigh[(INTERCOM_QUEUE_HIGH_SIZE + 3) / 4];
    uint32_t   qMemNormal[(INTERCOM_QUEUE_NORMAL_SIZE + 3) / 4];

    /* state mailboxes */
    ic_state_data_t stData;
    uint8_t   stLen[IC_STATE_COUNT + 1];
    ic_mask_t stDirty[IC_STATE_COUNT + 1];

    ic_chan_t chan[INTERCOM_MAX_NODES];

    /* link layer */
    ic_link_state_t lState;
    uint32_t  lTimer;
    /* slave */
    uint32_t  slotStart;
    uint8_t   slotPeer;  /* node index being served                         */
    uint8_t   slotK;     /* iteration counter over the other slaves         */
    uint8_t   slotRr;    /* round-robin start, advanced every slot          */
    uint8_t   slotPass;  /* 0: reachable peers, 1: peers considered dead    */
    uint8_t   retries;
    ic_mask_t slotNack;
    uint8_t   pollSeq;
    /* master */
    uint8_t   mCur;
    uint32_t  cycleStart;
    uint32_t  cycleCount;

    uint32_t  lastHousekeepMs;
    Intercom_Stats stats;
} ic_t;

/* ------------------------------------------------------------------------ */
/* Internal API (used by Intercom.c and the tests)                           */
/* ------------------------------------------------------------------------ */
void            ic_default_cfg(IntercomCfg *cfg);
Intercom_Status ic_init(ic_t *ic, const IntercomCfg *cfg);
void            ic_process(ic_t *ic);
Intercom_Status ic_send(ic_t *ic, uint8_t dst, uint8_t msgId, const void *data,
                        uint8_t len, Intercom_Prio prio);
Intercom_Status ic_publish(ic_t *ic, uint8_t msgId, const void *data, uint8_t len);
void            ic_rx_byte_isr(ic_t *ic, uint8_t byte);
void            ic_tx_done_isr(ic_t *ic);
void            ic_get_stats(const ic_t *ic, Intercom_Stats *out);

uint16_t        ic_crc16(const uint8_t *data, uint16_t len);

#endif /* IC_INTERNAL_H */
