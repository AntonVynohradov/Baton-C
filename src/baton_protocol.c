/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Codec: CRC-16/CCITT-FALSE, incremental frame parser and encoder
 *
 * Author:      Anton Vynohradov
 * Email:       avynohradov@systemfromscratch.com
 *
 * License:     Apache License 2.0
 *
 * Copyright (c) 2026 Anton Vynohradov
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
 ******************************************************************************/

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <string.h>

#include "baton_protocol.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

/** CRC-16/CCITT-FALSE polynomial. */
#define BATON_CRC_POLY 0x1021U

/** Parser states. */
#define PARSE_SYNC BATON_PARSER_IDLE
#define PARSE_HEADER 1U
#define PARSE_PAYLOAD 2U
#define PARSE_CRC_HIGH 3U
#define PARSE_CRC_LOW 4U

/** Header bytes after SYNC: DST, SRC, CTRL, LEN. */
#define PARSE_HEADER_BYTES 4U

/* ==============================================================================
 * PRIVATE FUNCTION DECLARATIONS (static)
 * =============================================================================*/

static int parse_header(struct baton_parser* parser, uint8_t byte);
static int parse_payload(struct baton_parser* parser, uint8_t byte);
static int parse_crc_low(struct baton_parser* parser, uint8_t byte);

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

/** Store one of DST, SRC, CTRL, LEN; reject an invalid LEN as soon as it is read. */
static int parse_header(struct baton_parser* parser, uint8_t byte)
{
    int result = 0;

    parser->crc = baton_crc16_update(parser->crc, byte);
    switch (parser->index)
    {
        case 0U:
            parser->frame.dst = byte;
            parser->index = 1U;
            break;
        case 1U:
            parser->frame.src = byte;
            parser->index = 2U;
            break;
        case 2U:
            parser->frame.ctrl = byte;
            parser->index = 3U;
            break;
        case 3U:
            parser->index = 0U;
            if (byte > BATON_MAX_PAYLOAD)
            {
                parser->state = PARSE_SYNC;
                result = BATON_PARSE_ERROR;
            }
            else
            {
                parser->frame.length = byte;
                parser->state = (byte == 0U) ? PARSE_CRC_HIGH : PARSE_PAYLOAD;
            }
            break;
        default:
            /* Corrupted parser structure: drop the frame instead of writing anywhere. */
            parser->state = PARSE_SYNC;
            result = BATON_PARSE_ERROR;
            break;
    }

    return result;
}

/** Store one payload byte; the bound check also catches a corrupted parser structure. */
static int parse_payload(struct baton_parser* parser, uint8_t byte)
{
    int result = 0;

    if ((parser->index >= (uint16_t) parser->frame.length)
        || (parser->frame.length > BATON_MAX_PAYLOAD))
    {
        parser->state = PARSE_SYNC;
        result = BATON_PARSE_ERROR;
    }
    else
    {
        parser->crc = baton_crc16_update(parser->crc, byte);
        parser->frame.payload[parser->index] = byte;
        parser->index++;
        if (parser->index == (uint16_t) parser->frame.length)
        {
            parser->state = PARSE_CRC_HIGH;
        }
    }

    return result;
}

/** Last CRC byte: the frame is complete, valid or not. */
static int parse_crc_low(struct baton_parser* parser, uint8_t byte)
{
    const uint16_t received = (uint16_t) ((uint16_t) (parser->index << 8U) | (uint16_t) byte);

    parser->state = PARSE_SYNC;
    parser->index = 0U;

    return (received == parser->crc) ? 1 : BATON_PARSE_ERROR;
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

uint16_t baton_crc16_update(uint16_t crc, uint8_t byte)
{
    uint16_t value = (uint16_t) (crc ^ (uint16_t) ((uint16_t) byte << 8U));

    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        if ((value & 0x8000U) != 0U)
        {
            value = (uint16_t) ((uint16_t) (value << 1U) ^ BATON_CRC_POLY);
        }
        else
        {
            value = (uint16_t) (value << 1U);
        }
    }

    return value;
}

uint16_t baton_crc16(const uint8_t* data, size_t length)
{
    uint16_t crc = BATON_CRC_INIT;

    if (data != NULL)
    {
        for (size_t i = 0U; i < length; ++i)
        {
            crc = baton_crc16_update(crc, data[i]);
        }
    }

    return crc;
}

void baton_parser_init(struct baton_parser* parser)
{
    if (parser != NULL)
    {
        (void) memset(parser, 0, sizeof(*parser));
        parser->state = PARSE_SYNC;
        parser->crc = BATON_CRC_INIT;
    }
}

int baton_parser_feed(struct baton_parser* parser, uint8_t byte)
{
    int result = BATON_PARSE_ERROR;

    if (parser != NULL)
    {
        result = 0;
        switch (parser->state)
        {
            case PARSE_SYNC:
                if (byte == BATON_SYNC)
                {
                    parser->state = PARSE_HEADER;
                    parser->index = 0U;
                    parser->crc = BATON_CRC_INIT;
                }
                break;
            case PARSE_HEADER:
                result = parse_header(parser, byte);
                break;
            case PARSE_PAYLOAD:
                result = parse_payload(parser, byte);
                break;
            case PARSE_CRC_HIGH:
                /* index keeps the received high byte until the low byte arrives */
                parser->index = (uint16_t) byte;
                parser->state = PARSE_CRC_LOW;
                break;
            case PARSE_CRC_LOW:
                result = parse_crc_low(parser, byte);
                break;
            default:
                parser->state = PARSE_SYNC;
                result = BATON_PARSE_ERROR;
                break;
        }
    }

    return result;
}

int baton_encode(uint8_t dst, uint8_t src, uint8_t ctrl, const uint8_t* payload,
                 size_t payload_length, uint8_t* output, size_t output_capacity,
                 size_t* output_length)
{
    int result = BATON_PARSE_ERROR;
    const size_t frame_length = payload_length + BATON_FRAME_OVERHEAD;

    if ((output != NULL) && (output_length != NULL) && (payload_length <= BATON_MAX_PAYLOAD)
        && ((payload != NULL) || (payload_length == 0U)) && (output_capacity >= frame_length))
    {
        output[0] = BATON_SYNC;
        output[1] = dst;
        output[2] = src;
        output[3] = ctrl;
        output[4] = (uint8_t) payload_length;
        if ((payload_length > 0U) && (payload != &output[BATON_HEADER_SIZE]))
        {
            (void) memmove(&output[BATON_HEADER_SIZE], payload, payload_length);
        }
        baton_put_u16be(&output[BATON_HEADER_SIZE + payload_length],
                        baton_crc16(&output[1], payload_length + (BATON_HEADER_SIZE - 1U)));
        *output_length = frame_length;
        result = 0;
    }

    return result;
}
