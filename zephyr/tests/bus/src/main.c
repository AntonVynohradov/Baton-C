/*
 * Copyright (c) 2026 Anton Vynohradov
 * SPDX-License-Identifier: Apache-2.0
 *
 * A complete Baton bus in one image: three nodes of the Zephyr module on
 * three emulated UARTs joined into one wire. Every byte a node transmits
 * reaches every connected node, the sender included (the single-wire echo).
 * The wire can corrupt bytes and disconnect a node.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/serial/uart_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/ztest.h>

#include "baton_zephyr.h"

#define NODES 3U
#define MASTER 0U
#define SLAVE_A 1U
#define SLAVE_B 2U

#define MAX_RECORDS 256U

static const uint8_t address[NODES] = {0x01U, 0x11U, 0x12U};

static const struct device* const node_dev[NODES] = {
    DEVICE_DT_GET(DT_NODELABEL(master)),
    DEVICE_DT_GET(DT_NODELABEL(slave_a)),
    DEVICE_DT_GET(DT_NODELABEL(slave_b)),
};

static const struct device* const uart_dev[NODES] = {
    DEVICE_DT_GET(DT_NODELABEL(bus_uart0)),
    DEVICE_DT_GET(DT_NODELABEL(bus_uart1)),
    DEVICE_DT_GET(DT_NODELABEL(bus_uart2)),
};

/* ---- The wire --------------------------------------------------------------- */

static struct
{
    bool connected[NODES];
    uint32_t noise_period; /* corrupt one byte in noise_period (0 = none) */
    uint32_t rng;
    uint32_t corrupted;
} wire;

static uint32_t next_random(void)
{
    /* xorshift32: a fixed seed keeps every run identical. */
    wire.rng ^= wire.rng << 13;
    wire.rng ^= wire.rng >> 17;
    wire.rng ^= wire.rng << 5;
    return wire.rng;
}

static void wire_tx_ready(const struct device* uart, size_t size, void* user_data)
{
    const size_t from = POINTER_TO_UINT(user_data);
    uint8_t bytes[16];
    uint32_t count;

    ARG_UNUSED(size);

    /* All UART callbacks run in the one uart_emul work queue thread. */
    while ((count = uart_emul_get_tx_data(uart, bytes, sizeof(bytes))) > 0U)
    {
        for (uint32_t i = 0U; i < count; ++i)
        {
            if ((wire.noise_period != 0U) && ((next_random() % wire.noise_period) == 0U))
            {
                bytes[i] ^= (uint8_t) (1U << (next_random() % 8U));
                wire.corrupted++;
            }
        }
        for (size_t to = 0U; to < NODES; ++to)
        {
            /* A disconnected node still hears itself, nobody else hears it. */
            const bool linked = (to == from) || (wire.connected[from] && wire.connected[to]);

            if (linked)
            {
                (void) uart_emul_put_rx_data(uart_dev[to], bytes, count);
            }
        }
    }
}

/* ---- What every node received ----------------------------------------------- */

struct record
{
    uint8_t src;
    uint8_t msg_id;
    uint8_t data[4];
    uint8_t length;
};

static struct
{
    struct k_spinlock lock;
    struct record rx[NODES][MAX_RECORDS];
    uint32_t rx_count[NODES];
    uint32_t drops[NODES];
    uint32_t silent_polls[NODES]; /* master: polls without an answer, per node index */
} log;

static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    const size_t node = POINTER_TO_UINT(context);

    K_SPINLOCK(&log.lock)
    {
        if (log.rx_count[node] < MAX_RECORDS)
        {
            struct record* r = &log.rx[node][log.rx_count[node]];

            r->src = src;
            r->msg_id = msg_id;
            r->length = MIN(length, sizeof(r->data));
            memcpy(r->data, data, r->length);
        }
        log.rx_count[node]++;
    }
}

static void on_drop(void* context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes)
{
    const size_t node = POINTER_TO_UINT(context);

    ARG_UNUSED(msg_id);
    ARG_UNUSED(dst);
    ARG_UNUSED(missed_nodes);
    K_SPINLOCK(&log.lock)
    {
        log.drops[node]++;
    }
}

static void on_poll_done(void* context, uint8_t slave, bool responded, uint32_t nack_nodes)
{
    ARG_UNUSED(context);
    ARG_UNUSED(nack_nodes);
    K_SPINLOCK(&log.lock)
    {
        for (size_t i = 0U; i < NODES; ++i)
        {
            if ((address[i] == slave) && !responded)
            {
                log.silent_polls[i]++;
            }
        }
    }
}

static void clear_log(void)
{
    K_SPINLOCK(&log.lock)
    {
        memset(log.rx, 0, sizeof(log.rx));
        memset(log.rx_count, 0, sizeof(log.rx_count));
        memset(log.drops, 0, sizeof(log.drops));
        memset(log.silent_polls, 0, sizeof(log.silent_polls));
    }
}

static uint32_t count_from(size_t node, uint8_t src, uint8_t msg_id)
{
    uint32_t n = 0U;

    K_SPINLOCK(&log.lock)
    {
        for (uint32_t i = 0U; i < MIN(log.rx_count[node], MAX_RECORDS); ++i)
        {
            if ((log.rx[node][i].src == src) && (log.rx[node][i].msg_id == msg_id))
            {
                n++;
            }
        }
    }
    return n;
}

/* ---- Helpers ---------------------------------------------------------------- */

/* Zephyr's WAIT_FOR() polls with a sleep; the timeout is in milliseconds here. */
#define EVENTUALLY(cond, timeout_ms) WAIT_FOR((cond), (timeout_ms) * USEC_PER_MSEC, k_msleep(5))

/* Queue a numbered event, waiting while the queue is full. */
static void send_numbered(size_t from, size_t to, uint8_t msg_id, uint16_t number)
{
    const uint8_t data[2] = {(uint8_t) number, (uint8_t) (number >> 8)};
    int result;

    while ((result = baton_zephyr_send(node_dev[from], address[to], msg_id, data, sizeof(data),
                                       BATON_PRIO_NORMAL))
           == -ENOBUFS)
    {
        k_sleep(K_MSEC(5));
    }
    zassert_ok(result, "send from 0x%02x failed", address[from]);
}

/* Every numbered event from @p src arrived once, in order 0..n-1. */
static void check_exactly_once(size_t node, uint8_t src, uint8_t msg_id, uint16_t n)
{
    uint16_t expected = 0U;

    K_SPINLOCK(&log.lock)
    {
        for (uint32_t i = 0U; i < MIN(log.rx_count[node], MAX_RECORDS); ++i)
        {
            const struct record* r = &log.rx[node][i];

            if ((r->src == src) && (r->msg_id == msg_id))
            {
                const uint16_t number = (uint16_t) (r->data[0] | (r->data[1] << 8));

                zassert_equal(number, expected, "0x%02x got #%u from 0x%02x, expected #%u",
                              address[node], number, src, expected);
                expected++;
            }
        }
    }
    zassert_equal(expected, n, "0x%02x got %u of %u events", address[node], expected, n);
}

/* Numbered events from @p src arrived at most once and in order; returns how many. */
static uint32_t check_in_order(size_t node, uint8_t src, uint8_t msg_id)
{
    int32_t last = -1;
    uint32_t n = 0U;

    K_SPINLOCK(&log.lock)
    {
        for (uint32_t i = 0U; i < MIN(log.rx_count[node], MAX_RECORDS); ++i)
        {
            const struct record* r = &log.rx[node][i];

            if ((r->src == src) && (r->msg_id == msg_id))
            {
                const int32_t number = (int32_t) (r->data[0] | (r->data[1] << 8));

                zassert_true(number > last, "0x%02x got #%d after #%d from 0x%02x", address[node],
                             number, last, src);
                last = number;
                n++;
            }
        }
    }
    return n;
}

static uint32_t drops_of(size_t node)
{
    uint32_t n = 0U;

    K_SPINLOCK(&log.lock)
    {
        n = log.drops[node];
    }
    return n;
}

static uint32_t crc_errors_on_bus(void)
{
    struct baton_stats stats;
    uint32_t total = 0U;

    for (size_t i = 0U; i < NODES; ++i)
    {
        zassert_ok(baton_zephyr_get_stats(node_dev[i], &stats));
        total += stats.crc_errors;
    }
    return total;
}

/* ---- Suite ------------------------------------------------------------------ */

static void* bus_setup(void)
{
    wire.rng = CONFIG_BATON_TEST_SEED;
    for (size_t i = 0U; i < NODES; ++i)
    {
        wire.connected[i] = true;
        uart_emul_callback_tx_data_ready_set(uart_dev[i], wire_tx_ready, UINT_TO_POINTER(i));
    }
    for (size_t i = 0U; i < NODES; ++i)
    {
        const struct baton_zephyr_callbacks callbacks = {
            .on_message = on_message,
            .on_drop = on_drop,
            .on_poll_done = on_poll_done,
            .context = UINT_TO_POINTER(i),
        };

        zassert_true(device_is_ready(node_dev[i]));
        zassert_ok(baton_zephyr_start(node_dev[i], &callbacks));
    }
    return NULL;
}

static void bus_before(void* fixture)
{
    ARG_UNUSED(fixture);
    for (size_t i = 0U; i < NODES; ++i)
    {
        wire.connected[i] = true;
    }
    wire.noise_period = 0U;
    /* Let every channel settle after the previous test. */
    k_sleep(K_MSEC(300));
    clear_log();
}

ZTEST_SUITE(baton_bus, NULL, bus_setup, bus_before, NULL, NULL);

ZTEST(baton_bus, test_start_twice)
{
    zassert_equal(baton_zephyr_start(node_dev[MASTER], NULL), -EALREADY);
}

ZTEST(baton_bus, test_api_errors)
{
    const uint8_t data[4] = {0};

    zassert_equal(baton_zephyr_send(node_dev[MASTER], 0x11U, 0x7FU, data, 2U, BATON_PRIO_NORMAL),
                  -ENOMSG);
    zassert_equal(baton_zephyr_send(node_dev[MASTER], 0x11U, BATON_MSG_CMD_SET_OUTPUT, data, 4U,
                                    BATON_PRIO_NORMAL),
                  -EMSGSIZE);
    zassert_equal(baton_zephyr_send(node_dev[MASTER], 0x55U, BATON_MSG_CMD_SET_OUTPUT, data, 2U,
                                    BATON_PRIO_NORMAL),
                  -EADDRNOTAVAIL);
    zassert_equal(baton_zephyr_publish(node_dev[MASTER], BATON_MSG_CMD_SET_OUTPUT, data, 2U),
                  -ENOMSG);
    zassert_equal(baton_zephyr_get_stats(node_dev[MASTER], NULL), -EINVAL);
}

ZTEST(baton_bus, test_state_reaches_every_node)
{
    uint8_t status[4];

    baton_put_u32le(status, 0xC0FFEEU);
    zassert_ok(baton_zephyr_publish(node_dev[SLAVE_A], BATON_MSG_NODE_STATUS, status, 4U));

    zassert_true(EVENTUALLY((count_from(MASTER, 0x11U, BATON_MSG_NODE_STATUS) > 0U)
                                && (count_from(SLAVE_B, 0x11U, BATON_MSG_NODE_STATUS) > 0U),
                            2000));
    zassert_equal(count_from(SLAVE_A, 0x11U, BATON_MSG_NODE_STATUS), 0U, "echoed to itself");
}

ZTEST(baton_bus, test_events_master_to_slave_exactly_once)
{
    const uint16_t n = 60U;

    for (uint16_t i = 0U; i < n; ++i)
    {
        send_numbered(MASTER, SLAVE_B, BATON_MSG_CMD_SET_OUTPUT, i);
    }
    zassert_true(EVENTUALLY(count_from(SLAVE_B, 0x01U, BATON_MSG_CMD_SET_OUTPUT) >= n, 5000));
    k_sleep(K_MSEC(200)); /* no late duplicate */
    check_exactly_once(SLAVE_B, 0x01U, BATON_MSG_CMD_SET_OUTPUT, n);
}

ZTEST(baton_bus, test_events_slave_to_slave_exactly_once)
{
    const uint16_t n = 60U;

    for (uint16_t i = 0U; i < n; ++i)
    {
        send_numbered(SLAVE_A, SLAVE_B, BATON_MSG_OUTPUT_REPORT, i);
    }
    zassert_true(EVENTUALLY(count_from(SLAVE_B, 0x11U, BATON_MSG_OUTPUT_REPORT) >= n, 5000));
    k_sleep(K_MSEC(200));
    check_exactly_once(SLAVE_B, 0x11U, BATON_MSG_OUTPUT_REPORT, n);
}

ZTEST(baton_bus, test_noisy_line_loses_nothing)
{
    const uint16_t n = 100U;
    const uint32_t corrupted_before = wire.corrupted;
    const uint32_t crc_before = crc_errors_on_bus();

    /* About one byte in 150 corrupted: retransmissions recover every frame
     * well within the event TTL. */
    wire.noise_period = 150U;
    for (uint16_t i = 0U; i < n; ++i)
    {
        send_numbered(MASTER, SLAVE_A, BATON_MSG_CMD_SET_OUTPUT, i);
        send_numbered(SLAVE_A, MASTER, BATON_MSG_OUTPUT_REPORT, i);
    }
    zassert_true(EVENTUALLY((count_from(SLAVE_A, 0x01U, BATON_MSG_CMD_SET_OUTPUT) >= n)
                                && (count_from(MASTER, 0x11U, BATON_MSG_OUTPUT_REPORT) >= n),
                            20000));
    wire.noise_period = 0U;
    k_sleep(K_MSEC(300));

    check_exactly_once(SLAVE_A, 0x01U, BATON_MSG_CMD_SET_OUTPUT, n);
    check_exactly_once(MASTER, 0x11U, BATON_MSG_OUTPUT_REPORT, n);
    zassert_equal(drops_of(MASTER) + drops_of(SLAVE_A), 0U);

    TC_PRINT("corrupted bytes: %u, CRC errors: %u, events lost: 0 of %u\n",
             wire.corrupted - corrupted_before, crc_errors_on_bus() - crc_before, 2U * n);
    zassert_true(wire.corrupted > corrupted_before, "the wire injected no noise");
    zassert_true(crc_errors_on_bus() > crc_before, "no node detected the noise");
}

ZTEST(baton_bus, test_heavy_noise_every_loss_is_reported)
{
    const uint16_t n = 100U;
    uint32_t delivered_down;
    uint32_t delivered_up;

    /* About one byte in 40 corrupted: some events exhaust their TTL. None may
     * be duplicated or reordered, and the sender learns about every event it
     * could not deliver (a delivered event whose acknowledgement was lost is
     * reported too, so delivered + dropped >= sent). */
    wire.noise_period = 40U;
    for (uint16_t i = 0U; i < n; ++i)
    {
        send_numbered(MASTER, SLAVE_A, BATON_MSG_CMD_SET_OUTPUT, i);
        send_numbered(SLAVE_A, MASTER, BATON_MSG_OUTPUT_REPORT, i);
    }
    zassert_true(EVENTUALLY(
        (count_from(SLAVE_A, 0x01U, BATON_MSG_CMD_SET_OUTPUT) + drops_of(MASTER) >= n)
            && (count_from(MASTER, 0x11U, BATON_MSG_OUTPUT_REPORT) + drops_of(SLAVE_A) >= n),
        30000));
    wire.noise_period = 0U;
    k_sleep(K_MSEC(2000)); /* past the TTL: every pending event is settled */

    delivered_down = check_in_order(SLAVE_A, 0x01U, BATON_MSG_CMD_SET_OUTPUT);
    delivered_up = check_in_order(MASTER, 0x11U, BATON_MSG_OUTPUT_REPORT);
    TC_PRINT("master->0x11: %u delivered, %u reported lost; 0x11->master: %u delivered, "
             "%u reported lost (of %u each)\n",
             delivered_down, drops_of(MASTER), delivered_up, drops_of(SLAVE_A), n);
    zassert_true(delivered_down + drops_of(MASTER) >= n, "an event vanished without a report");
    zassert_true(delivered_up + drops_of(SLAVE_A) >= n, "an event vanished without a report");
}

ZTEST(baton_bus, test_disconnected_slave_is_reported_and_recovers)
{
    const uint8_t data[2] = {0U, 1U};

    wire.connected[SLAVE_B] = false;

    /* The master notices the silent slave and reports the event it cannot deliver. */
    zassert_ok(baton_zephyr_send(node_dev[MASTER], 0x12U, BATON_MSG_CMD_SET_OUTPUT, data, 2U,
                                 BATON_PRIO_HIGH));
    zassert_true(EVENTUALLY(log.silent_polls[SLAVE_B] > 0U, 2000), "no silent poll reported");
    zassert_true(EVENTUALLY(log.drops[MASTER] > 0U, 4000), "undeliverable event not reported");
    zassert_equal(count_from(SLAVE_B, 0x01U, BATON_MSG_CMD_SET_OUTPUT), 0U);

    /* The rest of the bus keeps working meanwhile. */
    send_numbered(MASTER, SLAVE_A, BATON_MSG_CMD_SET_OUTPUT, 0U);
    zassert_true(EVENTUALLY(count_from(SLAVE_A, 0x01U, BATON_MSG_CMD_SET_OUTPUT) > 0U, 2000));

    /* Reconnected, the slave is polled again and receives new events. */
    wire.connected[SLAVE_B] = true;
    send_numbered(MASTER, SLAVE_B, BATON_MSG_CMD_SET_OUTPUT, 7U);
    zassert_true(EVENTUALLY(count_from(SLAVE_B, 0x01U, BATON_MSG_CMD_SET_OUTPUT) > 0U, 3000),
                 "slave not reachable after reconnection");
}
