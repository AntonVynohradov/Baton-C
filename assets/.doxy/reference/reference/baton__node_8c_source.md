

# File baton\_node.c

[**File List**](files.md) **>** [**src**](dir_68267d1309a1af8e8297ef4c3efbcdba.md) **>** [**baton\_node.c**](baton__node_8c.md)

[Go to the documentation of this file](baton__node_8c.md)


```C++
/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Node layer: master/slave state machines, message packing (TLV),
 *              stop-and-wait channels, duplicate filtering, event TTL
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

#include <stddef.h>
#include <string.h>

#include "baton_node.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

#define HOUSEKEEP_PERIOD_MS 10U

#define Q_HIGH 0U
#define Q_NORMAL 1U
#define REF_STATE 2U

#define NO_NODE 0xFFU

#define L_IDLE 0U
#define S_DATA_TX 1U
#define S_WAIT_ACK 2U
#define S_RESP_TX 3U
#define S_ACK_TX 4U
#define M_POLL_TX 5U
#define M_WAIT_RESP 6U

#define RESP_TIMEOUT_MARGIN_MS 10U

#define STREAK_MAX 255U

#define X_DESC_(name, id, max_length) {(uint8_t) (id), (uint8_t) (max_length)},

/* One case per message: a duplicate id in the lists is a compile error (duplicate case value). */
#define X_CASE_(name, id, max_length) \
    case (id):                        \
        result = (int) (max_length);  \
        break;

/* Compile-time checks of every message definition. */
#define X_CHECK_(name, id, max_length)                                                  \
    _Static_assert(((uint32_t) (max_length) >= 1U)                                      \
                       && ((uint32_t) (max_length) <= BATON_MAX_MSG_LEN),               \
                   "Baton message MAX_LEN must be 1..BATON_MAX_MSG_LEN");               \
    _Static_assert((uint32_t) (id) <= 255U, "Baton message ID must be 0..255");         \
    _Static_assert((BATON_REC_HEADER_SIZE + (((uint32_t) (max_length) + 3U) & 0xFFFCU)) \
                       < (BATON_QUEUE_HIGH_SIZE & 0xFFFCU),                             \
                   "Baton message does not fit into BATON_QUEUE_HIGH_SIZE");            \
    _Static_assert((BATON_REC_HEADER_SIZE + (((uint32_t) (max_length) + 3U) & 0xFFFCU)) \
                       < (BATON_QUEUE_NORMAL_SIZE & 0xFFFCU),                           \
                   "Baton message does not fit into BATON_QUEUE_NORMAL_SIZE");
/* ==============================================================================
 * PRIVATE TYPE DEFINITIONS
 * =============================================================================*/

struct msg_desc
{
    uint8_t id;
    uint8_t max_length;
};

/* ==============================================================================
 * PRIVATE VARIABLES (static)
 * =============================================================================*/

BATON_STATE_LIST(X_CHECK_)
BATON_EVENT_LIST(X_CHECK_)

static const struct msg_desc state_desc[BATON_STATE_COUNT + 1U] = {
    BATON_STATE_LIST(X_DESC_){0U, 0U}};

/* ==============================================================================
 * PRIVATE FUNCTION DECLARATIONS (static)
 * =============================================================================*/

static void lock(const struct baton_node* node);
static void unlock(const struct baton_node* node);
static uint32_t node_bit(uint8_t index);
static uint8_t node_index(const struct baton_node* node, uint8_t address);
static bool find_desc(const struct msg_desc* table, uint8_t count, uint8_t id, uint8_t* index);
static int msg_max_length(uint8_t id);
static uint16_t state_offset(uint8_t s);
static uint32_t frame_ms(const struct baton_node* node, uint32_t bytes);
static uint32_t min_slot_ms(const struct baton_node* node);

static bool config_valid(const struct baton_node_config* config);
static bool topology_valid(struct baton_node* node);
static bool timing_valid(const struct baton_node* node);

static uint16_t ref_make(uint8_t kind, uint16_t value);
static uint8_t ref_kind(uint16_t ref);
static uint16_t ref_value(uint16_t ref);
static void chan_remove_at(struct baton_channel* channel, uint8_t i);
static void chan_abandon(struct baton_channel* channel);
static void chan_commit(struct baton_node* node, uint8_t peer);
static void chan_drop_ref(struct baton_node* node, uint8_t peer, uint16_t ref);

static uint16_t emit(uint8_t* out, uint16_t n, uint8_t id, const uint8_t* data, uint8_t length);
static bool try_emit_state(struct baton_node* node, uint8_t* out, uint16_t* n, uint16_t limit,
                           uint8_t s, uint32_t clear_bit);
static int pack_again(struct baton_node* node, uint8_t peer, uint8_t* out, uint16_t max_length);
static uint16_t pack_events(struct baton_node* node, uint8_t peer, uint8_t* out, uint16_t n,
                            uint16_t max_length);
static uint16_t pack_states(struct baton_node* node, uint8_t peer, uint8_t* out, uint16_t n,
                            uint16_t max_length);
static int pack(struct baton_node* node, uint8_t peer, uint8_t* out, uint16_t max_length);

static void deliver(struct baton_node* node, uint8_t src_index, const uint8_t* payload,
                    uint16_t length);
static void rx_content(struct baton_node* node, uint8_t src_index, uint8_t ctrl,
                       const uint8_t* payload, uint16_t length);

static void tx_request(struct baton_node* node, uint8_t* buffer, size_t capacity, uint8_t dst,
                       uint8_t ctrl, uint8_t payload_length);
static void on_tx_done(struct baton_node* node, uint32_t now_ms);

static int32_t slave_budget(const struct baton_node* node, uint32_t now_ms);
static int slave_tx_data(struct baton_node* node, uint8_t peer, uint32_t now_ms);
static void slave_send_resp(struct baton_node* node);
static void slave_slot_step(struct baton_node* node, uint32_t now_ms);
static void slave_on_poll(struct baton_node* node, const struct baton_frame* frame,
                          uint32_t now_ms);
static void slave_on_data(struct baton_node* node, uint8_t src_index,
                          const struct baton_frame* frame);
static void slave_on_ack(struct baton_node* node, uint8_t src_index, uint8_t ctrl, uint32_t now_ms);
static void slave_ack_timeout(struct baton_node* node, uint32_t now_ms);

static void master_send_poll(struct baton_node* node);
static void master_poll_done(struct baton_node* node, bool responded, uint32_t nack_nodes);
static void master_on_resp(struct baton_node* node, uint8_t src_index,
                           const struct baton_frame* frame);
static void master_service(struct baton_node* node, uint32_t now_ms);

static void link_service(struct baton_node* node, uint32_t now_ms);
static void handle_frame(struct baton_node* node, const struct baton_frame* frame, uint32_t now_ms);

static void queue_reclaim(struct baton_node* node, uint8_t qi);
static void expire_record(struct baton_node* node, uint8_t qi, uint16_t offset,
                          const struct baton_record* record);
static void expire_events(struct baton_node* node, uint32_t now_ms);
static void housekeeping(struct baton_node* node, uint32_t now_ms);

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

/* ---- Small helpers ---------------------------------------------------------- */

static void lock(const struct baton_node* node)
{
    if (node->config.enter_critical != NULL)
    {
        node->config.enter_critical(node->config.context);
    }
}

static void unlock(const struct baton_node* node)
{
    if (node->config.exit_critical != NULL)
    {
        node->config.exit_critical(node->config.context);
    }
}

static uint32_t node_bit(uint8_t index)
{
    return (uint32_t) 1U << index;
}

static uint8_t node_index(const struct baton_node* node, uint8_t address)
{
    uint8_t result = NO_NODE;

    for (uint8_t i = 0U; (i < node->node_count) && (result == NO_NODE); ++i)
    {
        if (node->node_address[i] == address)
        {
            result = i;
        }
    }

    return result;
}

static bool find_desc(const struct msg_desc* table, uint8_t count, uint8_t id, uint8_t* index)
{
    bool found = false;

    for (uint8_t i = 0U; (i < count) && (!found); ++i)
    {
        if (table[i].id == id)
        {
            *index = i;
            found = true;
        }
    }

    return found;
}

static int msg_max_length(uint8_t id)
{
    int result;

    switch (id)
    {
        BATON_STATE_LIST(X_CASE_)
        BATON_EVENT_LIST(X_CASE_)
        default:
            result = -1;
            break;
    }

    return result;
}

static uint16_t state_offset(uint8_t s)
{
    uint16_t offset = 0U;

    for (uint8_t i = 0U; i < s; ++i)
    {
        offset = (uint16_t) (offset + state_desc[i].max_length);
    }

    return offset;
}

static uint32_t frame_ms(const struct baton_node* node, uint32_t bytes)
{
    return baton_link_frame_ms(&node->link, bytes);
}

static uint32_t min_slot_ms(const struct baton_node* node)
{
    const struct baton_node_config* c = &node->config;

    return frame_ms(node, BATON_FRAME_MAX) + (3U * (uint32_t) c->tx_gap_ms) + 2U
           + (uint32_t) c->ack_timeout_ms
           + frame_ms(node, BATON_FRAME_OVERHEAD + BATON_TLV_HEADER_SIZE + BATON_MAX_MSG_LEN);
}

/* ---- Configuration ---------------------------------------------------------- */

static bool config_valid(const struct baton_node_config* config)
{
    return (config->write != NULL) && (config->slave_addresses != NULL)
           && ((config->enter_critical == NULL) == (config->exit_critical == NULL))
           && (config->slave_count != 0U) && (config->slave_count < BATON_MAX_NODES)
           && (config->baudrate != 0U) && (config->ack_timeout_ms != 0U)
           && (config->inter_byte_timeout_ms != 0U)
           && ((config->role == BATON_ROLE_MASTER) || (config->role == BATON_ROLE_SLAVE));
}

static bool topology_valid(struct baton_node* node)
{
    const struct baton_node_config* c = &node->config;
    bool valid = true;
    uint8_t self;

    node->node_count = (uint8_t) (c->slave_count + 1U);
    node->node_address[0] = c->master_address;
    for (uint8_t i = 0U; i < c->slave_count; ++i)
    {
        node->node_address[i + 1U] = c->slave_addresses[i];
    }
    for (uint8_t i = 0U; i < node->node_count; ++i)
    {
        if (node->node_address[i] == BATON_ADDR_BROADCAST)
        {
            valid = false;
        }
        for (uint8_t j = (uint8_t) (i + 1U); j < node->node_count; ++j)
        {
            if (node->node_address[i] == node->node_address[j])
            {
                valid = false;
            }
        }
    }

    self = node_index(node, c->own_address);
    if ((self == NO_NODE) || ((c->role == BATON_ROLE_MASTER) && (self != 0U))
        || ((c->role == BATON_ROLE_SLAVE) && (self == 0U)))
    {
        valid = false;
    }
    else
    {
        /* node_count is 2..32, so the shift is 0..30 */
        const uint32_t all = 0xFFFFFFFFU >> (32U - (uint32_t) node->node_count);

        node->self_index = self;
        node->others_mask = all & ~node_bit(self);
    }

    return valid;
}

static bool timing_valid(const struct baton_node* node)
{
    const struct baton_node_config* c = &node->config;

    return ((uint32_t) c->ack_timeout_ms
            > (frame_ms(node, BATON_FRAME_OVERHEAD) + (uint32_t) c->tx_gap_ms))
           && ((uint32_t) c->slot_time_ms >= min_slot_ms(node));
}

/* ---- Channels: stop-and-wait bookkeeping ------------------------------------- */

/* A reference to a message in an unacknowledged frame:
 * bits 15..14 kind (Q_HIGH, Q_NORMAL, REF_STATE), bits 13..0 queue offset / 4 or state index. */
static uint16_t ref_make(uint8_t kind, uint16_t value)
{
    return (uint16_t) ((uint16_t) ((uint16_t) kind << 14U) | (uint16_t) (value & 0x3FFFU));
}

static uint8_t ref_kind(uint16_t ref)
{
    return (uint8_t) (ref >> 14U);
}

static uint16_t ref_value(uint16_t ref)
{
    return (uint16_t) (ref & 0x3FFFU);
}

static void chan_remove_at(struct baton_channel* channel, uint8_t i)
{
    for (uint8_t k = i; (uint8_t) (k + 1U) < channel->inflight_count; ++k)
    {
        channel->inflight[k] = channel->inflight[k + 1U];
    }
    channel->inflight_count--;
}

static void chan_abandon(struct baton_channel* channel)
{
    channel->inflight_count = 0U;
    channel->tx_seq = (uint8_t) ((uint8_t) (channel->tx_seq + 1U) & BATON_SEQ_MASK);
}

static void chan_commit(struct baton_node* node, uint8_t peer)
{
    struct baton_channel* channel = &node->channel[peer];
    const uint32_t bit = node_bit(peer);

    for (uint8_t i = 0U; i < channel->inflight_count; ++i)
    {
        const uint16_t ref = channel->inflight[i];
        const uint8_t kind = ref_kind(ref);

        if (kind != REF_STATE)
        {
            struct baton_queue* queue = &node->queue[kind];
            const uint16_t offset = (uint16_t) (ref_value(ref) << 2U);
            struct baton_record record;

            baton_queue_read(queue, offset, &record);
            baton_queue_set_masks(queue, offset, record.pending & ~bit, record.inflight & ~bit);
        }
    }
    channel->inflight_count = 0U;
    channel->tx_seq = (uint8_t) ((uint8_t) (channel->tx_seq + 1U) & BATON_SEQ_MASK);
    channel->tx_first = false;
}

static void chan_drop_ref(struct baton_node* node, uint8_t peer, uint16_t ref)
{
    struct baton_channel* channel = &node->channel[peer];
    uint8_t kept = 0U;

    for (uint8_t i = 0U; i < channel->inflight_count; ++i)
    {
        if (channel->inflight[i] != ref)
        {
            channel->inflight[kept] = channel->inflight[i];
            kept++;
        }
    }
    channel->inflight_count = kept;
    if (kept == 0U)
    {
        chan_abandon(channel);
    }
}

/* ---- Packing messages into a payload (TLV: id, length, data) ------------------ */

static uint16_t emit(uint8_t* out, uint16_t n, uint8_t id, const uint8_t* data, uint8_t length)
{
    out[n] = id;
    out[n + 1U] = length;
    if (length > 0U)
    {
        (void) memcpy(&out[n + BATON_TLV_HEADER_SIZE], data, length);
    }

    return (uint16_t) (n + BATON_TLV_HEADER_SIZE + length);
}

static bool try_emit_state(struct baton_node* node, uint8_t* out, uint16_t* n, uint16_t limit,
                           uint8_t s, uint32_t clear_bit)
{
    bool emitted = false;
    uint8_t length;

    lock(node);
    length = node->state_length[s];
    if (((uint32_t) *n + BATON_TLV_HEADER_SIZE + (uint32_t) length) <= (uint32_t) limit)
    {
        *n = emit(out, *n, state_desc[s].id, &node->state_data[state_offset(s)], length);
        node->state_dirty[s] &= ~clear_bit;
        emitted = true;
    }
    unlock(node);

    return emitted;
}

static int pack_again(struct baton_node* node, uint8_t peer, uint8_t* out, uint16_t max_length)
{
    struct baton_channel* channel = &node->channel[peer];
    uint16_t event_bytes = 0U;
    uint16_t n = 0U;
    int result;

    for (uint8_t i = 0U; i < channel->inflight_count; ++i)
    {
        const uint16_t ref = channel->inflight[i];

        if (ref_kind(ref) != REF_STATE)
        {
            struct baton_record record;

            baton_queue_read(&node->queue[ref_kind(ref)], (uint16_t) (ref_value(ref) << 2U),
                             &record);
            event_bytes = (uint16_t) (event_bytes + BATON_TLV_HEADER_SIZE + record.length);
        }
    }

    if (event_bytes > max_length)
    {
        result = -1;
    }
    else
    {
        uint8_t i = 0U;

        while (i < channel->inflight_count)
        {
            const uint16_t ref = channel->inflight[i];
            const uint8_t kind = ref_kind(ref);

            if (kind == REF_STATE)
            {
                const uint8_t s = (uint8_t) ref_value(ref);

                if (try_emit_state(node, out, &n, (uint16_t) (max_length - event_bytes), s, 0U))
                {
                    i++;
                }
                else
                {
                    chan_remove_at(channel, i);
                    lock(node);
                    node->state_dirty[s] |= node_bit(peer);
                    unlock(node);
                }
            }
            else
            {
                struct baton_queue* queue = &node->queue[kind];
                const uint16_t offset = (uint16_t) (ref_value(ref) << 2U);
                struct baton_record record;

                baton_queue_read(queue, offset, &record);
                n = emit(out, n, record.msg_id, baton_queue_data(queue, offset), record.length);
                event_bytes = (uint16_t) (event_bytes - (BATON_TLV_HEADER_SIZE + record.length));
                i++;
            }
        }

        if (channel->inflight_count == 0U)
        {
            chan_abandon(channel);
            n = 0U;
        }
        result = (int) n;
    }

    return result;
}

static uint16_t pack_events(struct baton_node* node, uint8_t peer, uint8_t* out, uint16_t n,
                            uint16_t max_length)
{
    struct baton_channel* channel = &node->channel[peer];
    const uint32_t bit = node_bit(peer);
    uint16_t written = n;

    for (uint8_t qi = 0U; (qi < 2U) && (channel->inflight_count < BATON_MAX_MSGS_PER_FRAME); ++qi)
    {
        struct baton_queue* queue = &node->queue[qi];
        uint16_t head;
        uint16_t tail;
        uint16_t offset;
        bool stop = false;

        lock(node);
        head = queue->head;
        tail = queue->tail;
        unlock(node);

        offset = baton_queue_first(queue, head, tail);
        while ((!stop) && (offset != tail))
        {
            struct baton_record record;

            baton_queue_read(queue, offset, &record);
            if ((record.flags & BATON_REC_VALID) == 0U)
            {
                stop = true; /* still being written by the producer */
            }
            else if ((record.pending & bit) == 0U)
            {
                /* not for this peer, or already acknowledged by it */
            }
            else if ((((uint32_t) written + BATON_TLV_HEADER_SIZE + (uint32_t) record.length)
                      > (uint32_t) max_length)
                     || (channel->inflight_count >= BATON_MAX_MSGS_PER_FRAME))
            {
                stop = true; /* keep FIFO order */
            }
            else
            {
                written = emit(out, written, record.msg_id, baton_queue_data(queue, offset),
                               record.length);
                baton_queue_set_masks(queue, offset, record.pending, record.inflight | bit);
                channel->inflight[channel->inflight_count] =
                    ref_make(qi, (uint16_t) (offset >> 2U));
                channel->inflight_count++;
            }
            if (!stop)
            {
                offset = baton_queue_next(queue, offset, tail);
            }
        }
    }

    return written;
}

static uint16_t pack_states(struct baton_node* node, uint8_t peer, uint8_t* out, uint16_t n,
                            uint16_t max_length)
{
    struct baton_channel* channel = &node->channel[peer];
    const uint32_t bit = node_bit(peer);
    uint16_t written = n;

    for (uint8_t s = 0U;
         (s < (uint8_t) BATON_STATE_COUNT) && (channel->inflight_count < BATON_MAX_MSGS_PER_FRAME);
         ++s)
    {
        if ((node->state_dirty[s] & bit) != 0U)
        {
            if (try_emit_state(node, out, &written, max_length, s, bit))
            {
                channel->inflight[channel->inflight_count] = ref_make(REF_STATE, (uint16_t) s);
                channel->inflight_count++;
            }
        }
    }

    return written;
}

static int pack(struct baton_node* node, uint8_t peer, uint8_t* out, uint16_t max_length)
{
    int result = 0;

    if (node->channel[peer].inflight_count > 0U)
    {
        result = pack_again(node, peer, out, max_length);
    }
    if (result == 0)
    {
        const uint16_t n = pack_events(node, peer, out, 0U, max_length);

        result = (int) pack_states(node, peer, out, n, max_length);
    }

    return result;
}

/* ---- Receiving messages --------------------------------------------------------- */

static void deliver(struct baton_node* node, uint8_t src_index, const uint8_t* payload,
                    uint16_t length)
{
    const uint8_t src = node->node_address[src_index];
    uint16_t position = 0U;
    uint16_t left = length;
    bool stop = false;

    while ((!stop) && (left > 0U))
    {
        if (left < BATON_TLV_HEADER_SIZE)
        {
            node->stats.bad_messages++;
            stop = true;
        }
        else
        {
            const uint8_t id = payload[position];
            const uint8_t size = payload[position + 1U];
            const uint16_t record = (uint16_t) ((uint16_t) size + BATON_TLV_HEADER_SIZE);

            if (record > left)
            {
                node->stats.bad_messages++;
                stop = true;
            }
            else
            {
                const int max_length = msg_max_length(id);

                if ((max_length < 0) || ((int) size > max_length))
                {
                    node->stats.bad_messages++;
                }
                else if (node->config.on_message != NULL)
                {
                    node->config.on_message(node->config.context, src, id,
                                            &payload[position + BATON_TLV_HEADER_SIZE], size);
                }
                else
                {
                    /* no receiver */
                }
                position = (uint16_t) (position + record);
                left = (uint16_t) (left - record);
            }
        }
    }
}

static void rx_content(struct baton_node* node, uint8_t src_index, uint8_t ctrl,
                       const uint8_t* payload, uint16_t length)
{
    /* frames without content do not take part in sequencing */
    if (length > 0U)
    {
        struct baton_channel* channel = &node->channel[src_index];
        const uint8_t seq = baton_ctrl_seq(ctrl);
        const bool first = baton_ctrl_first(ctrl);
        bool duplicate;

        if (!channel->rx_valid)
        {
            duplicate = false;
        }
        else if (first)
        {
            duplicate = channel->rx_last_first && (seq == channel->rx_seq);
        }
        else
        {
            duplicate = (seq == channel->rx_seq);
        }

        if (duplicate)
        {
            node->stats.duplicates++;
        }
        else
        {
            channel->rx_valid = true;
            channel->rx_seq = seq;
            channel->rx_last_first = first;
            deliver(node, src_index, payload, length);
        }
    }
}

/* ---- Transmitting --------------------------------------------------------------- */

static void tx_request(struct baton_node* node, uint8_t* buffer, size_t capacity, uint8_t dst,
                       uint8_t ctrl, uint8_t payload_length)
{
    size_t length = 0U;

    (void) baton_encode(dst, node->config.own_address, ctrl, &buffer[BATON_HEADER_SIZE],
                        (size_t) payload_length, buffer, capacity, &length);
    (void) baton_link_send(&node->link, buffer, length);
}

static void on_tx_done(struct baton_node* node, uint32_t now_ms)
{
    switch (node->link_state)
    {
        case S_DATA_TX:
            node->link_state = S_WAIT_ACK;
            node->link_timer = now_ms;
            break;
        case M_POLL_TX:
            node->link_state = M_WAIT_RESP;
            node->link_timer = now_ms;
            break;
        case S_RESP_TX:
        case S_ACK_TX:
            node->link_state = L_IDLE;
            break;
        default:
            /* no transmission of the state machine was outstanding */
            break;
    }
}

/* ---- Slave ------------------------------------------------------------------------- */

static int32_t slave_budget(const struct baton_node* node, uint32_t now_ms)
{
    const struct baton_node_config* c = &node->config;
    const uint32_t elapsed_ms = now_ms - node->slot_start;
    const uint32_t reserve_ms =
        frame_ms(node, BATON_FRAME_MAX) + (2U * (uint32_t) c->tx_gap_ms) + 2U;
    const uint32_t overhead = BATON_FRAME_OVERHEAD;
    const int32_t available = (int32_t) c->slot_time_ms - (int32_t) elapsed_ms
                              - (int32_t) reserve_ms - (int32_t) c->tx_gap_ms
                              - (int32_t) c->ack_timeout_ms;
    int32_t bytes = 0;

    if (available > 0)
    {
        const uint32_t wire_bytes = ((uint32_t) available * c->baudrate) / 10000U;

        bytes = (int32_t) wire_bytes - (int32_t) overhead;
        if (bytes > (int32_t) BATON_MAX_PAYLOAD)
        {
            bytes = (int32_t) BATON_MAX_PAYLOAD;
        }
        if (bytes < 0)
        {
            bytes = 0;
        }
    }

    return bytes;
}

static int slave_tx_data(struct baton_node* node, uint8_t peer, uint32_t now_ms)
{
    const int32_t budget = slave_budget(node, now_ms);
    int result = -1;

    if (budget >= (int32_t) BATON_TLV_HEADER_SIZE)
    {
        result = pack(node, peer, &node->tx_frame[BATON_HEADER_SIZE], (uint16_t) budget);
        if (result > 0)
        {
            const struct baton_channel* channel = &node->channel[peer];

            tx_request(node, node->tx_frame, sizeof(node->tx_frame), node->node_address[peer],
                       baton_ctrl(BATON_FT_DATA, channel->tx_first, channel->tx_seq),
                       (uint8_t) result);
            node->link_state = S_DATA_TX;
            result = 1;
        }
    }

    return result;
}

static void slave_send_resp(struct baton_node* node)
{
    uint8_t* payload = &node->tx_frame[BATON_HEADER_SIZE];
    const struct baton_channel* channel = &node->channel[0];
    const uint16_t room = BATON_MAX_PAYLOAD - BATON_RESP_PREFIX;
    int packed;

    payload[0] = (uint8_t) (BATON_ACKB_VALID | (node->poll_seq & BATON_SEQ_MASK));
    for (uint8_t i = 0U; i < BATON_MASK_BYTES; ++i)
    {
        payload[1U + i] = (uint8_t) ((node->slot_nack >> (8U * i)) & 0xFFU);
    }
    /* The content for the master was packed with the same room before, so a
       retransmission always fits again: pack() does not return -1 here. */
    packed = pack(node, 0U, &payload[BATON_RESP_PREFIX], room);
    tx_request(node, node->tx_frame, sizeof(node->tx_frame), node->node_address[0],
               baton_ctrl(BATON_FT_RESP, channel->tx_first, channel->tx_seq),
               (uint8_t) (BATON_RESP_PREFIX + (uint32_t) packed));
    node->link_state = S_RESP_TX;
}

static void slave_slot_step(struct baton_node* node, uint32_t now_ms)
{
    const uint8_t slaves = (uint8_t) (node->node_count - 1U);
    bool sent = false;

    while ((!sent) && (node->slot_pass < 2U))
    {
        while ((!sent) && (node->slot_k < slaves))
        {
            /* round robin over the slaves so no peer is always served last */
            const uint8_t peer =
                (uint8_t) (1U
                           + (uint8_t) (((uint32_t) node->slot_rr + (uint32_t) node->slot_k)
                                        % (uint32_t) slaves));
            const bool healthy = (node->channel[peer].fail_streak < BATON_DEAD_STREAK);
            const bool first_pass = (node->slot_pass == 0U);

            node->slot_peer = peer;
            if ((peer != node->self_index) && (healthy == first_pass)
                && ((node->slot_nack & node_bit(peer)) == 0U))
            {
                sent = (slave_tx_data(node, peer, now_ms) > 0);
            }
            if (!sent)
            {
                node->slot_k++;
                node->retries = 0U;
            }
        }
        if (!sent)
        {
            node->slot_pass++;
            node->slot_k = 0U;
        }
    }
    if (!sent)
    {
        slave_send_resp(node);
    }
}

static void slave_on_poll(struct baton_node* node, const struct baton_frame* frame, uint32_t now_ms)
{
    if (frame->length < BATON_POLL_PREFIX)
    {
        node->stats.bad_messages++;
    }
    else
    {
        const struct baton_channel* master = &node->channel[0];
        const uint8_t ackb = frame->payload[0];

        if (((ackb & BATON_ACKB_VALID) != 0U) && ((ackb & BATON_SEQ_MASK) == master->tx_seq)
            && (master->inflight_count > 0U))
        {
            chan_commit(node, 0U); /* the master confirmed our previous RESP */
        }
        rx_content(node, 0U, frame->ctrl, &frame->payload[BATON_POLL_PREFIX],
                   (uint16_t) (frame->length - BATON_POLL_PREFIX));

        /* The master is the authority: a POLL always starts a fresh slot. */
        baton_link_cancel(&node->link);
        node->poll_seq = baton_ctrl_seq(frame->ctrl);
        node->slot_start = now_ms;
        node->slot_k = 0U;
        node->slot_rr++;
        node->slot_pass = 0U;
        node->retries = 0U;
        node->slot_nack = 0U;
        slave_slot_step(node, now_ms);
    }
}

static void slave_on_data(struct baton_node* node, uint8_t src_index,
                          const struct baton_frame* frame)
{
    if ((node->link_state == L_IDLE) && (src_index != 0U))
    {
        rx_content(node, src_index, frame->ctrl, frame->payload, (uint16_t) frame->length);
        tx_request(node, node->ack_frame, sizeof(node->ack_frame), node->node_address[src_index],
                   baton_ctrl(BATON_FT_ACK, false, baton_ctrl_seq(frame->ctrl)), 0U);
        node->link_state = S_ACK_TX;
    }
}

static void slave_on_ack(struct baton_node* node, uint8_t src_index, uint8_t ctrl, uint32_t now_ms)
{
    struct baton_channel* channel = &node->channel[src_index];

    if ((node->link_state == S_WAIT_ACK) && (src_index == node->slot_peer)
        && (baton_ctrl_seq(ctrl) == channel->tx_seq) && (channel->inflight_count > 0U))
    {
        chan_commit(node, src_index);
        channel->fail_streak = 0U;
        node->retries = 0U;
        node->link_state = L_IDLE;
        slave_slot_step(node, now_ms); /* same peer first: it may have more */
    }
}

static void slave_ack_timeout(struct baton_node* node, uint32_t now_ms)
{
    const uint8_t peer = node->slot_peer;
    struct baton_channel* channel = &node->channel[peer];
    const uint8_t max_retries =
        (channel->fail_streak >= BATON_DEAD_STREAK) ? 0U : node->config.max_retries;
    bool resent = false;

    node->stats.ack_timeouts++;
    node->link_state = L_IDLE;
    if (node->retries < max_retries)
    {
        node->retries++;
        node->stats.retries++;
        resent = (slave_tx_data(node, peer, now_ms) > 0);
    }
    if (!resent)
    {
        if (channel->inflight_count > 0U)
        {
            node->slot_nack |= node_bit(peer); /* reported to the master in RESP */
            if (channel->fail_streak < STREAK_MAX)
            {
                channel->fail_streak++;
            }
        }
        node->slot_k++;
        node->retries = 0U;
        slave_slot_step(node, now_ms);
    }
}

/* ---- Master ------------------------------------------------------------------------- */

static void master_send_poll(struct baton_node* node)
{
    const uint8_t si = (uint8_t) (node->current + 1U);
    const struct baton_channel* channel = &node->channel[si];
    uint8_t* payload = &node->tx_frame[BATON_HEADER_SIZE];
    const uint8_t valid = channel->resp_valid ? BATON_ACKB_VALID : 0U;
    const uint16_t room = BATON_MAX_PAYLOAD - BATON_POLL_PREFIX;
    int packed;

    payload[0] = (uint8_t) (valid | (channel->resp_seq & BATON_SEQ_MASK));
    /* Same room as before: a retransmission always fits, pack() does not return -1 here. */
    packed = pack(node, si, &payload[BATON_POLL_PREFIX], room);
    tx_request(node, node->tx_frame, sizeof(node->tx_frame), node->node_address[si],
               baton_ctrl(BATON_FT_POLL, channel->tx_first, channel->tx_seq),
               (uint8_t) (BATON_POLL_PREFIX + (uint32_t) packed));
    node->link_state = M_POLL_TX;
}

static void master_poll_done(struct baton_node* node, bool responded, uint32_t nack_nodes)
{
    const uint8_t si = (uint8_t) (node->current + 1U);
    struct baton_channel* channel = &node->channel[si];

    if (responded)
    {
        channel->fail_streak = 0U;
    }
    else if (channel->fail_streak < STREAK_MAX)
    {
        channel->fail_streak++;
    }
    else
    {
        /* saturated */
    }
    node->current++;
    node->link_state = L_IDLE;
    if (node->config.on_poll_done != NULL)
    {
        node->config.on_poll_done(node->config.context, node->node_address[si], responded,
                                  nack_nodes);
    }
}

static void master_on_resp(struct baton_node* node, uint8_t src_index,
                           const struct baton_frame* frame)
{
    if ((node->link_state == M_WAIT_RESP) && (src_index == (uint8_t) (node->current + 1U)))
    {
        if (frame->length < BATON_RESP_PREFIX)
        {
            node->stats.bad_messages++;
        }
        else
        {
            struct baton_channel* channel = &node->channel[src_index];
            const uint8_t ackb = frame->payload[0];
            uint32_t nack = 0U;

            if (((ackb & BATON_ACKB_VALID) != 0U) && ((ackb & BATON_SEQ_MASK) == channel->tx_seq)
                && (channel->inflight_count > 0U))
            {
                chan_commit(node, src_index); /* the slave confirmed our POLL content */
            }
            for (uint8_t i = 0U; i < BATON_MASK_BYTES; ++i)
            {
                const uint32_t mask_byte = (uint32_t) frame->payload[1U + i];
                const uint32_t shift = 8U * (uint32_t) i;

                nack |= (mask_byte << shift);
            }
            if (frame->length > BATON_RESP_PREFIX)
            {
                /* Acknowledge only RESP frames with content: an empty RESP shares the
                   seq of the next content RESP and must not confirm it in advance. */
                channel->resp_seq = baton_ctrl_seq(frame->ctrl);
                channel->resp_valid = true;
            }
            rx_content(node, src_index, frame->ctrl, &frame->payload[BATON_RESP_PREFIX],
                       (uint16_t) (frame->length - BATON_RESP_PREFIX));
            master_poll_done(node, true, nack);
        }
    }
}

static void master_service(struct baton_node* node, uint32_t now_ms)
{
    if (node->link_state == M_WAIT_RESP)
    {
        const uint32_t elapsed = (uint32_t) (now_ms - node->link_timer);
        const uint32_t limit = (uint32_t) node->config.slot_time_ms
                               + (uint32_t) node->config.tx_gap_ms + RESP_TIMEOUT_MARGIN_MS;

        if (((elapsed >= limit) && (!baton_link_rx_busy(&node->link, now_ms)))
            || (elapsed >= (2U * limit)))
        {
            node->stats.resp_timeouts++;
            master_poll_done(node, false, 0U);
        }
    }
    if (node->link_state == L_IDLE)
    {
        bool wait = false;
        bool poll = false;

        /* bounded: at most BATON_DEAD_POLL_DIV cycles are skipped */
        while ((!wait) && (!poll))
        {
            if (node->current >= node->config.slave_count)
            {
                if ((node->config.cycle_period_ms != 0U)
                    && ((uint32_t) (now_ms - node->cycle_start)
                        < (uint32_t) node->config.cycle_period_ms))
                {
                    wait = true;
                }
                else
                {
                    node->current = 0U;
                    node->cycle_start = now_ms;
                    node->cycle_count++;
                }
            }
            if (!wait)
            {
                /* an unreachable slave is polled only every BATON_DEAD_POLL_DIV-th cycle */
                if ((node->channel[node->current + 1U].fail_streak < BATON_DEAD_STREAK)
                    || ((node->cycle_count % BATON_DEAD_POLL_DIV) == 0U))
                {
                    poll = true;
                }
                else
                {
                    node->current++;
                }
            }
        }
        if (poll)
        {
            master_send_poll(node);
        }
    }
}

/* ---- Dispatch ----------------------------------------------------------------------- */

static void link_service(struct baton_node* node, uint32_t now_ms)
{
    if (node->config.role == BATON_ROLE_MASTER)
    {
        master_service(node, now_ms);
    }
    else if (node->link_state == S_WAIT_ACK)
    {
        const uint32_t elapsed = (uint32_t) (now_ms - node->link_timer);
        const uint32_t limit = (uint32_t) node->config.ack_timeout_ms;

        if (((elapsed >= limit) && (!baton_link_rx_busy(&node->link, now_ms)))
            || (elapsed >= (2U * limit)))
        {
            slave_ack_timeout(node, now_ms);
        }
    }
    else
    {
        /* slave idle or transmitting: driven by received frames and the end of transmissions */
    }
}

static void handle_frame(struct baton_node* node, const struct baton_frame* frame, uint32_t now_ms)
{
    const uint8_t si = node_index(node, frame->src);

    if ((frame->dst == node->config.own_address) && (si != NO_NODE) && (si != node->self_index))
    {
        const bool master = (node->config.role == BATON_ROLE_MASTER);

        switch (baton_ctrl_type(frame->ctrl))
        {
            case BATON_FT_POLL:
                if ((!master) && (si == 0U))
                {
                    slave_on_poll(node, frame, now_ms);
                }
                break;
            case BATON_FT_RESP:
                if (master)
                {
                    master_on_resp(node, si, frame);
                }
                break;
            case BATON_FT_DATA:
                if (!master)
                {
                    slave_on_data(node, si, frame);
                }
                break;
            default: /* BATON_FT_ACK */
                if (!master)
                {
                    slave_on_ack(node, si, frame->ctrl, now_ms);
                }
                break;
        }
    }
}

/* ---- Housekeeping: release acknowledged events, expire old ones --------------------- */

static void queue_reclaim(struct baton_node* node, uint8_t qi)
{
    struct baton_queue* queue = &node->queue[qi];
    uint16_t head;
    uint16_t tail;
    uint16_t offset;
    bool stop = false;

    lock(node);
    head = queue->head;
    tail = queue->tail;
    unlock(node);

    offset = baton_queue_first(queue, head, tail);
    while ((!stop) && (offset != tail))
    {
        struct baton_record record;

        /* inflight is a subset of pending: a bit is set in inflight only while it is set in
           pending, and both are cleared together, so pending == 0 means inflight == 0 */
        baton_queue_read(queue, offset, &record);
        if (((record.flags & BATON_REC_VALID) == 0U) || (record.pending != 0U))
        {
            stop = true;
        }
        else
        {
            offset = baton_queue_next(queue, offset, tail);
        }
    }
    /* Only this function moves the head. The producer resets head and tail only
       when the queue is empty, and it is not: records were released from it. */
    if (offset != head)
    {
        lock(node);
        queue->head = offset;
        if (queue->head == queue->tail)
        {
            queue->head = 0U;
            queue->tail = 0U;
        }
        unlock(node);
    }
}

static void expire_record(struct baton_node* node, uint8_t qi, uint16_t offset,
                          const struct baton_record* record)
{
    const uint16_t ref = ref_make(qi, (uint16_t) (offset >> 2U));

    for (uint8_t p = 0U; p < node->node_count; ++p)
    {
        if ((record->inflight & node_bit(p)) != 0U)
        {
            chan_drop_ref(node, p, ref);
        }
    }
    baton_queue_set_masks(&node->queue[qi], offset, 0U, 0U);
    node->stats.events_dropped++;
    if (node->config.on_drop != NULL)
    {
        node->config.on_drop(node->config.context, record->msg_id, record->dst, record->pending);
    }
}

static void expire_events(struct baton_node* node, uint32_t now_ms)
{
    for (uint8_t qi = 0U; qi < 2U; ++qi)
    {
        const struct baton_queue* queue = &node->queue[qi];
        uint16_t head;
        uint16_t tail;
        uint16_t offset;
        bool stop = false;

        lock(node);
        head = queue->head;
        tail = queue->tail;
        unlock(node);

        offset = baton_queue_first(queue, head, tail);
        while ((!stop) && (offset != tail))
        {
            struct baton_record record;

            baton_queue_read(queue, offset, &record);
            if ((record.flags & BATON_REC_VALID) == 0U)
            {
                stop = true;
            }
            else
            {
                if ((record.pending != 0U)
                    && ((uint32_t) (now_ms - record.stamp) >= node->config.event_ttl_ms))
                {
                    expire_record(node, qi, offset, &record);
                }
                offset = baton_queue_next(queue, offset, tail);
            }
        }
    }
}

static void housekeeping(struct baton_node* node, uint32_t now_ms)
{
    if ((uint32_t) (now_ms - node->last_housekeep_ms) >= HOUSEKEEP_PERIOD_MS)
    {
        node->last_housekeep_ms = now_ms;
        if (node->config.event_ttl_ms != 0U)
        {
            expire_events(node, now_ms);
        }
    }
    queue_reclaim(node, Q_HIGH);
    queue_reclaim(node, Q_NORMAL);
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

void baton_node_default_config(struct baton_node_config* config)
{
    if (config != NULL)
    {
        (void) memset(config, 0, sizeof(*config));
        config->role = BATON_ROLE_SLAVE;
        config->baudrate = 9600U;
        config->slot_time_ms = 250U;
        config->ack_timeout_ms = 20U;
        config->max_retries = 1U;
        config->tx_gap_ms = 2U;
        config->inter_byte_timeout_ms = 5U;
        config->cycle_period_ms = 0U;
        config->event_ttl_ms = 5000U;
        config->rx_echo = true;
    }
}

int baton_node_init(struct baton_node* node, const struct baton_node_config* config,
                    uint32_t now_ms)
{
    int result = BATON_ERR_PARAM;

    if (node != NULL)
    {
        node->initialized = false;
    }
    if ((node != NULL) && (config != NULL))
    {
        if (config_valid(config))
        {
            struct baton_link_config link_config;

            (void) memset(node, 0, sizeof(*node));
            node->config = *config;

            link_config.write = config->write;
            link_config.context = config->context;
            link_config.baudrate = config->baudrate;
            link_config.tx_gap_ms = config->tx_gap_ms;
            link_config.inter_byte_timeout_ms = config->inter_byte_timeout_ms;
            link_config.rx_echo = config->rx_echo;
            (void) baton_link_init(&node->link, &link_config, now_ms);

            baton_queue_init(&node->queue[Q_HIGH], node->queue_high_memory,
                             (uint16_t) sizeof(node->queue_high_memory));
            baton_queue_init(&node->queue[Q_NORMAL], node->queue_normal_memory,
                             (uint16_t) sizeof(node->queue_normal_memory));

            if (topology_valid(node) && timing_valid(node))
            {
                for (uint8_t i = 0U; i < BATON_MAX_NODES; ++i)
                {
                    node->channel[i].tx_first = true;
                }
                node->now_ms = now_ms;
                node->last_housekeep_ms = now_ms;
                node->current = config->slave_count; /* the first process starts a cycle */
                node->cycle_start = now_ms - (uint32_t) config->cycle_period_ms;
                node->link_state = L_IDLE;
                node->initialized = true;
                result = BATON_OK;
            }
        }
    }

    return result;
}

void baton_node_process(struct baton_node* node, uint32_t now_ms)
{
    if ((node != NULL) && node->initialized)
    {
        int event;

        lock(node);
        node->now_ms = now_ms;
        unlock(node);

        do
        {
            event = baton_link_receive(&node->link, now_ms);
            if (event == BATON_LINK_FRAME)
            {
                handle_frame(node, baton_link_frame(&node->link), now_ms);
            }
            else if (event == BATON_LINK_TX_DONE)
            {
                on_tx_done(node, now_ms);
            }
            else
            {
                /* receive ring empty */
            }
        } while (event != BATON_LINK_NONE);

        if (baton_link_transmit(&node->link, now_ms) == BATON_LINK_TX_DONE)
        {
            on_tx_done(node, now_ms);
        }
        link_service(node, now_ms);
        /* start a frame requested just now, if the gap allows */
        if (baton_link_transmit(&node->link, now_ms) == BATON_LINK_TX_DONE)
        {
            on_tx_done(node, now_ms);
        }
        housekeeping(node, now_ms);
    }
}

int baton_node_send(struct baton_node* node, uint8_t dst, uint8_t msg_id, const void* data,
                    uint8_t length, uint8_t priority)
{
    static const struct msg_desc event_desc[BATON_EVENT_COUNT + 1U] = {
        BATON_EVENT_LIST(X_DESC_){0U, 0U}};
    int result = BATON_OK;
    uint8_t index = 0U;
    uint32_t mask = 0U;

    if ((node == NULL) || (!node->initialized))
    {
        result = BATON_ERR_NOT_INIT;
    }
    else if ((length > 0U) && (data == NULL))
    {
        result = BATON_ERR_PARAM;
    }
    else if (!find_desc(event_desc, (uint8_t) BATON_EVENT_COUNT, msg_id, &index))
    {
        result = BATON_ERR_MSG_ID;
    }
    else if (length > event_desc[index].max_length)
    {
        result = BATON_ERR_LEN;
    }
    else if (dst == BATON_ADDR_BROADCAST)
    {
        mask = node->others_mask;
    }
    else
    {
        const uint8_t i = node_index(node, dst);

        if ((i == NO_NODE) || (i == node->self_index))
        {
            result = BATON_ERR_ADDR;
        }
        else
        {
            mask = node_bit(i);
        }
    }

    if (result == BATON_OK)
    {
        struct baton_queue* queue = &node->queue[(priority == BATON_PRIO_HIGH) ? Q_HIGH : Q_NORMAL];
        struct baton_record record;
        uint16_t offset = 0U;
        bool reserved;

        record.pending = mask;
        record.inflight = 0U;
        record.msg_id = msg_id;
        record.length = length;
        record.dst = dst;
        record.flags = 0U; /* not visible to the consumer yet */

        lock(node);
        reserved = baton_queue_alloc(queue, baton_queue_record_size(length), &offset);
        if (reserved)
        {
            baton_queue_set_flags(queue, offset, 0U);
        }
        record.stamp = node->now_ms;
        unlock(node);

        if (!reserved)
        {
            node->stats.queue_full++;
            result = BATON_ERR_FULL;
        }
        else
        {
            baton_queue_write(queue, offset, &record);
            if (length > 0U)
            {
                (void) memcpy(baton_queue_data(queue, offset), data, length);
            }
            lock(node);
            baton_queue_set_flags(queue, offset, BATON_REC_VALID);
            unlock(node);
        }
    }

    return result;
}

int baton_node_publish(struct baton_node* node, uint8_t msg_id, const void* data, uint8_t length)
{
    int result = BATON_OK;
    uint8_t index = 0U;

    if ((node == NULL) || (!node->initialized))
    {
        result = BATON_ERR_NOT_INIT;
    }
    else if ((length > 0U) && (data == NULL))
    {
        result = BATON_ERR_PARAM;
    }
    else if (!find_desc(state_desc, (uint8_t) BATON_STATE_COUNT, msg_id, &index))
    {
        result = BATON_ERR_MSG_ID;
    }
    else if (length > state_desc[index].max_length)
    {
        result = BATON_ERR_LEN;
    }
    else
    {
        lock(node);
        if (length > 0U)
        {
            (void) memcpy(&node->state_data[state_offset(index)], data, length);
        }
        node->state_length[index] = length;
        node->state_dirty[index] = node->others_mask;
        unlock(node);
    }

    return result;
}

void baton_node_rx_isr(struct baton_node* node, uint8_t byte)
{
    if (node != NULL)
    {
        baton_link_rx_isr(&node->link, byte);
    }
}

void baton_node_tx_done_isr(struct baton_node* node)
{
    if (node != NULL)
    {
        baton_link_tx_done_isr(&node->link);
    }
}

void baton_node_get_stats(const struct baton_node* node, struct baton_stats* stats)
{
    if ((node != NULL) && (stats != NULL))
    {
        const struct baton_link_counters* link = &node->link.counters;

        *stats = node->stats;
        stats->tx_frames = link->tx_frames;
        stats->rx_frames = link->rx_frames;
        stats->crc_errors = link->crc_errors;
        stats->rx_timeouts = link->rx_timeouts;
        stats->rx_overflows = node->link.rx_overflows;
        stats->collisions = link->collisions;
        stats->tx_timeouts = link->tx_timeouts;
        stats->queue_high_used = baton_queue_used(&node->queue[Q_HIGH]);
        stats->queue_normal_used = baton_queue_used(&node->queue[Q_NORMAL]);
    }
}
```


