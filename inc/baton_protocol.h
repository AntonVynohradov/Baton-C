/**
 * @file baton_protocol.h
 * @brief Baton codec: CRC-16/CCITT-FALSE, incremental frame parser and frame encoder.
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
 * @defgroup baton_protocol Codec
 * @ingroup baton
 * @brief CRC-16, incremental stream parser and frame encoder.
 *
 * @details Pure functions over caller-owned buffers: no state outside the
 *          structures passed in, safe to use from several contexts on
 *          different structures.
 *
 * @par Public interface includes:
 * @li CRC: @ref baton_crc16, @ref baton_crc16_update.
 * @li Parser: @ref baton_parser_init, @ref baton_parser_feed.
 * @li Encoder: @ref baton_encode.
 * @{
 */

#ifndef BATON_PROTOCOL_H
#define BATON_PROTOCOL_H

/** @name Includes */
/** @{ */

#include <stddef.h>
#include <stdint.h>

#include "baton_config.h"
#include "baton_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @} */

/** @name Frame limits and errors */
/** @{ */

/** Largest complete frame on the wire, SYNC to CRC. */
#define BATON_FRAME_MAX (BATON_FRAME_OVERHEAD + BATON_MAX_PAYLOAD)

/** Initial value of the CRC-16/CCITT-FALSE register. */
#define BATON_CRC_INIT 0xFFFFU

/** Value of baton_parser::state while the parser waits for SYNC (between frames). */
#define BATON_PARSER_IDLE 0U

/** Error returned for invalid input, malformed frames, or insufficient capacity. */
#define BATON_PARSE_ERROR (-1)

/** @} */

/** @name Frame and parser state */
/** @{ */

/** @brief One decoded frame (header fields and payload, without SYNC and CRC). */
struct baton_frame
{
    uint8_t dst;                        /**< Destination address. */
    uint8_t src;                        /**< Source address. */
    uint8_t ctrl;                       /**< Control byte, see baton_ctrl(). */
    uint8_t length;                     /**< Number of valid bytes in payload. */
    uint8_t payload[BATON_MAX_PAYLOAD]; /**< Payload bytes. */
};

/**
 * @brief Incremental parser state for one byte stream.
 *
 * @details The parser decodes directly into @ref baton_parser::frame, so a
 *          decoded frame needs no second buffer. Keep one instance per input
 *          stream and initialize it before feeding bytes.
 */
struct baton_parser
{
    struct baton_frame frame; /**< Frame being received; valid after feed returned 1. */
    uint16_t crc;             /**< Running CRC over DST..PAYLOAD. */
    uint16_t index;           /**< Bytes of the current field already received. */
    uint8_t state;            /**< Internal state; BATON_PARSER_IDLE between frames. */
};

/** @} */

/** @name CRC and frame parsing */
/** @{ */

/**
 * @brief Add one byte to a CRC-16/CCITT-FALSE register.
 *
 * @param crc Current register value (BATON_CRC_INIT before the first byte).
 * @param byte Next byte.
 * @return Updated register value.
 */
uint16_t baton_crc16_update(uint16_t crc, uint8_t byte);

/**
 * @brief Calculate the CRC-16/CCITT-FALSE of a buffer (poly 0x1021, init 0xFFFF).
 *
 * @param data Bytes covered by the CRC.
 * @param length Number of bytes to process.
 * @return Calculated CRC; BATON_CRC_INIT when @p data is NULL.
 */
uint16_t baton_crc16(const uint8_t* data, size_t length);

/**
 * @brief Reset a parser before starting or restarting a byte stream.
 * @param parser Parser state to initialize; NULL is ignored.
 */
void baton_parser_init(struct baton_parser* parser);

/**
 * @brief Feed one byte to the incremental frame parser.
 *
 * @details The parser waits for SYNC, reads the header, rejects a LEN above
 *          BATON_MAX_PAYLOAD as soon as it is read, then reads the payload and
 *          the CRC and validates it. After a valid or invalid frame it waits for
 *          the next SYNC. Bytes of a rejected frame are not re-scanned for SYNC;
 *          the inter-byte timeout of the link resynchronizes a broken stream.
 *
 * @param parser Parser state.
 * @param byte Next byte from the input stream.
 * @retval 1 A complete valid frame is available in @p parser->frame until the next call.
 * @retval 0 More bytes are required.
 * @retval BATON_PARSE_ERROR NULL parser, LEN out of range, CRC mismatch or corrupted state.
 */
int baton_parser_feed(struct baton_parser* parser, uint8_t byte);

/** @} */

/** @name Frame encoding */
/** @{ */

/**
 * @brief Encode one frame: SYNC, header, payload and CRC.
 *
 * @details @p payload may point to `&output[BATON_HEADER_SIZE]`: a payload
 *          that was built in place is then not copied.
 *
 * @param dst Destination address.
 * @param src Source address.
 * @param ctrl Control byte, see baton_ctrl().
 * @param payload Payload bytes, or NULL when @p payload_length is zero.
 * @param payload_length Number of payload bytes (at most BATON_MAX_PAYLOAD).
 * @param output Destination buffer for the complete frame.
 * @param output_capacity Capacity of @p output in bytes.
 * @param output_length Receives the frame length on success.
 * @retval 0 Frame encoded.
 * @retval BATON_PARSE_ERROR Invalid input or insufficient output capacity.
 */
int baton_encode(uint8_t dst, uint8_t src, uint8_t ctrl, const uint8_t* payload,
                 size_t payload_length, uint8_t* output, size_t output_capacity,
                 size_t* output_length);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BATON_PROTOCOL_H */

/** @} */
