/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Host master: polls the example bus through a PC serial port
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
 * @brief Baton master on a PC serial port (USB-UART or USB-RS-485 adapter).
 * @details Usage:
 *          baton_master <port> [--baud N] [--echo] [--seconds N] [--set ADDR OUTPUT VALUE]...
 *
 *          The master polls the slaves 0x11 and 0x12 of the example bus, prints
 *          every message it receives and a summary of the poll slots once per
 *          second. Each --set sends a CMD_SET_OUTPUT event. Use --echo with a
 *          single-wire adapter that receives its own bytes (the library filters
 *          them); without it the end of a write is the end of the transmission.
 */

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "baton.h"
#include "serial_port.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

#define MASTER_ADDRESS 0x01U
#define MAX_COMMANDS 8

/* ==============================================================================
 * PRIVATE TYPE DEFINITIONS
 * =============================================================================*/

struct command
{
    uint8_t address;
    uint8_t data[2];
};

/* ==============================================================================
 * PRIVATE VARIABLES (static)
 * =============================================================================*/

static const uint8_t slave_addresses[] = {0x11U, 0x12U};

static struct baton_node node;
static bool use_echo;
static unsigned polls;
static unsigned answered;

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

static int port_write(void* context, const uint8_t* data, size_t length)
{
    const int result = serial_write(data, length);

    (void) context;
    if ((result == 0) && !use_echo)
    {
        /* serial_write() returns when the bytes are handed over: the end of the frame */
        baton_node_tx_done_isr(&node);
    }
    return result;
}

static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    (void) context;
    printf("%8u  0x%02X  ", (unsigned) host_millis(), (unsigned) src);
    if ((msg_id == BATON_MSG_NODE_STATUS) && (length == 4U))
    {
        printf("NODE_STATUS    uptime %u s\n", (unsigned) baton_get_u32le(data));
    }
    else if ((msg_id == BATON_MSG_OUTPUT_REPORT) && (length == 2U))
    {
        printf("OUTPUT_REPORT  output %u = 0x%02X\n", (unsigned) data[0], (unsigned) data[1]);
    }
    else
    {
        printf("message 0x%02X, %u bytes\n", (unsigned) msg_id, (unsigned) length);
    }
}

static void on_drop(void* context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes)
{
    (void) context;
    printf("dropped event 0x%02X to 0x%02X, not acknowledged by node mask 0x%02X\n",
           (unsigned) msg_id, (unsigned) dst, (unsigned) missed_nodes);
}

static void on_poll_done(void* context, uint8_t slave, bool responded, uint32_t nack_nodes)
{
    (void) context;
    (void) slave;
    (void) nack_nodes;
    polls++;
    if (responded)
    {
        answered++;
    }
}

static void usage(void)
{
    printf("usage: baton_master <port> [--baud N] [--echo] [--seconds N]"
           " [--set ADDR OUTPUT VALUE]...\n"
           "example: baton_master /dev/ttyUSB0 --set 0x11 0 1\n");
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

int main(int argc, char** argv)
{
    struct baton_node_config config;
    struct command commands[MAX_COMMANDS];
    int command_count = 0;
    uint32_t baud = 9600U;
    uint32_t seconds = 10U;
    uint32_t start;
    uint32_t next_report;

    if (argc < 2)
    {
        usage();
        return 2;
    }
    for (int i = 2; i < argc; ++i)
    {
        if ((strcmp(argv[i], "--baud") == 0) && ((i + 1) < argc))
        {
            baud = (uint32_t) strtoul(argv[++i], NULL, 0);
        }
        else if ((strcmp(argv[i], "--seconds") == 0) && ((i + 1) < argc))
        {
            seconds = (uint32_t) strtoul(argv[++i], NULL, 0);
        }
        else if (strcmp(argv[i], "--echo") == 0)
        {
            use_echo = true;
        }
        else if ((strcmp(argv[i], "--set") == 0) && ((i + 3) < argc)
                 && (command_count < MAX_COMMANDS))
        {
            commands[command_count].address = (uint8_t) strtoul(argv[i + 1], NULL, 0);
            commands[command_count].data[0] = (uint8_t) strtoul(argv[i + 2], NULL, 0);
            commands[command_count].data[1] = (uint8_t) strtoul(argv[i + 3], NULL, 0);
            command_count++;
            i += 3;
        }
        else
        {
            usage();
            return 2;
        }
    }

    if (serial_open(argv[1], baud) != 0)
    {
        printf("cannot open %s at %u bit/s\n", argv[1], (unsigned) baud);
        return 1;
    }

    baton_node_default_config(&config);
    config.role = BATON_ROLE_MASTER;
    config.own_address = MASTER_ADDRESS;
    config.master_address = MASTER_ADDRESS;
    config.slave_addresses = slave_addresses;
    config.slave_count = (uint8_t) sizeof(slave_addresses);
    config.baudrate = baud;
    config.rx_echo = use_echo;
    config.write = port_write;
    config.on_message = on_message;
    config.on_drop = on_drop;
    config.on_poll_done = on_poll_done;
    if (baton_node_init(&node, &config, host_millis()) != BATON_OK)
    {
        printf("invalid configuration\n");
        serial_close();
        return 1;
    }

    for (int i = 0; i < command_count; ++i)
    {
        const int result = baton_node_send(&node, commands[i].address, BATON_MSG_CMD_SET_OUTPUT,
                                           commands[i].data, 2U, BATON_PRIO_NORMAL);

        printf("CMD_SET_OUTPUT to 0x%02X: output %u = 0x%02X (%s)\n",
               (unsigned) commands[i].address, (unsigned) commands[i].data[0],
               (unsigned) commands[i].data[1], (result == BATON_OK) ? "queued" : "rejected");
    }

    start = host_millis();
    next_report = start + 1000U;
    while ((uint32_t) (host_millis() - start) < (seconds * 1000U))
    {
        uint8_t buffer[64];
        const int count = serial_read(buffer, sizeof(buffer), 1U);

        for (int i = 0; i < count; ++i)
        {
            baton_node_rx_isr(&node, buffer[i]);
        }
        baton_node_process(&node, host_millis());
        if ((int32_t) (host_millis() - next_report) >= 0)
        {
            struct baton_stats stats;

            baton_node_get_stats(&node, &stats);
            printf("--- %u polls, %u answered, %u CRC errors\n", polls, answered,
                   (unsigned) stats.crc_errors);
            next_report += 1000U;
        }
    }
    serial_close();
    return 0;
}
