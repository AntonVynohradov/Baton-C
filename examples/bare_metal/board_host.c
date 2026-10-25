/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Host implementation of board.h: simulated bus with a master and
 *              a second slave, runs the bare-metal example as a test
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
 * @file board_host.c
 * @brief Host implementation of board.h that runs the bare-metal example as a test.
 * @details board_idle() advances a simulated clock by 1 ms and moves one byte per
 *          transmitter over a simulated single-wire bus (9600 bit/s is 0.96 bytes
 *          per millisecond). Every byte reaches every node, the sender included
 *          (echo). The master 0x01 and the slave 0x12 run on the host as further
 *          Baton nodes. Before every request a burst of line noise is put on the
 *          idle bus. The process exits with 0 when every check passed.
 */

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "baton.h"
#include "board.h"

/* ==============================================================================
 * DEFINES & MACROS
 * =============================================================================*/

#define MASTER_ADDRESS 0x01U
#define DEVICE_ADDRESS 0x11U
#define PEER_ADDRESS 0x12U
#define RUN_TIME_MS 6000U
#define TX_COUNT 3 /* device, master, peer */

/* ==============================================================================
 * PRIVATE TYPE DEFINITIONS
 * =============================================================================*/

/** One UART on the simulated bus. */
struct transmitter
{
    uint8_t data[BATON_FRAME_MAX];
    size_t length;
    size_t position;
};

struct received
{
    int output_reports;
    uint8_t last_report[2];
    uint32_t device_uptime;
};

/* ==============================================================================
 * PRIVATE VARIABLES (static)
 * =============================================================================*/

static const uint8_t slave_addresses[] = {DEVICE_ADDRESS, PEER_ADDRESS};
static const uint8_t noise[] = {0xA5U, 0x11U, 0x01U, 0x00U, 0x5AU};

static uint32_t now_ms = 1000U;
static struct transmitter tx[TX_COUNT]; /* [0] device, [1] master, [2] peer */
static struct baton_node master;
static struct baton_node peer;
static struct received at_master;
static struct received at_peer;
static unsigned collisions;
static unsigned polls;
static unsigned failed_polls;
static int step;

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

static int start_tx(struct transmitter* t, const uint8_t* data, size_t length)
{
    int result = -1;

    if ((t->position >= t->length) && (length <= sizeof(t->data)))
    {
        memcpy(t->data, data, length);
        t->length = length;
        t->position = 0U;
        result = 0;
    }
    return result;
}

static int master_write(void* context, const uint8_t* data, size_t length)
{
    (void) context;
    return start_tx(&tx[1], data, length);
}

static int peer_write(void* context, const uint8_t* data, size_t length)
{
    (void) context;
    return start_tx(&tx[2], data, length);
}

static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    struct received* r = (struct received*) context;
    const char* who = (r == &at_master) ? "master" : "0x12  ";

    if ((msg_id == BATON_MSG_OUTPUT_REPORT) && (length == 2U))
    {
        r->output_reports++;
        r->last_report[0] = data[0];
        r->last_report[1] = data[1];
        printf("%6u ms  %s <- 0x%02X  OUTPUT_REPORT output %u = 0x%02X\n", (unsigned) now_ms, who,
               (unsigned) src, (unsigned) data[0], (unsigned) data[1]);
    }
    else if ((msg_id == BATON_MSG_NODE_STATUS) && (length == 4U) && (src == DEVICE_ADDRESS))
    {
        r->device_uptime = baton_get_u32le(data);
    }
    else
    {
        /* other traffic of the example bus */
    }
}

static void on_poll_done(void* context, uint8_t slave, bool responded, uint32_t nack_nodes)
{
    (void) context;
    (void) slave;
    (void) nack_nodes;
    polls++;
    if (!responded)
    {
        failed_polls++;
    }
}

static void init_host_node(struct baton_node* node, uint8_t role, uint8_t address,
                           baton_write_fn write, struct received* r)
{
    struct baton_node_config config;

    baton_node_default_config(&config);
    config.role = role;
    config.own_address = address;
    config.master_address = MASTER_ADDRESS;
    config.slave_addresses = slave_addresses;
    config.slave_count = (uint8_t) sizeof(slave_addresses);
    config.write = write;
    config.on_message = on_message;
    config.on_poll_done = (role == BATON_ROLE_MASTER) ? on_poll_done : NULL;
    config.context = r;
    if (baton_node_init(node, &config, now_ms) != BATON_OK)
    {
        printf("FAIL: host node 0x%02X does not start\n", (unsigned) address);
        exit(EXIT_FAILURE);
    }
}

static bool bus_idle(void)
{
    bool idle = true;

    for (int i = 0; i < TX_COUNT; ++i)
    {
        if (tx[i].position < tx[i].length)
        {
            idle = false;
        }
    }
    return idle;
}

static void deliver(uint8_t byte)
{
    app_uart_rx_isr(byte);
    baton_node_rx_isr(&master, byte);
    baton_node_rx_isr(&peer, byte);
}

/* Move one byte per active transmitter; two at once is a collision. */
static void bus_step(void)
{
    int active = 0;

    for (int i = 0; i < TX_COUNT; ++i)
    {
        if (tx[i].position < tx[i].length)
        {
            active++;
            deliver(tx[i].data[tx[i].position]);
            tx[i].position++;
        }
    }
    if (active > 1)
    {
        collisions++;
    }
}

static void request(const char* text, struct baton_node* from, uint8_t output, uint8_t value)
{
    const uint8_t command[2] = {output, value};

    /* line noise first: a broken header and a stray byte on the idle bus */
    for (size_t i = 0U; i < sizeof(noise); ++i)
    {
        deliver(noise[i]);
    }
    printf("%6u ms  %s -> 0x11  CMD_SET_OUTPUT output %u = 0x%02X (after line noise)\n",
           (unsigned) now_ms, text, (unsigned) output, (unsigned) value);
    if (baton_node_send(from, DEVICE_ADDRESS, BATON_MSG_CMD_SET_OUTPUT, command, 2U,
                        BATON_PRIO_NORMAL)
        != BATON_OK)
    {
        printf("FAIL: send\n");
        exit(EXIT_FAILURE);
    }
}

static int check(bool ok, const char* what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    return ok ? 0 : 1;
}

static void finish(void)
{
    struct baton_stats stats;
    int failures = 0;

    baton_node_get_stats(&master, &stats);
    printf("master: %u polls (%u without answer), %u CRC errors, %u frames aborted by timeout\n",
           polls, failed_polls, (unsigned) stats.crc_errors, (unsigned) stats.rx_timeouts);
    failures += check(at_master.output_reports == 2, "master got 2 OUTPUT_REPORT events");
    failures += check((at_master.last_report[0] == 2U) && (at_master.last_report[1] == 0x00U),
                      "last report to the master: output 2 = 0x00");
    failures += check(at_peer.output_reports == 1, "slave 0x12 got 1 OUTPUT_REPORT event");
    failures += check((at_peer.last_report[0] == 1U) && (at_peer.last_report[1] == 0x55U),
                      "report to 0x12: output 1 = 0x55");
    failures += check(at_master.device_uptime >= 5U, "master sees the uptime state of 0x11");
    failures += check(at_peer.device_uptime >= 5U, "slave 0x12 sees the uptime state of 0x11");
    failures += check(collisions == 0U, "no collisions on the bus");
    printf("%s\n", (failures == 0) ? "EXAMPLE PASSED" : "EXAMPLE FAILED");
    exit((failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

void board_init(uint32_t baud_rate)
{
    printf("bare-metal example: slave 0x11 at %u bit/s on a simulated single-wire bus\n",
           (unsigned) baud_rate);
    init_host_node(&master, BATON_ROLE_MASTER, MASTER_ADDRESS, master_write, &at_master);
    init_host_node(&peer, BATON_ROLE_SLAVE, PEER_ADDRESS, peer_write, &at_peer);
}

uint32_t board_millis(void)
{
    return now_ms;
}

int board_uart_write(const uint8_t* data, size_t length)
{
    return start_tx(&tx[0], data, length);
}

void board_idle(void)
{
    now_ms++;
    bus_step();
    baton_node_process(&master, now_ms);
    baton_node_process(&peer, now_ms);

    /* each request at the first idle millisecond after its start time */
    if ((step == 0) && (now_ms >= 1500U) && bus_idle())
    {
        request("master", &master, 0U, 1U);
        step++;
    }
    if ((step == 1) && (now_ms >= 2200U) && bus_idle())
    {
        request("0x12  ", &peer, 1U, 0x55U);
        step++;
    }
    if ((step == 2) && (now_ms >= 3100U) && bus_idle())
    {
        request("master", &master, 2U, 0x00U);
        step++;
    }
    if (now_ms >= (1000U + RUN_TIME_MS))
    {
        finish();
    }
}
