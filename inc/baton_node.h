/**
 * @file baton_node.h
 * @brief Baton node: master/slave bus schedule, reliable events, state mailboxes.
 *
 * Project: Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Author: Anton Vynohradov
 * Email: avynohradov@systemfromscratch.com
 *
 * @copyright Copyright (c) 2026 Anton Vynohradov
 * @license Apache License 2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup baton_node Node
 * @ingroup baton
 * @brief One bus node (master or slave) on top of the @ref baton_link.
 *
 * @details The master passes the baton (POLL) to the slaves in a fixed order;
 *          only the polled slave transmits, for at most slot_time_ms, so the
 *          bus never has collisions. Messages are of two classes, defined by
 *          the application message lists in `baton_user_config.h`:
 *          @li EVENT: queued (HIGH or NORMAL priority), acknowledged,
 *              retransmitted, delivered exactly once and in order per
 *              (source, destination, priority), dropped after event_ttl_ms
 *              with an on_drop report;
 *          @li STATE: a mailbox per state; the latest value is delivered to
 *              every other node, intermediate values may be skipped, it can
 *              never overflow.
 *
 *          Threading model: baton_node_init(), baton_node_process() and
 *          baton_node_get_stats() run in one context (main loop or one RTOS
 *          task). baton_node_rx_isr() and baton_node_tx_done_isr() are called
 *          from the UART interrupts. baton_node_send() and baton_node_publish()
 *          may be called from any context only when enter_critical and
 *          exit_critical are configured.
 *
 *          All memory is inside struct baton_node, owned by the caller. No
 *          function blocks, allocates memory or reads a clock.
 *
 * @par Public interface includes:
 * @li Lifecycle: @ref baton_node_default_config, @ref baton_node_init.
 * @li Processing: @ref baton_node_process.
 * @li Messages: @ref baton_node_send, @ref baton_node_publish.
 * @li Interrupt entry points: @ref baton_node_rx_isr, @ref baton_node_tx_done_isr.
 * @li Diagnostics: @ref baton_node_get_stats.
 * @{
 */

#ifndef BATON_NODE_H
#define BATON_NODE_H

/** @name Includes */
/** @{ */

#include <stdbool.h>
#include <stdint.h>

#include "baton_config.h"
#include "baton_defs.h"
#include "baton_link.h"
#include "baton_queue.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @} */

/** @name Return codes */
/** @{ */

#define BATON_OK 0              /**< Success. */
#define BATON_ERR_PARAM (-1)    /**< Invalid argument or configuration. */
#define BATON_ERR_NOT_INIT (-2) /**< baton_node_init() was not called or failed. */
#define BATON_ERR_MSG_ID (-3)   /**< Unknown message id, or wrong class (state vs event). */
#define BATON_ERR_LEN (-4)      /**< Data longer than the maximum length of the message. */
#define BATON_ERR_ADDR (-5)     /**< Unknown destination or own address. */
#define BATON_ERR_FULL (-6)     /**< Event queue has no room; try again later. */

/** @} */

/** @name Roles and priorities */
/** @{ */

#define BATON_ROLE_MASTER 0U /**< The node that polls the slaves. */
#define BATON_ROLE_SLAVE 1U  /**< A node that transmits only when polled. */

#define BATON_PRIO_NORMAL 0U /**< NORMAL event queue. */
#define BATON_PRIO_HIGH 1U   /**< HIGH event queue, packed before NORMAL events. */

/** @} */

/** @name Derived sizes */
/** @{ */

/** Bytes of a node mask inside a RESP. */
#define BATON_MASK_BYTES ((BATON_MAX_NODES + 7U) / 8U)
/** POLL payload prefix: ACKB. */
#define BATON_POLL_PREFIX 1U
/** RESP payload prefix: ACKB and the NACK node mask. */
#define BATON_RESP_PREFIX (1U + BATON_MASK_BYTES)
/** Largest MAX_LEN of a message: one message must fit into any frame type. */
#define BATON_MAX_MSG_LEN (BATON_MAX_PAYLOAD - BATON_RESP_PREFIX - BATON_TLV_HEADER_SIZE)

/** A node failing this many times in a row is treated as unreachable. */
#define BATON_DEAD_STREAK 3U
/** The master polls an unreachable slave only every BATON_DEAD_POLL_DIV-th cycle. */
#define BATON_DEAD_POLL_DIV 4U

/** @} */

/**
 * @name Message identifiers generated from the application message lists
 * @details For every `X(NAME, ID, MAX_LEN)` the library defines
 *          `BATON_MSG_<NAME>` (the id) and `BATON_MSG_<NAME>_MAX_LEN`.
 */
/** @{ */

/** @cond INTERNAL */
/* Deviation DEV-04 (Rule 20.10): token pasting builds the public names from the lists. */
/* cppcheck-suppress misra-c2012-20.10 */
#define BATON_X_MSG_ENUM_(name, id, max_length) BATON_MSG_##name = (id),
/* cppcheck-suppress misra-c2012-20.10 */
#define BATON_X_MSG_SIZE_(name, id, max_length) BATON_MSG_##name##_MAX_LEN = (max_length),
#define BATON_X_COUNT_(name, id, max_length) +1U
#define BATON_X_STATE_SIZE_(name, id, max_length) +(uint32_t) (max_length)
/** @endcond */

/** Message identifiers: BATON_MSG_<NAME>. */
enum baton_msg_id
{
    BATON_STATE_LIST(BATON_X_MSG_ENUM_) BATON_EVENT_LIST(BATON_X_MSG_ENUM_) BATON_MSG_INVALID_ =
        0x100
};

/** Maximum data lengths: BATON_MSG_<NAME>_MAX_LEN. */
enum baton_msg_max_len
{
    BATON_STATE_LIST(BATON_X_MSG_SIZE_) BATON_EVENT_LIST(BATON_X_MSG_SIZE_) BATON_MSG_SIZE_DUMMY_ =
        0
};

/** Number of STATE messages. */
#define BATON_STATE_COUNT (0U BATON_STATE_LIST(BATON_X_COUNT_))
/** Number of EVENT messages. */
#define BATON_EVENT_COUNT (0U BATON_EVENT_LIST(BATON_X_COUNT_))
/** Bytes of the state mailboxes (one spare byte keeps the array non-empty). */
#define BATON_STATE_DATA_SIZE (1U BATON_STATE_LIST(BATON_X_STATE_SIZE_))

/** @} */

/** @name Type definitions */
/** @{ */

/** @brief Enter or leave the critical section shared with interrupts. */
typedef void (*baton_critical_fn)(void* context);

/**
 * @brief A message (state or event) was received.
 * @details Called from baton_node_process(). @p data is valid only during the call.
 */
typedef void (*baton_message_fn)(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                                 uint8_t length);

/**
 * @brief An event was not delivered within event_ttl_ms.
 * @param context Value of baton_node_config::context.
 * @param msg_id Message identifier.
 * @param dst Address given to baton_node_send().
 * @param missed_nodes Node mask of the nodes that did not acknowledge it (a node may still
 *                     have received it when only its acknowledgement was lost).
 */
typedef void (*baton_drop_fn)(void* context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes);

/**
 * @brief Master only: one poll slot finished.
 * @param context Value of baton_node_config::context.
 * @param slave Address of the polled slave.
 * @param responded The slave answered with a RESP.
 * @param nack_nodes Node mask of the nodes that did not acknowledge DATA from that slave.
 */
typedef void (*baton_poll_done_fn)(void* context, uint8_t slave, bool responded,
                                   uint32_t nack_nodes);

/**
 * @brief Node configuration passed to @ref baton_node_init; copied.
 * @details Node mask convention: bit 0 = master, bit i (i >= 1) = slave_addresses[i - 1].
 */
struct baton_node_config
{
    /* Topology */
    uint8_t role;                   /**< BATON_ROLE_MASTER or BATON_ROLE_SLAVE. */
    uint8_t own_address;            /**< Address of this node (master: master_address). */
    uint8_t master_address;         /**< Address of the master. */
    const uint8_t* slave_addresses; /**< Slaves in polling order, identical on every node. */
    uint8_t slave_count;            /**< 1..BATON_MAX_NODES - 1. */

    /* Timing (milliseconds unless stated) */
    uint32_t baudrate;             /**< Bit rate, used to compute frame durations. */
    uint16_t slot_time_ms;         /**< Longest time a slave owns the bus after a POLL. */
    uint16_t ack_timeout_ms;       /**< Wait for an ACK after a DATA frame. */
    uint8_t max_retries;           /**< DATA retransmissions per slot (0 = none). */
    uint8_t tx_gap_ms;             /**< Bus silence required before transmitting. */
    uint8_t inter_byte_timeout_ms; /**< Silence that aborts a partially received frame. */
    uint16_t cycle_period_ms; /**< Master: shortest time between cycle starts (0 = back to back). */
    uint32_t event_ttl_ms;    /**< An undelivered event is dropped after this (0 = never). */

    /* UART */
    bool rx_echo;         /**< true: the UART receives its own bytes (single wire);
                               false: call baton_node_tx_done_isr(). */
    baton_write_fn write; /**< Starts a non-blocking transmission (required). */

    /* Critical section (optional, both or neither) */
    baton_critical_fn enter_critical; /**< Disable the UART interrupt (or all interrupts). */
    baton_critical_fn exit_critical;  /**< Restore it. */

    /* Application callbacks (optional, called from baton_node_process()) */
    baton_message_fn on_message;     /**< A message was received. */
    baton_drop_fn on_drop;           /**< An event expired. */
    baton_poll_done_fn on_poll_done; /**< Master: a slot finished. */

    void* context; /**< Passed to every hook and callback. */
};

/** @brief Diagnostic counters, see baton_node_get_stats(). */
struct baton_stats
{
    uint32_t tx_frames;         /**< Transmissions started. */
    uint32_t rx_frames;         /**< Valid frames seen on the bus (any destination). */
    uint32_t crc_errors;        /**< Frames rejected: CRC mismatch or invalid LEN. */
    uint32_t rx_timeouts;       /**< Frames aborted by the inter-byte timeout. */
    uint32_t rx_overflows;      /**< Bytes lost because the receive ring was full. */
    uint32_t collisions;        /**< Echo mismatch while transmitting. */
    uint32_t tx_timeouts;       /**< Transmissions that failed to start or did not finish. */
    uint32_t ack_timeouts;      /**< Slave: no ACK for a DATA frame. */
    uint32_t retries;           /**< Slave: DATA retransmissions. */
    uint32_t resp_timeouts;     /**< Master: a slave did not answer a POLL. */
    uint32_t duplicates;        /**< Retransmitted content filtered out. */
    uint32_t bad_messages;      /**< Unknown id or bad length inside a frame. */
    uint32_t events_dropped;    /**< Events dropped by the TTL. */
    uint32_t queue_full;        /**< baton_node_send() rejected for lack of space. */
    uint16_t queue_high_used;   /**< Bytes in use in the HIGH queue. */
    uint16_t queue_normal_used; /**< Bytes in use in the NORMAL queue. */
};

/** @brief Stop-and-wait state of the channel to one peer, both directions. Private. */
struct baton_channel
{
    uint16_t inflight[BATON_MAX_MSGS_PER_FRAME]; /**< Messages of the unacknowledged frame. */
    uint8_t inflight_count; /**< > 0: the last frame with content is not acknowledged yet. */
    uint8_t tx_seq;         /**< Sequence number of the next or outstanding frame. */
    bool tx_first;          /**< FIRST flag until the first acknowledgement after init. */
    uint8_t rx_seq;         /**< Sequence number of the last delivered frame. */
    bool rx_valid;          /**< rx_seq is valid. */
    bool rx_last_first;     /**< The last delivered frame had the FIRST flag. */
    uint8_t resp_seq;       /**< Master: sequence number of the last RESP with content. */
    bool resp_valid;        /**< Master: resp_seq is valid. */
    uint8_t fail_streak;    /**< Consecutive failures (slave: ACK, master: RESP). */
};

/** @brief Node state. Treat the members as private; use the functions below. */
struct baton_node
{
    struct baton_node_config config; /**< Copy of the configuration. */
    struct baton_link link;          /**< Byte-level link. */
    bool initialized;                /**< baton_node_init() succeeded. */

    uint8_t node_address[BATON_MAX_NODES]; /**< Index 0 = master, 1..n = slaves. */
    uint8_t node_count;                    /**< Master + slaves. */
    uint8_t self_index;                    /**< Index of this node. */
    uint32_t others_mask;                  /**< Node mask of all other nodes. */

    uint8_t tx_frame[BATON_FRAME_MAX];       /**< POLL, RESP or DATA being sent. */
    uint8_t ack_frame[BATON_FRAME_OVERHEAD]; /**< ACK being sent. */

    struct baton_queue queue[2];                          /**< [HIGH], [NORMAL]. */
    uint8_t queue_high_memory[BATON_QUEUE_HIGH_SIZE];     /**< HIGH queue storage. */
    uint8_t queue_normal_memory[BATON_QUEUE_NORMAL_SIZE]; /**< NORMAL queue storage. */

    uint8_t state_data[BATON_STATE_DATA_SIZE];    /**< State mailboxes, in list order. */
    uint8_t state_length[BATON_STATE_COUNT + 1U]; /**< Current length of each state. */
    uint32_t state_dirty[BATON_STATE_COUNT + 1U]; /**< Node mask: still to be sent to. */

    struct baton_channel channel[BATON_MAX_NODES]; /**< One channel per node index. */

    uint8_t link_state;  /**< Master/slave state machine. */
    uint32_t link_timer; /**< Start of the current wait. */

    uint32_t slot_start; /**< Slave: time of the POLL that started the slot. */
    uint8_t slot_peer;   /**< Slave: node index being served. */
    uint8_t slot_k;      /**< Slave: iteration counter over the other slaves. */
    uint8_t slot_rr;     /**< Slave: round-robin start, advanced every slot. */
    uint8_t slot_pass;   /**< Slave: 0 = reachable peers, 1 = peers considered dead. */
    uint8_t retries;     /**< Slave: retransmissions to slot_peer in this slot. */
    uint32_t slot_nack;  /**< Slave: peers that did not acknowledge in this slot. */
    uint8_t poll_seq;    /**< Slave: sequence number of the last POLL. */

    uint8_t current;      /**< Master: index of the polled slave - 1. */
    uint32_t cycle_start; /**< Master: start of the current cycle. */
    uint32_t cycle_count; /**< Master: number of cycles started. */

    uint32_t now_ms;            /**< Time of the last baton_node_process(). */
    uint32_t last_housekeep_ms; /**< Last TTL scan. */
    struct baton_stats stats;   /**< Node-level counters. */
};

/** @} */

/** @name Public function declarations */
/** @{ */

/**
 * @brief Fill @p config with defaults for 9600 bit/s.
 * @details Topology, write function and callbacks must still be set.
 * @param config Configuration to fill; NULL is ignored.
 */
void baton_node_default_config(struct baton_node_config* config);

/**
 * @brief Validate the configuration and start the node. Can be called again to restart.
 * @param node Node state.
 * @param config Configuration copied into the node.
 * @param now_ms Current time.
 * @retval BATON_OK Node ready.
 * @retval BATON_ERR_PARAM NULL argument, inconsistent topology, duplicate message ids,
 *         a timing that does not allow one maximum-size exchange per slot, or an event
 *         that does not fit into a queue.
 */
int baton_node_init(struct baton_node* node, const struct baton_node_config* config,
                    uint32_t now_ms);

/**
 * @brief Run the protocol. Call often (every 1 ms is recommended). Never blocks.
 * @param node Node state; ignored when NULL or not initialized.
 * @param now_ms Current time.
 */
void baton_node_process(struct baton_node* node, uint32_t now_ms);

/**
 * @brief Queue an event for reliable delivery. The data is copied.
 *
 * @details The TTL of the event starts at the time of the last
 *          baton_node_process() call.
 *
 * @param node Node state.
 * @param dst Node address or BATON_ADDR_BROADCAST.
 * @param msg_id EVENT message identifier.
 * @param data Data bytes, or NULL when @p length is zero.
 * @param length Number of data bytes (at most the MAX_LEN of the message).
 * @param priority BATON_PRIO_NORMAL or BATON_PRIO_HIGH.
 * @return BATON_OK or BATON_ERR_* (see the return codes).
 */
int baton_node_send(struct baton_node* node, uint8_t dst, uint8_t msg_id, const void* data,
                    uint8_t length, uint8_t priority);

/**
 * @brief Update a state value; the latest value is delivered to every other node.
 * @param node Node state.
 * @param msg_id STATE message identifier.
 * @param data Data bytes, or NULL when @p length is zero.
 * @param length Number of data bytes (at most the MAX_LEN of the message).
 * @return BATON_OK or BATON_ERR_* (see the return codes). Never fails for lack of space.
 */
int baton_node_publish(struct baton_node* node, uint8_t msg_id, const void* data, uint8_t length);

/**
 * @brief Store one received byte. Call from the UART receive interrupt.
 * @param node Node state; NULL is ignored.
 * @param byte Received byte.
 */
void baton_node_rx_isr(struct baton_node* node, uint8_t byte);

/**
 * @brief Signal the end of a transmission (only needed when rx_echo is false).
 * @param node Node state; NULL is ignored.
 */
void baton_node_tx_done_isr(struct baton_node* node);

/**
 * @brief Copy the diagnostic counters.
 * @param node Node state.
 * @param stats Destination; nothing is written when an argument is NULL.
 */
void baton_node_get_stats(const struct baton_node* node, struct baton_stats* stats);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BATON_NODE_H */

/** @} */
