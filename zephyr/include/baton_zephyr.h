/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Zephyr module API: one Baton node per devicetree node
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

/**
 * @file baton_zephyr.h
 * @brief Zephyr integration of Baton-C.
 * @details Every enabled devicetree node with compatible
 *          "systemfromscratch,baton" becomes a device that owns one
 *          @ref baton_node, the UART interrupts of its parent UART and a
 *          protocol thread. The application gets the device with
 *          DEVICE_DT_GET(), registers its callbacks with
 *          baton_zephyr_start() and exchanges messages with
 *          baton_zephyr_send() and baton_zephyr_publish(), from any thread.
 *          Callbacks run in the protocol thread of the node.
 */

#ifndef BATON_ZEPHYR_H
#define BATON_ZEPHYR_H

#include <stdint.h>

#include <zephyr/device.h>

#include "baton.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Application callbacks, called from the protocol thread of the node. */
struct baton_zephyr_callbacks
{
    baton_message_fn on_message;     /**< A message was received (optional). */
    baton_drop_fn on_drop;           /**< An event expired undelivered (optional). */
    baton_poll_done_fn on_poll_done; /**< Master: a poll slot finished (optional). */
    void* context;                   /**< Passed to every callback. */
};

/**
 * @brief Register the callbacks and connect the node to the bus.
 * @details Enables the UART receive interrupt and starts the protocol thread.
 *          Messages queued before the call are sent once the node runs.
 * @param dev Baton device.
 * @param callbacks Callbacks; copied. NULL means no callbacks.
 * @retval 0 Started.
 * @retval -ENODEV The device failed to initialize.
 * @retval -EALREADY Already started.
 */
int baton_zephyr_start(const struct device* dev, const struct baton_zephyr_callbacks* callbacks);

/**
 * @brief Queue an event for reliable delivery (baton_node_send()).
 * @retval 0 Queued.
 * @retval -EINVAL Invalid argument or priority.
 * @retval -ENOMSG Unknown message id or not an EVENT.
 * @retval -EMSGSIZE Data longer than the maximum length of the message.
 * @retval -EADDRNOTAVAIL Unknown destination or own address.
 * @retval -ENOBUFS Event queue full; try again later.
 * @retval -ENODEV The device failed to initialize.
 */
int baton_zephyr_send(const struct device* dev, uint8_t dst, uint8_t msg_id, const void* data,
                      uint8_t length, uint8_t priority);

/**
 * @brief Update a STATE value (baton_node_publish()); never fails for lack of space.
 * @return 0 or a negative errno, as baton_zephyr_send().
 */
int baton_zephyr_publish(const struct device* dev, uint8_t msg_id, const void* data,
                         uint8_t length);

/**
 * @brief Copy the diagnostic counters of the node.
 * @retval 0 Copied.
 * @retval -EINVAL @p stats is NULL.
 * @retval -ENODEV The device failed to initialize.
 */
int baton_zephyr_get_stats(const struct device* dev, struct baton_stats* stats);

#ifdef __cplusplus
}
#endif

#endif /* BATON_ZEPHYR_H */
