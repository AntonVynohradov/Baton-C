/**
 * @file baton_defs.h
 * @brief Baton wire-format constants, control-byte helpers and byte-order helpers.
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
 * @defgroup baton_defs Definitions
 * @ingroup baton
 * @brief Frame layout, frame types, payload prefixes and byte-order helpers.
 *
 * @details The wire format is described in `docs/protocol.md`:
 *          `SYNC | DST | SRC | CTRL | LEN | PAYLOAD[LEN] | CRC16 (big endian)`,
 *          CRC-16/CCITT-FALSE over DST..PAYLOAD,
 *          `CTRL = TYPE[7:6] | FIRST[5] | reserved[4] | SEQ[3:0]`.
 * @{
 */

#ifndef BATON_DEFS_H
#define BATON_DEFS_H

/** @name Includes */
/** @{ */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @} */

/** @name Frame layout */
/** @{ */

/** First byte of every frame. */
#define BATON_SYNC 0xA5U
/** SYNC, DST, SRC, CTRL and LEN. */
#define BATON_HEADER_SIZE 5U
/** CRC-16, transmitted high byte first. */
#define BATON_CRC_SIZE 2U
/** Bytes of a frame that are not payload. */
#define BATON_FRAME_OVERHEAD (BATON_HEADER_SIZE + BATON_CRC_SIZE)
/** Destination address meaning "every other node on the bus". */
#define BATON_ADDR_BROADCAST 0xFFU

/** @} */

/** @name Frame types (CTRL bits 7..6) */
/** @{ */

/** Master to slave: grants the bus for one slot, may carry messages. */
#define BATON_FT_POLL 0U
/** Slave to master: ends the slot, may carry messages. */
#define BATON_FT_RESP 1U
/** Slave to slave: messages, acknowledged with ACK. */
#define BATON_FT_DATA 2U
/** Slave to slave: acknowledges one DATA frame. */
#define BATON_FT_ACK 3U

/** @} */

/** @name Payload prefixes and message records */
/** @{ */

/** ACKB byte: bit 7 = valid, bits 3..0 = acknowledged sequence number. */
#define BATON_ACKB_VALID 0x80U
/** Mask of the sequence number in CTRL and ACKB. */
#define BATON_SEQ_MASK 0x0FU
/** One message inside a payload: id, length, data (TLV). */
#define BATON_TLV_HEADER_SIZE 2U

/** @} */

/** @name Control byte */
/** @{ */

/**
 * @brief Build a control byte.
 * @param type Frame type (BATON_FT_*), 0..3.
 * @param first FIRST flag: set until the receiver acknowledged a frame after a restart.
 * @param seq Sequence number, only bits 3..0 are used.
 * @return Control byte.
 */
static inline uint8_t baton_ctrl(uint8_t type, bool first, uint8_t seq)
{
    const uint8_t first_bit = first ? 0x20U : 0x00U;

    return (uint8_t) ((uint8_t) ((uint8_t) (type & 0x03U) << 6U) | first_bit
                      | (uint8_t) (seq & BATON_SEQ_MASK));
}

/** @brief Frame type of a control byte. */
static inline uint8_t baton_ctrl_type(uint8_t ctrl)
{
    return (uint8_t) (ctrl >> 6U);
}

/** @brief FIRST flag of a control byte. */
static inline bool baton_ctrl_first(uint8_t ctrl)
{
    return (ctrl & 0x20U) != 0U;
}

/** @brief Sequence number of a control byte. */
static inline uint8_t baton_ctrl_seq(uint8_t ctrl)
{
    return (uint8_t) (ctrl & BATON_SEQ_MASK);
}

/** @} */

/**
 * @name Byte-order helpers
 * @details The CRC is transmitted big endian; node masks inside a RESP are
 *          little endian.
 */
/** @{ */

/** @brief Read a big-endian 16-bit value. */
static inline uint16_t baton_get_u16be(const uint8_t* data)
{
    return (uint16_t) ((uint16_t) ((uint16_t) data[0] << 8U) | (uint16_t) data[1]);
}

/** @brief Write a 16-bit value in big-endian order. */
static inline void baton_put_u16be(uint8_t* data, uint16_t value)
{
    data[0] = (uint8_t) (value >> 8U);
    data[1] = (uint8_t) (value & 0xFFU);
}

/** @brief Read a little-endian 32-bit value. */
static inline uint32_t baton_get_u32le(const uint8_t* data)
{
    return (uint32_t) data[0] | ((uint32_t) data[1] << 8U) | ((uint32_t) data[2] << 16U)
           | ((uint32_t) data[3] << 24U);
}

/** @brief Write a 32-bit value in little-endian order. */
static inline void baton_put_u32le(uint8_t* data, uint32_t value)
{
    data[0] = (uint8_t) (value & 0xFFU);
    data[1] = (uint8_t) ((value >> 8U) & 0xFFU);
    data[2] = (uint8_t) ((value >> 16U) & 0xFFU);
    data[3] = (uint8_t) (value >> 24U);
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BATON_DEFS_H */

/** @} */
