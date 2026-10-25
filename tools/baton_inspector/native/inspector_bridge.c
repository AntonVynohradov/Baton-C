/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Native bridge of Baton Inspector: a flat C API over the Baton-C
 *              library (nodes, parser, message table) for Python ctypes
 *
 * Author:      Anton Vynohradov
 * Email:       avynohradov@systemfromscratch.com
 *
 * License:     Apache License 2.0
 *
 * Copyright (c) 2026 Anton Vynohradov
 *
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/

/*
 * This file is tool code, not library code: it allocates the node structures on
 * the heap so that Python can create as many nodes as a simulation needs. Every
 * protocol decision is made by the library linked into the bridge, built with
 * the baton_user_config.h of the system under test.
 */

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <stdlib.h>
#include <string.h>

#include "baton.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

#if defined(_WIN32)
#define BI_API __declspec(dllexport)
#else
#define BI_API __attribute__((visibility("default")))
#endif

/** Version of this bridge interface; Python checks it after loading. */
#define BI_INTERFACE_VERSION 1

/* ==============================================================================
 * PRIVATE TYPE DEFINITIONS
 * =============================================================================*/

typedef int (*bi_write_cb)(void* user, const uint8_t* data, uint32_t length);
typedef void (*bi_message_cb)(void* user, uint8_t src, uint8_t msg_id, const uint8_t* data,
                              uint8_t length);
typedef void (*bi_drop_cb)(void* user, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes);
typedef void (*bi_poll_cb)(void* user, uint8_t slave, int responded, uint32_t nack_nodes);

struct bi_node
{
    struct baton_node node;
    uint8_t slaves[BATON_MAX_NODES];
    bi_write_cb write;
    bi_message_cb on_message;
    bi_drop_cb on_drop;
    bi_poll_cb on_poll_done;
    void* user;
};

struct bi_msg
{
    const char* name;
    uint8_t id;
    uint8_t max_length;
    uint8_t is_state;
};

/* ==============================================================================
 * PRIVATE VARIABLES (static)
 * =============================================================================*/

#define BI_X_STATE_(name, id, max_length) {#name, (uint8_t) (id), (uint8_t) (max_length), 1U},
#define BI_X_EVENT_(name, id, max_length) {#name, (uint8_t) (id), (uint8_t) (max_length), 0U},

static const struct bi_msg messages[] = {BATON_STATE_LIST(BI_X_STATE_)
                                             BATON_EVENT_LIST(BI_X_EVENT_){NULL, 0U, 0U, 0U}};

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

static int node_write(void* context, const uint8_t* data, size_t length)
{
    struct bi_node* n = (struct bi_node*) context;
    return n->write(n->user, data, (uint32_t) length);
}

static void node_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                         uint8_t length)
{
    struct bi_node* n = (struct bi_node*) context;
    if (n->on_message != NULL)
    {
        n->on_message(n->user, src, msg_id, data, length);
    }
}

static void node_drop(void* context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes)
{
    struct bi_node* n = (struct bi_node*) context;
    if (n->on_drop != NULL)
    {
        n->on_drop(n->user, msg_id, dst, missed_nodes);
    }
}

static void node_poll_done(void* context, uint8_t slave, bool responded, uint32_t nack_nodes)
{
    struct bi_node* n = (struct bi_node*) context;
    if (n->on_poll_done != NULL)
    {
        n->on_poll_done(n->user, slave, responded ? 1 : 0, nack_nodes);
    }
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

BI_API int bi_interface_version(void)
{
    return BI_INTERFACE_VERSION;
}

BI_API const char* bi_library_version(void)
{
    return BATON_VERSION;
}

/** Build-time sizes: max_nodes, max_payload, max_msgs_per_frame, queue_high, queue_normal,
 *  rx_buf, max_msg_len, frame_max. */
BI_API void bi_config(uint32_t* out)
{
    out[0] = BATON_MAX_NODES;
    out[1] = BATON_MAX_PAYLOAD;
    out[2] = BATON_MAX_MSGS_PER_FRAME;
    out[3] = BATON_QUEUE_HIGH_SIZE;
    out[4] = BATON_QUEUE_NORMAL_SIZE;
    out[5] = BATON_RX_BUF_SIZE;
    out[6] = BATON_MAX_MSG_LEN;
    out[7] = BATON_FRAME_MAX;
}

BI_API int bi_msg_count(void)
{
    return (int) (BATON_STATE_COUNT + BATON_EVENT_COUNT);
}

BI_API const char* bi_msg_info(int index, uint8_t* id, uint8_t* max_length, uint8_t* is_state)
{
    const char* name = NULL;
    if ((index >= 0) && (index < bi_msg_count()))
    {
        *id = messages[index].id;
        *max_length = messages[index].max_length;
        *is_state = messages[index].is_state;
        name = messages[index].name;
    }
    return name;
}

/* ---- nodes ---- */

BI_API void* bi_node_new(void)
{
    return calloc(1U, sizeof(struct bi_node));
}

BI_API void bi_node_free(void* handle)
{
    free(handle);
}

/** params: role, own, master, slave_count, max_retries, tx_gap_ms, inter_byte_timeout_ms,
 *  rx_echo, baudrate, slot_time_ms, ack_timeout_ms, cycle_period_ms, event_ttl_ms. */
BI_API int bi_node_init(void* handle, const uint32_t* params, const uint8_t* slaves,
                        bi_write_cb write, bi_message_cb on_message, bi_drop_cb on_drop,
                        bi_poll_cb on_poll_done, void* user, uint32_t now_ms)
{
    struct bi_node* n = (struct bi_node*) handle;
    struct baton_node_config config;
    int result = BATON_ERR_PARAM;

    if ((n != NULL) && (params != NULL) && (slaves != NULL) && (write != NULL)
        && (params[3] < BATON_MAX_NODES))
    {
        (void) memcpy(n->slaves, slaves, params[3]);
        n->write = write;
        n->on_message = on_message;
        n->on_drop = on_drop;
        n->on_poll_done = on_poll_done;
        n->user = user;

        baton_node_default_config(&config);
        config.role = (uint8_t) params[0];
        config.own_address = (uint8_t) params[1];
        config.master_address = (uint8_t) params[2];
        config.slave_addresses = n->slaves;
        config.slave_count = (uint8_t) params[3];
        config.max_retries = (uint8_t) params[4];
        config.tx_gap_ms = (uint8_t) params[5];
        config.inter_byte_timeout_ms = (uint8_t) params[6];
        config.rx_echo = (params[7] != 0U);
        config.baudrate = params[8];
        config.slot_time_ms = (uint16_t) params[9];
        config.ack_timeout_ms = (uint16_t) params[10];
        config.cycle_period_ms = (uint16_t) params[11];
        config.event_ttl_ms = params[12];
        config.write = node_write;
        config.on_message = node_message;
        config.on_drop = node_drop;
        config.on_poll_done = node_poll_done;
        config.context = n;
        result = baton_node_init(&n->node, &config, now_ms);
    }
    return result;
}

BI_API void bi_node_process(void* handle, uint32_t now_ms)
{
    baton_node_process(&((struct bi_node*) handle)->node, now_ms);
}

BI_API void bi_node_rx(void* handle, const uint8_t* data, uint32_t length)
{
    struct bi_node* n = (struct bi_node*) handle;
    for (uint32_t i = 0U; i < length; ++i)
    {
        baton_node_rx_isr(&n->node, data[i]);
    }
}

BI_API void bi_node_tx_done(void* handle)
{
    baton_node_tx_done_isr(&((struct bi_node*) handle)->node);
}

BI_API int bi_node_send(void* handle, uint8_t dst, uint8_t msg_id, const uint8_t* data,
                        uint8_t length, uint8_t priority)
{
    return baton_node_send(&((struct bi_node*) handle)->node, dst, msg_id, data, length, priority);
}

BI_API int bi_node_publish(void* handle, uint8_t msg_id, const uint8_t* data, uint8_t length)
{
    return baton_node_publish(&((struct bi_node*) handle)->node, msg_id, data, length);
}

/** out: the 14 uint32 counters of struct baton_stats followed by the two queue fills. */
BI_API void bi_node_stats(void* handle, uint32_t* out)
{
    struct baton_stats s;
    baton_node_get_stats(&((struct bi_node*) handle)->node, &s);
    out[0] = s.tx_frames;
    out[1] = s.rx_frames;
    out[2] = s.crc_errors;
    out[3] = s.rx_timeouts;
    out[4] = s.rx_overflows;
    out[5] = s.collisions;
    out[6] = s.tx_timeouts;
    out[7] = s.ack_timeouts;
    out[8] = s.retries;
    out[9] = s.resp_timeouts;
    out[10] = s.duplicates;
    out[11] = s.bad_messages;
    out[12] = s.events_dropped;
    out[13] = s.queue_full;
    out[14] = s.queue_high_used;
    out[15] = s.queue_normal_used;
}

/* ---- parser ---- */

BI_API void* bi_parser_new(void)
{
    struct baton_parser* p = calloc(1U, sizeof(struct baton_parser));
    baton_parser_init(p);
    return p;
}

BI_API void bi_parser_free(void* handle)
{
    free(handle);
}

BI_API void bi_parser_reset(void* handle)
{
    baton_parser_init((struct baton_parser*) handle);
}

BI_API int bi_parser_idle(void* handle)
{
    return (((struct baton_parser*) handle)->state == BATON_PARSER_IDLE) ? 1 : 0;
}

BI_API int bi_parser_feed(void* handle, uint8_t byte)
{
    return baton_parser_feed((struct baton_parser*) handle, byte);
}

/** header: dst, src, ctrl, length; payload: at least BATON_MAX_PAYLOAD bytes. */
BI_API void bi_parser_frame(void* handle, uint8_t* header, uint8_t* payload)
{
    const struct baton_frame* f = &((struct baton_parser*) handle)->frame;
    header[0] = f->dst;
    header[1] = f->src;
    header[2] = f->ctrl;
    header[3] = f->length;
    (void) memcpy(payload, f->payload, f->length);
}

BI_API int bi_encode(uint8_t dst, uint8_t src, uint8_t ctrl, const uint8_t* payload,
                     uint32_t length, uint8_t* out, uint32_t capacity)
{
    size_t written = 0U;
    const int result = baton_encode(dst, src, ctrl, payload, length, out, capacity, &written);
    return (result == 0) ? (int) written : -1;
}
