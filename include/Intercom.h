/**
 * @file    Intercom.h
 * @brief   Intercom - deterministic master/slave messaging over a single-wire UART bus.
 *
 * This is the ONLY library header the application needs. It pulls in the
 * user configuration file "IntercomConfig.h" (copied from
 * config/IntercomConfig_template.h into the application project).
 *
 * Threading model:
 *   - Intercom_Process(), Intercom_Init(), Intercom_GetStats() run from one
 *     context (main loop or a single RTOS task).
 *   - Intercom_RxByteIsr() / Intercom_TxDoneIsr() are called from UART ISRs.
 *   - Intercom_Send() / Intercom_Publish() may be called from any context
 *     ONLY if enterCritical/exitCritical are provided in IntercomCfg.
 *
 * No function blocks, no dynamic memory is used.
 */
#ifndef INTERCOM_H
#define INTERCOM_H

#include <stdint.h>
#include <stdbool.h>
#include "IntercomConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Normally defined by cmake/version.cmake from the vX.Y.Z Git tag; these are
   the fallback values for builds without CMake (keep in sync with it). */
#ifndef INTERCOM_VERSION_MAJOR
#define INTERCOM_VERSION_MAJOR 1
#endif
#ifndef INTERCOM_VERSION_MINOR
#define INTERCOM_VERSION_MINOR 0
#endif
#ifndef INTERCOM_VERSION_PATCH
#define INTERCOM_VERSION_PATCH 0
#endif

/** Destination address meaning "every other node on the bus". */
#define INTERCOM_ADDR_BROADCAST 0xFFu

/* ------------------------------------------------------------------------ */
/* Message identifiers generated from the user message lists                 */
/* ------------------------------------------------------------------------ */
#define INTERCOM_X_MSG_ENUM_(name, id, maxLen) INTERCOM_MSG_##name = (id),
#define INTERCOM_X_MSG_SIZE_(name, id, maxLen) INTERCOM_MSG_##name##_MAX_LEN = (maxLen),

/** Message identifiers: INTERCOM_MSG_<NAME>. */
typedef enum
{
    INTERCOM_STATE_LIST(INTERCOM_X_MSG_ENUM_)
    INTERCOM_EVENT_LIST(INTERCOM_X_MSG_ENUM_)
    INTERCOM_MSG_INVALID_ = 0x100
} Intercom_MsgId;

/** Maximum payload lengths: INTERCOM_MSG_<NAME>_MAX_LEN. */
enum
{
    INTERCOM_STATE_LIST(INTERCOM_X_MSG_SIZE_)
    INTERCOM_EVENT_LIST(INTERCOM_X_MSG_SIZE_)
    INTERCOM_MSG_SIZE_DUMMY_ = 0
};

/* ------------------------------------------------------------------------ */
/* Types                                                                      */
/* ------------------------------------------------------------------------ */
typedef enum
{
    INTERCOM_OK            =  0,
    INTERCOM_ERR_PARAM     = -1, /**< Invalid argument or configuration. */
    INTERCOM_ERR_NOT_INIT  = -2, /**< Intercom_Init() not called / failed. */
    INTERCOM_ERR_MSG_ID    = -3, /**< Unknown id, or wrong class (state vs event). */
    INTERCOM_ERR_LEN       = -4, /**< Payload longer than the message's max length. */
    INTERCOM_ERR_ADDR      = -5, /**< Unknown destination or own address. */
    INTERCOM_ERR_FULL      = -6  /**< Event queue has no room; try again later. */
} Intercom_Status;

typedef enum
{
    INTERCOM_ROLE_MASTER = 0,
    INTERCOM_ROLE_SLAVE  = 1
} Intercom_Role;

typedef enum
{
    INTERCOM_PRIO_NORMAL = 0,
    INTERCOM_PRIO_HIGH   = 1
} Intercom_Prio;

/** Runtime configuration passed to Intercom_Init(). Copied internally. */
typedef struct
{
    /* ---- Topology ---- */
    Intercom_Role  role;
    uint8_t        ownAddr;      /**< This node's address (master: == masterAddr). */
    uint8_t        masterAddr;   /**< Address of the master. */
    const uint8_t *slaveAddrs;   /**< Slaves in polling order, identical on every node. */
    uint8_t        slaveCount;   /**< 1 .. INTERCOM_MAX_NODES-1 */

    /* ---- Timing (milliseconds unless stated) ---- */
    uint32_t baudrate;           /**< Used to compute frame durations. */
    uint16_t slotTimeMs;         /**< Max time a slave owns the bus after POLL. */
    uint16_t ackTimeoutMs;       /**< Wait for ACK after DATA frame sent. */
    uint8_t  maxRetries;         /**< DATA retransmissions per slot (0 = none). */
    uint8_t  txGapMs;            /**< Bus silence required before transmitting. */
    uint8_t  interByteTimeoutMs; /**< Silence that aborts a partially received frame. */
    uint16_t cyclePeriodMs;      /**< Master: min time between cycle starts (0 = back to back). */
    uint32_t eventTtlMs;         /**< Undelivered event is dropped after this (0 = never). */

    /* ---- UART ---- */
    bool rxEcho;                 /**< true: the UART receives its own TX bytes (single-wire
                                      half duplex). Library detects TX end and collisions
                                      from the echo. false: call Intercom_TxDoneIsr(). */

    /* ---- Hardware hooks (required) ---- */
    /** Monotonic millisecond counter. Must be ISR-safe if Send/Publish are used in ISRs. */
    uint32_t (*getTimeMs)(void *ctx);
    /** Start a non-blocking transmission. The buffer stays valid until TX is done.
     *  Return false if the UART could not start. */
    bool     (*uartStartTx)(const uint8_t *data, uint16_t len, void *ctx);

    /* ---- Optional hooks (NULL = unused). Both or neither. ---- */
    void (*enterCritical)(void *ctx);
    void (*exitCritical)(void *ctx);

    /* ---- Application callbacks (optional, called from Intercom_Process()) ---- */
    /* Node mask convention: bit 0 = master, bit i (i >= 1) = slaveAddrs[i - 1]. */

    /** A message (state or event) was received. */
    void (*onMessage)(uint8_t srcAddr, uint8_t msgId, const uint8_t *data, uint8_t len, void *ctx);
    /** An event was not delivered within eventTtlMs. dstAddr = address given to
     *  Intercom_Send(), missedNodes = node mask of nodes that did not get it. */
    void (*onDrop)(uint8_t msgId, uint8_t dstAddr, uint32_t missedNodes, void *ctx);
    /** Master only: one poll slot finished. nackNodes = node mask of nodes that
     *  did not acknowledge DATA from the polled slave during its slot. */
    void (*onPollDone)(uint8_t slaveAddr, bool responded, uint32_t nackNodes, void *ctx);

    void *ctx;                   /**< Passed to every hook and callback. */
} IntercomCfg;

/** Diagnostic counters. */
typedef struct
{
    uint32_t txFrames;
    uint32_t rxFrames;          /**< Valid frames seen on the bus (any destination). */
    uint32_t crcErrors;
    uint32_t rxTimeouts;        /**< Frames aborted by inter-byte timeout. */
    uint32_t rxOverflows;       /**< Bytes lost: RX ring buffer full. */
    uint32_t collisions;        /**< Echo mismatch while transmitting. */
    uint32_t txTimeouts;
    uint32_t ackTimeouts;
    uint32_t retries;
    uint32_t respTimeouts;      /**< Master: slave did not answer a POLL. */
    uint32_t duplicates;        /**< Retransmitted content filtered out. */
    uint32_t badMessages;       /**< Unknown id / bad length inside a frame. */
    uint32_t eventsDropped;     /**< TTL expiry. */
    uint32_t queueFull;         /**< Intercom_Send() rejected for lack of space. */
    uint16_t queueHighUsed;     /**< Bytes currently in use. */
    uint16_t queueNormalUsed;
} Intercom_Stats;

/* ------------------------------------------------------------------------ */
/* API                                                                        */
/* ------------------------------------------------------------------------ */

/** Fill cfg with defaults for 9600 bit/s. Topology and hooks must still be set. */
void Intercom_GetDefaultCfg(IntercomCfg *cfg);

/** Validate cfg and start the library. Can be called again to restart. */
Intercom_Status Intercom_Init(const IntercomCfg *cfg);

/** Run the protocol. Call often (at least every 1 ms is recommended). Never blocks. */
void Intercom_Process(void);

/**
 * Queue an event for reliable delivery (ACK + retry, exactly once, FIFO per
 * destination and priority). Data is copied; the call returns immediately.
 * @param dstAddr  node address or INTERCOM_ADDR_BROADCAST
 */
Intercom_Status Intercom_Send(uint8_t dstAddr, uint8_t msgId, const void *data,
                              uint8_t len, Intercom_Prio prio);

/**
 * Update a state value. The latest value is delivered to every other node;
 * intermediate values may be skipped. Never fails for lack of space.
 */
Intercom_Status Intercom_Publish(uint8_t msgId, const void *data, uint8_t len);

/** Call from the UART RX interrupt for every received byte. */
void Intercom_RxByteIsr(uint8_t byte);

/** Call from the UART TX-complete interrupt (only needed when rxEcho == false). */
void Intercom_TxDoneIsr(void);

/** Copy diagnostic counters. */
void Intercom_GetStats(Intercom_Stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* INTERCOM_H */
