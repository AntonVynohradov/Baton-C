/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Half-duplex link: receive ring, parser with inter-byte timeout,
 *              transmitter with bus-gap wait, echo check and timeout
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

#include "baton_link.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

/** Transmitter states. */
#define TX_IDLE 0U
#define TX_WAIT_GAP 1U
#define TX_BUSY 2U

/** Index mask of the receive ring (its size is a power of two). */
#define RX_MASK (BATON_RX_BUF_SIZE - 1U)

/** Margin added to the expected duration of a transmission before it times out. */
#define TX_TIMEOUT_MARGIN_MS 5U

/* ==============================================================================
 * PRIVATE FUNCTION DECLARATIONS (static)
 * =============================================================================*/

static int tx_finish(struct baton_link* link, uint32_t now_ms);
static int rx_parse(struct baton_link* link, uint8_t byte, uint32_t now_ms);
static bool rx_echo(struct baton_link* link, uint8_t byte, uint32_t now_ms);
static int tx_start(struct baton_link* link, uint32_t now_ms);

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

/** End the current transmission and report it. */
static int tx_finish(struct baton_link* link, uint32_t now_ms)
{
    link->tx_state = TX_IDLE;
    link->last_tx_end_ms = now_ms;
    if (link->echo_error)
    {
        link->counters.collisions++;
    }

    return BATON_LINK_TX_DONE;
}

/** Feed one byte from another node to the parser. */
static int rx_parse(struct baton_link* link, uint8_t byte, uint32_t now_ms)
{
    int event = BATON_LINK_NONE;
    int parsed;

    if (((uint32_t) (now_ms - link->last_rx_ms) > (uint32_t) link->config.inter_byte_timeout_ms)
        && (link->parser.state != BATON_PARSER_IDLE))
    {
        link->counters.rx_timeouts++;
        baton_parser_init(&link->parser);
    }
    link->last_rx_ms = now_ms;

    parsed = baton_parser_feed(&link->parser, byte);
    if (parsed > 0)
    {
        link->counters.rx_frames++;
        event = BATON_LINK_FRAME;
    }
    else if (parsed < 0)
    {
        link->counters.crc_errors++;
    }
    else
    {
        /* frame incomplete: wait for more bytes */
    }

    return event;
}

/**
 * Compare one byte with the echo of the own transmission.
 * @return true when the byte was an echo byte and the echo is now complete.
 */
static bool rx_echo(struct baton_link* link, uint8_t byte, uint32_t now_ms)
{
    bool complete = false;

    if (byte != link->tx_data[link->echo_index])
    {
        link->echo_error = true;
    }
    link->echo_index++;
    if (link->echo_index >= link->tx_length)
    {
        (void) tx_finish(link, now_ms);
        complete = true;
    }

    return complete;
}

/** Start the requested transmission once the bus has been silent long enough. */
static int tx_start(struct baton_link* link, uint32_t now_ms)
{
    int event = BATON_LINK_NONE;
    const uint32_t gap = (uint32_t) link->config.tx_gap_ms;

    if ((!baton_link_rx_busy(link, now_ms)) && ((uint32_t) (now_ms - link->last_rx_ms) >= gap)
        && ((uint32_t) (now_ms - link->last_tx_end_ms) >= gap))
    {
        link->tx_state = TX_BUSY;
        link->tx_done_flag = false;
        link->echo_index = 0U;
        link->echo_error = false;
        link->tx_start_ms = now_ms;
        link->counters.tx_frames++;
        if (link->config.write(link->config.context, link->tx_data, (size_t) link->tx_length) < 0)
        {
            link->counters.tx_timeouts++;
            event = tx_finish(link, now_ms);
        }
    }

    return event;
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

int baton_link_init(struct baton_link* link, const struct baton_link_config* config,
                    uint32_t now_ms)
{
    int result = BATON_PARSE_ERROR;

    if ((link != NULL) && (config != NULL) && (config->write != NULL) && (config->baudrate != 0U)
        && (config->inter_byte_timeout_ms != 0U))
    {
        (void) memset(link, 0, sizeof(*link));
        link->config = *config;
        baton_parser_init(&link->parser);
        link->tx_state = TX_IDLE;
        link->last_rx_ms = now_ms;
        link->last_tx_end_ms = now_ms;
        result = 0;
    }

    return result;
}

void baton_link_rx_isr(struct baton_link* link, uint8_t byte)
{
    if (link != NULL)
    {
        const uint16_t head = link->rx_head;
        uint16_t next = head;

        next++;
        next &= RX_MASK;

        if (next == link->rx_tail)
        {
            link->rx_overflows++;
        }
        else
        {
            link->rx_buffer[head] = byte;
            link->rx_head = next;
        }
    }
}

void baton_link_tx_done_isr(struct baton_link* link)
{
    if (link != NULL)
    {
        link->tx_done_flag = true;
    }
}

int baton_link_receive(struct baton_link* link, uint32_t now_ms)
{
    int event = BATON_LINK_NONE;

    if (link != NULL)
    {
        uint16_t tail = link->rx_tail;
        bool done = false;

        while ((!done) && (tail != link->rx_head))
        {
            const uint8_t byte = link->rx_buffer[tail];

            tail++;
            tail &= RX_MASK;
            link->rx_tail = tail;

            /* While TX_BUSY, echo_index < tx_length: the last echo byte ends the transmission. */
            if ((link->tx_state == TX_BUSY) && link->config.rx_echo)
            {
                if (rx_echo(link, byte, now_ms))
                {
                    event = BATON_LINK_TX_DONE;
                    done = true;
                }
            }
            else
            {
                event = rx_parse(link, byte, now_ms);
                done = (event != BATON_LINK_NONE);
            }
        }
    }

    return event;
}

int baton_link_transmit(struct baton_link* link, uint32_t now_ms)
{
    int event = BATON_LINK_NONE;

    if (link != NULL)
    {
        if (link->tx_state == TX_WAIT_GAP)
        {
            event = tx_start(link, now_ms);
        }
        else if (link->tx_state == TX_BUSY)
        {
            const uint32_t limit = baton_link_frame_ms(link, (uint32_t) link->tx_length)
                                   + (uint32_t) link->config.tx_gap_ms + TX_TIMEOUT_MARGIN_MS;

            if ((!link->config.rx_echo) && link->tx_done_flag)
            {
                event = tx_finish(link, now_ms);
            }
            else if ((uint32_t) (now_ms - link->tx_start_ms) > limit)
            {
                link->counters.tx_timeouts++;
                event = tx_finish(link, now_ms);
            }
            else
            {
                /* still transmitting */
            }
        }
        else
        {
            /* idle */
        }
    }

    return event;
}

int baton_link_send(struct baton_link* link, const uint8_t* data, size_t length)
{
    int result = BATON_PARSE_ERROR;

    if ((link != NULL) && (data != NULL) && (length > 0U) && (length <= BATON_FRAME_MAX))
    {
        link->tx_data = data;
        link->tx_length = (uint16_t) length;
        link->tx_state = TX_WAIT_GAP;
        result = 0;
    }

    return result;
}

void baton_link_cancel(struct baton_link* link)
{
    if ((link != NULL) && (link->tx_state == TX_WAIT_GAP))
    {
        link->tx_state = TX_IDLE;
    }
}

const struct baton_frame* baton_link_frame(const struct baton_link* link)
{
    const struct baton_frame* frame = NULL;

    if (link != NULL)
    {
        frame = &link->parser.frame;
    }

    return frame;
}

bool baton_link_rx_busy(const struct baton_link* link, uint32_t now_ms)
{
    bool busy = false;

    if (link != NULL)
    {
        busy = (link->parser.state != BATON_PARSER_IDLE)
               && ((uint32_t) (now_ms - link->last_rx_ms)
                   <= (uint32_t) link->config.inter_byte_timeout_ms);
    }

    return busy;
}

uint32_t baton_link_frame_ms(const struct baton_link* link, uint32_t bytes)
{
    uint32_t ms = 0U;

    if (link != NULL)
    {
        ms = ((bytes * 10000U) + link->config.baudrate - 1U) / link->config.baudrate;
    }

    return ms;
}
