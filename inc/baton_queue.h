/**
 * @file baton_queue.h
 * @brief Baton event queue: ring buffer of variable-size records.
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
 * @defgroup baton_queue Event queue
 * @ingroup baton
 * @brief Byte ring of variable-size event records, used by @ref baton_node.
 *
 * @details Layout rules:
 * @li A record is a 16-byte header (@ref baton_record, serialized byte by
 *     byte) followed by the data, padded to a multiple of 4 bytes.
 * @li A record is never split at the end of the buffer. If it does not fit at
 *     the end, a SKIP marker is written there (when a header fits) and the
 *     record goes to offset 0. A reader that reaches an offset without room for
 *     a header, or a SKIP marker, continues at 0.
 * @li head == tail means empty; the producer resets both to 0 when the queue
 *     is empty, so the wrapped state never starts from a stale head.
 * @li Records are released only from the head.
 *
 * The producer (baton_node_send()) may run in an interrupt; it calls
 * @ref baton_queue_alloc with the critical section held and publishes a
 * record by setting @ref BATON_REC_VALID last.
 * @{
 */

#ifndef BATON_QUEUE_H
#define BATON_QUEUE_H

/** @name Includes */
/** @{ */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @} */

/** @name Record header */
/** @{ */

/** Serialized size of a record header in bytes. */
#define BATON_REC_HEADER_SIZE 16U
/** Record is complete and visible to the consumer. */
#define BATON_REC_VALID 0x01U
/** Marker at the end of the buffer: continue at offset 0. */
#define BATON_REC_SKIP 0x02U

/** @brief Header of one event record. */
struct baton_record
{
    uint32_t pending;  /**< Node mask: nodes that have not acknowledged the event. */
    uint32_t inflight; /**< Node mask: nodes with an unacknowledged frame containing it. */
    uint32_t stamp;    /**< Time of baton_node_send() in ms. */
    uint8_t msg_id;    /**< Message identifier. */
    uint8_t length;    /**< Number of data bytes. */
    uint8_t dst;       /**< Destination address given to baton_node_send(). */
    uint8_t flags;     /**< BATON_REC_VALID, BATON_REC_SKIP. */
};

/** @brief Queue state. */
struct baton_queue
{
    uint8_t* memory; /**< Record storage, provided by the owner. */
    uint16_t size;   /**< Usable bytes, multiple of 4. */
    uint16_t head;   /**< Oldest record (consumer). */
    uint16_t tail;   /**< Next free byte (producer). */
};

/** @} */

/** @name Public function declarations */
/** @{ */

/**
 * @brief Initialize an empty queue over @p memory.
 * @param queue Queue state.
 * @param memory Storage of @p size bytes.
 * @param size Storage size; rounded down to a multiple of 4.
 */
void baton_queue_init(struct baton_queue* queue, uint8_t* memory, uint16_t size);

/** @brief Bytes a record with @p length data bytes occupies. */
uint16_t baton_queue_record_size(uint8_t length);

/**
 * @brief Reserve @p need bytes at the tail. Call with the critical section held.
 * @param queue Queue state.
 * @param need Record size from baton_queue_record_size().
 * @param offset Receives the record offset.
 * @return true when the space was reserved.
 */
bool baton_queue_alloc(struct baton_queue* queue, uint16_t need, uint16_t* offset);

/**
 * @brief Offset of the first record for a head/tail snapshot.
 * @return Offset of the first record, or @p tail when the queue is empty.
 */
uint16_t baton_queue_first(const struct baton_queue* queue, uint16_t head, uint16_t tail);

/**
 * @brief Offset of the record after the one at @p offset.
 * @return Offset of the next record, or @p tail after the last one.
 */
uint16_t baton_queue_next(const struct baton_queue* queue, uint16_t offset, uint16_t tail);

/** @brief Bytes currently in use. */
uint16_t baton_queue_used(const struct baton_queue* queue);

/** @brief Read the header of the record at @p offset. */
void baton_queue_read(const struct baton_queue* queue, uint16_t offset,
                      struct baton_record* record);

/** @brief Write the complete header of the record at @p offset. */
void baton_queue_write(struct baton_queue* queue, uint16_t offset,
                       const struct baton_record* record);

/** @brief Update only the node masks of the record at @p offset. */
void baton_queue_set_masks(struct baton_queue* queue, uint16_t offset, uint32_t pending,
                           uint32_t inflight);

/** @brief Update only the flags of the record at @p offset. */
void baton_queue_set_flags(struct baton_queue* queue, uint16_t offset, uint8_t flags);

/** @brief Data bytes of the record at @p offset. */
uint8_t* baton_queue_data(struct baton_queue* queue, uint16_t offset);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BATON_QUEUE_H */

/** @} */
