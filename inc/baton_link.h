/**
 * @file baton_link.h
 * @brief Baton half-duplex link: receive ring, parser with timeout, transmitter.
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
 * @defgroup baton_link Link
 * @ingroup baton
 * @brief Byte-level half-duplex link between the UART and the node layer.
 *
 * @details The link owns the receive ring (filled from the UART interrupt), a
 *          parser with an inter-byte timeout and a transmitter that waits for a
 *          silent bus, checks the echo of its own bytes on a single-wire UART
 *          and detects a missing end of transmission. It reports what happened
 *          as events; the node layer (@ref baton_node) uses it, applications
 *          normally do not call it directly.
 *
 *          The library never reads a clock: every function that needs the time
 *          takes `now_ms`, a monotonic millisecond counter that may wrap.
 *
 * @par Public interface includes:
 * @li Lifecycle: @ref baton_link_init.
 * @li Interrupt entry points: @ref baton_link_rx_isr, @ref baton_link_tx_done_isr.
 * @li Processing: @ref baton_link_receive, @ref baton_link_transmit,
 *     @ref baton_link_send, @ref baton_link_cancel.
 * @li Queries: @ref baton_link_frame, @ref baton_link_rx_busy, @ref baton_link_frame_ms.
 * @{
 */

#ifndef BATON_LINK_H
#define BATON_LINK_H

/** @name Includes */
/** @{ */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "baton_protocol.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @} */

/** @name Link events */
/** @{ */

/** Nothing happened; the receive ring is empty or the transmitter has nothing to report. */
#define BATON_LINK_NONE 0
/** A valid frame was received; read it with baton_link_frame(). */
#define BATON_LINK_FRAME 1
/** The frame passed to baton_link_send() was transmitted, failed or timed out. */
#define BATON_LINK_TX_DONE 2

/** @} */

/** @name Type definitions */
/** @{ */

/**
 * @brief Start a non-blocking transmission.
 *
 * @details The buffer stays valid and unchanged until the link reports
 *          BATON_LINK_TX_DONE. The UART driver sends the bytes in the
 *          background (interrupt or DMA).
 *
 * @param context Value of baton_link_config::context.
 * @param data Complete frame, SYNC to CRC.
 * @param length Number of bytes in @p data.
 * @return 0 when the transmission was started, a negative value otherwise.
 */
typedef int (*baton_write_fn)(void* context, const uint8_t* data, size_t length);

/** @brief Link configuration passed to @ref baton_link_init. */
struct baton_link_config
{
    baton_write_fn write;          /**< Starts a transmission (required). */
    void* context;                 /**< Passed to write. */
    uint32_t baudrate;             /**< Bit rate; frame durations assume 8N1 (10 bits per byte). */
    uint8_t tx_gap_ms;             /**< Bus silence required before transmitting. */
    uint8_t inter_byte_timeout_ms; /**< Silence that aborts a partially received frame (> 0). */
    bool rx_echo;                  /**< true: the UART receives its own bytes (single wire). */
};

/** @brief Link diagnostic counters. */
struct baton_link_counters
{
    uint32_t tx_frames;   /**< Transmissions started. */
    uint32_t rx_frames;   /**< Valid frames received (any destination). */
    uint32_t crc_errors;  /**< Frames rejected: CRC mismatch or invalid LEN. */
    uint32_t rx_timeouts; /**< Frames aborted by the inter-byte timeout. */
    uint32_t collisions;  /**< Transmissions whose echo did not match. */
    uint32_t tx_timeouts; /**< Transmissions that failed to start or did not finish in time. */
};

/** @brief Link state. Treat the members as private; use the functions below. */
struct baton_link
{
    struct baton_link_config config;     /**< Copy of the configuration. */
    struct baton_parser parser;          /**< Receive stream parser. */
    struct baton_link_counters counters; /**< Diagnostic counters. */

    volatile uint8_t rx_buffer[BATON_RX_BUF_SIZE]; /**< Receive ring (interrupt producer). */
    volatile uint16_t rx_head;                     /**< Next write position (interrupt). */
    volatile uint16_t rx_tail;                     /**< Next read position (process). */
    volatile uint32_t rx_overflows;                /**< Bytes lost because the ring was full. */
    volatile bool tx_done_flag;                    /**< Set by baton_link_tx_done_isr(). */

    const uint8_t* tx_data;  /**< Frame being transmitted. */
    uint16_t tx_length;      /**< Length of tx_data. */
    uint16_t echo_index;     /**< Echo bytes already compared. */
    bool echo_error;         /**< An echo byte did not match. */
    uint8_t tx_state;        /**< Idle, waiting for a silent bus, or busy. */
    uint32_t tx_start_ms;    /**< Start of the current transmission. */
    uint32_t last_tx_end_ms; /**< End of the last transmission. */
    uint32_t last_rx_ms;     /**< Last byte received from another node (fed to the parser). */
};

/** @} */

/** @name Public function declarations */
/** @{ */

/**
 * @brief Initialize a link and reset its parser, ring and counters.
 * @param link Link state to initialize.
 * @param config Configuration copied into the link.
 * @param now_ms Current time.
 * @retval 0 Link ready.
 * @retval BATON_PARSE_ERROR NULL argument, no write function, zero baud rate or timeout.
 */
int baton_link_init(struct baton_link* link, const struct baton_link_config* config,
                    uint32_t now_ms);

/**
 * @brief Store one received byte. Call from the UART receive interrupt.
 * @details A byte that does not fit into the ring is counted and dropped.
 * @param link Link state; NULL is ignored.
 * @param byte Received byte.
 */
void baton_link_rx_isr(struct baton_link* link, uint8_t byte);

/**
 * @brief Signal the end of a transmission. Call from the UART TX-complete interrupt.
 * @details Needed only when baton_link_config::rx_echo is false.
 * @param link Link state; NULL is ignored.
 */
void baton_link_tx_done_isr(struct baton_link* link);

/**
 * @brief Process received bytes until something happens.
 *
 * @details Echo bytes of the own transmission are compared and consumed; the
 *          other bytes go to the parser. Call again after every event until it
 *          returns BATON_LINK_NONE.
 *
 * @param link Link state.
 * @param now_ms Current time.
 * @retval BATON_LINK_FRAME A valid frame is available through baton_link_frame().
 * @retval BATON_LINK_TX_DONE The echo of the own frame is complete.
 * @retval BATON_LINK_NONE The ring is empty (or @p link is NULL).
 */
int baton_link_receive(struct baton_link* link, uint32_t now_ms);

/**
 * @brief Start a requested transmission when the bus is silent; supervise a running one.
 * @param link Link state.
 * @param now_ms Current time.
 * @retval BATON_LINK_TX_DONE The transmission finished, failed to start or timed out.
 * @retval BATON_LINK_NONE Nothing to report (or @p link is NULL).
 */
int baton_link_transmit(struct baton_link* link, uint32_t now_ms);

/**
 * @brief Request the transmission of a complete frame.
 *
 * @details The frame is sent by baton_link_transmit() after the bus has been
 *          silent for tx_gap_ms. The buffer must stay valid until the link
 *          reports BATON_LINK_TX_DONE. A new request replaces a pending one.
 *
 * @param link Link state.
 * @param data Complete frame, SYNC to CRC.
 * @param length Number of bytes (1..BATON_FRAME_MAX).
 * @retval 0 Request stored.
 * @retval BATON_PARSE_ERROR NULL argument or invalid length.
 */
int baton_link_send(struct baton_link* link, const uint8_t* data, size_t length);

/**
 * @brief Drop a requested transmission that has not started yet.
 * @param link Link state; NULL is ignored.
 */
void baton_link_cancel(struct baton_link* link);

/**
 * @brief Last frame reported by BATON_LINK_FRAME.
 * @param link Link state.
 * @return Frame, valid until the next call of baton_link_receive(); NULL when @p link is NULL.
 */
const struct baton_frame* baton_link_frame(const struct baton_link* link);

/**
 * @brief Whether a frame is being received right now.
 * @param link Link state.
 * @param now_ms Current time.
 * @return true when the parser is inside a frame and the inter-byte timeout has not expired.
 */
bool baton_link_rx_busy(const struct baton_link* link, uint32_t now_ms);

/**
 * @brief Duration of @p bytes bytes on the wire (10 bits per byte), rounded up.
 * @param link Link state.
 * @param bytes Number of bytes.
 * @return Milliseconds; 0 when @p link is NULL.
 */
uint32_t baton_link_frame_ms(const struct baton_link* link, uint32_t bytes);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BATON_LINK_H */

/** @} */
