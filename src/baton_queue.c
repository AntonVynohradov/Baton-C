/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Event queue: ring buffer of variable-size records
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

#include "baton_defs.h"
#include "baton_queue.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

/** Byte offsets of the header fields inside a record. */
#define REC_PENDING 0U
#define REC_INFLIGHT 4U
#define REC_STAMP 8U
#define REC_MSG_ID 12U
#define REC_LENGTH 13U
#define REC_DST 14U
#define REC_FLAGS 15U

/* ==============================================================================
 * PRIVATE FUNCTION DECLARATIONS (static)
 * =============================================================================*/

static uint16_t align4(uint16_t value);
static uint16_t normalize(const struct baton_queue* queue, uint16_t offset, uint16_t tail);

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

static uint16_t align4(uint16_t value)
{
    return (uint16_t) ((uint16_t) (value + 3U) & (uint16_t) 0xFFFCU);
}

/** Continue at offset 0 when no header fits at @p offset or a SKIP marker is there. */
static uint16_t normalize(const struct baton_queue* queue, uint16_t offset, uint16_t tail)
{
    uint16_t result = offset;

    if (offset != tail)
    {
        if (((uint32_t) offset + BATON_REC_HEADER_SIZE) > (uint32_t) queue->size)
        {
            result = 0U;
        }
        else if ((queue->memory[offset + REC_FLAGS] & BATON_REC_SKIP) != 0U)
        {
            result = 0U;
        }
        else
        {
            /* a record starts here */
        }
    }

    return result;
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

void baton_queue_init(struct baton_queue* queue, uint8_t* memory, uint16_t size)
{
    if ((queue != NULL) && (memory != NULL))
    {
        queue->memory = memory;
        queue->size = (uint16_t) (size & (uint16_t) 0xFFFCU);
        queue->head = 0U;
        queue->tail = 0U;
    }
}

uint16_t baton_queue_record_size(uint8_t length)
{
    return (uint16_t) (BATON_REC_HEADER_SIZE + align4((uint16_t) length));
}

bool baton_queue_alloc(struct baton_queue* queue, uint16_t need, uint16_t* offset)
{
    bool reserved = false;

    if ((queue != NULL) && (offset != NULL) && (need >= BATON_REC_HEADER_SIZE))
    {
        uint16_t head;
        uint16_t tail;

        if (queue->head == queue->tail)
        {
            queue->head = 0U;
            queue->tail = 0U;
        }
        head = queue->head;
        tail = queue->tail;

        if (tail >= head)
        {
            if (((uint32_t) queue->size - (uint32_t) tail) >= (uint32_t) need)
            {
                *offset = tail;
                queue->tail = (uint16_t) (tail + need);
                reserved = true;
            }
            else if (head > need) /* wrap; strict so that tail never catches head */
            {
                if (((uint32_t) queue->size - (uint32_t) tail) >= BATON_REC_HEADER_SIZE)
                {
                    (void) memset(&queue->memory[tail], 0, BATON_REC_HEADER_SIZE);
                    queue->memory[tail + REC_FLAGS] = BATON_REC_SKIP;
                }
                *offset = 0U;
                queue->tail = need;
                reserved = true;
            }
            else
            {
                /* no room at the end and not enough before the head */
            }
        }
        else if ((uint16_t) (head - tail) > need)
        {
            *offset = tail;
            queue->tail = (uint16_t) (tail + need);
            reserved = true;
        }
        else
        {
            /* no room between tail and head */
        }
    }

    return reserved;
}

uint16_t baton_queue_first(const struct baton_queue* queue, uint16_t head, uint16_t tail)
{
    uint16_t result = tail;

    if (queue != NULL)
    {
        result = normalize(queue, head, tail);
    }

    return result;
}

uint16_t baton_queue_next(const struct baton_queue* queue, uint16_t offset, uint16_t tail)
{
    uint16_t result = tail;

    if (queue != NULL)
    {
        const uint8_t length = queue->memory[offset + REC_LENGTH];

        result = normalize(queue, (uint16_t) (offset + baton_queue_record_size(length)), tail);
    }

    return result;
}

uint16_t baton_queue_used(const struct baton_queue* queue)
{
    uint16_t used = 0U;

    if (queue != NULL)
    {
        const uint16_t head = queue->head;
        const uint16_t tail = queue->tail;

        if (tail >= head)
        {
            used = (uint16_t) (tail - head);
        }
        else
        {
            used = (uint16_t) ((uint16_t) (queue->size - head) + tail);
        }
    }

    return used;
}

void baton_queue_read(const struct baton_queue* queue, uint16_t offset, struct baton_record* record)
{
    if ((queue != NULL) && (record != NULL))
    {
        const uint8_t* header = &queue->memory[offset];

        record->pending = baton_get_u32le(&header[REC_PENDING]);
        record->inflight = baton_get_u32le(&header[REC_INFLIGHT]);
        record->stamp = baton_get_u32le(&header[REC_STAMP]);
        record->msg_id = header[REC_MSG_ID];
        record->length = header[REC_LENGTH];
        record->dst = header[REC_DST];
        record->flags = header[REC_FLAGS];
    }
}

void baton_queue_write(struct baton_queue* queue, uint16_t offset,
                       const struct baton_record* record)
{
    if ((queue != NULL) && (record != NULL))
    {
        uint8_t* header = &queue->memory[offset];

        baton_put_u32le(&header[REC_PENDING], record->pending);
        baton_put_u32le(&header[REC_INFLIGHT], record->inflight);
        baton_put_u32le(&header[REC_STAMP], record->stamp);
        header[REC_MSG_ID] = record->msg_id;
        header[REC_LENGTH] = record->length;
        header[REC_DST] = record->dst;
        header[REC_FLAGS] = record->flags;
    }
}

void baton_queue_set_masks(struct baton_queue* queue, uint16_t offset, uint32_t pending,
                           uint32_t inflight)
{
    if (queue != NULL)
    {
        baton_put_u32le(&queue->memory[offset + REC_PENDING], pending);
        baton_put_u32le(&queue->memory[offset + REC_INFLIGHT], inflight);
    }
}

void baton_queue_set_flags(struct baton_queue* queue, uint16_t offset, uint8_t flags)
{
    if (queue != NULL)
    {
        queue->memory[offset + REC_FLAGS] = flags;
    }
}

uint8_t* baton_queue_data(struct baton_queue* queue, uint16_t offset)
{
    uint8_t* data = NULL;

    if (queue != NULL)
    {
        data = &queue->memory[offset + BATON_REC_HEADER_SIZE];
    }

    return data;
}
