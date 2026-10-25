/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Bare-metal Baton slave: UART RX interrupt, main loop, outputs
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
 * @brief Bare-metal Baton slave 0x11 on a bus with master 0x01 and slave 0x12.
 * @details The UART RX interrupt passes every byte to baton_node_rx_isr(),
 *          which only stores it in the node's ring. The main loop runs
 *          baton_node_process() every millisecond, publishes the uptime as the
 *          NODE_STATUS state once per second, applies CMD_SET_OUTPUT events and
 *          confirms each one with an OUTPUT_REPORT event to its sender.
 *          Everything runs in the main loop, so no critical section is needed.
 */

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <stdbool.h>
#include <string.h>

#include "baton.h"
#include "board.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

#define NODE_ADDRESS 0x11U
#define MASTER_ADDRESS 0x01U
#define BAUD_RATE 9600U
#define OUTPUT_COUNT 4U
#define STATUS_PERIOD_MS 1000U

/* ==============================================================================
 * PRIVATE VARIABLES (static)
 * =============================================================================*/

static const uint8_t slave_addresses[] = {0x11U, 0x12U};

static struct baton_node node;
static uint8_t outputs[OUTPUT_COUNT];

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

static int uart_write(void* context, const uint8_t* data, size_t length)
{
    (void) context;
    return board_uart_write(data, length);
}

static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    (void) context;
    if ((msg_id == (uint8_t) BATON_MSG_CMD_SET_OUTPUT) && (length == 2U)
        && (data[0] < OUTPUT_COUNT))
    {
        outputs[data[0]] = data[1];
        /* The library delivered the event exactly once; the report tells the sender
           that the application applied it. */
        (void) baton_node_send(&node, src, (uint8_t) BATON_MSG_OUTPUT_REPORT, data, 2U,
                               BATON_PRIO_HIGH);
    }
}

static void publish_status(uint32_t now_ms)
{
    uint8_t status[4];

    baton_put_u32le(status, now_ms / 1000U);
    (void) baton_node_publish(&node, (uint8_t) BATON_MSG_NODE_STATUS, status, sizeof(status));
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

void app_uart_rx_isr(uint8_t byte)
{
    baton_node_rx_isr(&node, byte);
}

int main(void)
{
    struct baton_node_config config;
    uint32_t next_status_ms;

    baton_node_default_config(&config);
    config.role = BATON_ROLE_SLAVE;
    config.own_address = NODE_ADDRESS;
    config.master_address = MASTER_ADDRESS;
    config.slave_addresses = slave_addresses;
    config.slave_count = (uint8_t) sizeof(slave_addresses);
    config.baudrate = BAUD_RATE;
    config.rx_echo = true; /* single-wire UART: the receiver hears the own bytes */
    config.write = uart_write;
    config.on_message = on_message;

    board_init(BAUD_RATE);
    if (baton_node_init(&node, &config, board_millis()) != BATON_OK)
    {
        return 1;
    }

    next_status_ms = board_millis();
    for (;;)
    {
        const uint32_t now = board_millis();

        baton_node_process(&node, now);
        if ((int32_t) (now - next_status_ms) >= 0)
        {
            publish_status(now);
            next_status_ms += STATUS_PERIOD_MS;
        }
        board_idle();
    }
}
