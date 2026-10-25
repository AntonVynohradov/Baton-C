/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Zephyr Baton slave: UART interrupts, protocol thread
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
 * @file main.c
 * @brief Zephyr Baton slave 0x12: UART interrupts, one protocol thread.
 * @details The RX interrupt passes every byte to baton_node_rx_isr() (the node
 *          has its own interrupt-safe ring, so no k_msgq is needed). The TX
 *          interrupt sends the frame from the buffer the library keeps valid
 *          until the end of the transmission. The protocol thread calls
 *          baton_node_process() every millisecond; other threads may call
 *          baton_node_send() and baton_node_publish() because the critical
 *          section hooks lock interrupts. The UART is the devicetree chosen
 *          node baton,uart (see app.overlay.example). Disable console and
 *          logging on that UART: text corrupts the framing.
 */

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include <errno.h>

#include "baton.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

#define NODE_ADDRESS 0x12U
#define MASTER_ADDRESS 0x01U
#define BAUD_RATE 9600U

#define BATON_STACK_SIZE 1536
#define BATON_PRIORITY K_PRIO_PREEMPT(5)

/* ==============================================================================
 * PRIVATE VARIABLES (static)
 * =============================================================================*/

static const struct device* const baton_uart = DEVICE_DT_GET(DT_CHOSEN(baton_uart));
static const uint8_t slave_addresses[] = {0x11U, 0x12U};

static struct baton_node node;
static unsigned int irq_key;

static const uint8_t* tx_data;
static size_t tx_length;
static size_t tx_position;

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

static void uart_isr(const struct device* dev, void* user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) > 0)
    {
        if (uart_irq_rx_ready(dev) > 0)
        {
            uint8_t bytes[16];
            const int count = uart_fifo_read(dev, bytes, sizeof(bytes));

            for (int i = 0; i < count; ++i)
            {
                baton_node_rx_isr(&node, bytes[i]);
            }
        }
        else if (uart_irq_tx_ready(dev) > 0)
        {
            if (tx_position < tx_length)
            {
                const int sent =
                    uart_fifo_fill(dev, &tx_data[tx_position], (int) (tx_length - tx_position));
                tx_position += (size_t) sent;
            }
            else
            {
                /* The echo of the last byte ends the transmission (rx_echo). */
                uart_irq_tx_disable(dev);
            }
        }
        else
        {
            break;
        }
    }
}

static int uart_write(void* context, const uint8_t* data, size_t length)
{
    ARG_UNUSED(context);

    tx_data = data;
    tx_length = length;
    tx_position = 0U;
    uart_irq_tx_enable(baton_uart);
    return 0;
}

static void enter_critical(void* context)
{
    ARG_UNUSED(context);
    irq_key = irq_lock();
}

static void exit_critical(void* context)
{
    ARG_UNUSED(context);
    irq_unlock(irq_key);
}

static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    ARG_UNUSED(context);
    if ((msg_id == BATON_MSG_CMD_SET_OUTPUT) && (length == 2U))
    {
        /* Drive a GPIO here, then confirm to the sender. */
        (void) baton_node_send(&node, src, BATON_MSG_OUTPUT_REPORT, data, 2U, BATON_PRIO_HIGH);
    }
}

static void baton_thread(void* p1, void* p2, void* p3)
{
    uint32_t next_status = k_uptime_get_32();

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (;;)
    {
        const uint32_t now = k_uptime_get_32();

        baton_node_process(&node, now);
        if ((int32_t) (now - next_status) >= 0)
        {
            uint8_t status[4];

            baton_put_u32le(status, now / 1000U);
            (void) baton_node_publish(&node, BATON_MSG_NODE_STATUS, status, sizeof(status));
            next_status += 1000U;
        }
        k_sleep(K_MSEC(1));
    }
}

K_THREAD_DEFINE(baton_tid, BATON_STACK_SIZE, baton_thread, NULL, NULL, NULL, BATON_PRIORITY, 0,
                100);

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

int main(void)
{
    struct baton_node_config config;

    if (!device_is_ready(baton_uart))
    {
        return -ENODEV;
    }

    baton_node_default_config(&config);
    config.role = BATON_ROLE_SLAVE;
    config.own_address = NODE_ADDRESS;
    config.master_address = MASTER_ADDRESS;
    config.slave_addresses = slave_addresses;
    config.slave_count = (uint8_t) sizeof(slave_addresses);
    config.baudrate = BAUD_RATE;
    config.rx_echo = true;
    config.write = uart_write;
    config.enter_critical = enter_critical;
    config.exit_critical = exit_critical;
    config.on_message = on_message;
    if (baton_node_init(&node, &config, k_uptime_get_32()) != BATON_OK)
    {
        return -EINVAL;
    }

    if (uart_irq_callback_user_data_set(baton_uart, uart_isr, NULL) != 0)
    {
        return -EIO;
    }
    uart_irq_rx_enable(baton_uart);
    return 0;
}
