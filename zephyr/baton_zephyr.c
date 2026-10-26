/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Zephyr module: devicetree instances, UART interrupts, protocol thread
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
 * @file baton_zephyr.c
 * @brief One Baton node per "systemfromscratch,baton" devicetree node.
 * @details The UART interrupt passes every received byte to
 *          baton_node_rx_isr() and fills the transmit FIFO from the frame the
 *          library keeps valid until the end of the transmission. The echo of
 *          the last byte ends the transmission (the library runs with rx_echo)
 *          and releases the optional RS-485 driver enable. A protocol thread
 *          per node calls baton_node_process() every
 *          CONFIG_BATON_PROCESS_PERIOD_MS and wakes up early on received bytes
 *          and new messages. A spinlock is the critical section shared with
 *          the interrupt, so baton_zephyr_send() and baton_zephyr_publish()
 *          may be called from any thread.
 */

#define DT_DRV_COMPAT systemfromscratch_baton

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>

#include "baton_zephyr.h"

LOG_MODULE_REGISTER(baton, CONFIG_BATON_LOG_LEVEL);

/* The library times frames, gaps and inter-byte timeouts in milliseconds
 * (2 ms bus gap and 5 ms inter-byte timeout by default); a coarser uptime
 * makes these timeouts fire early or late by a whole tick. */
BUILD_ASSERT(CONFIG_SYS_CLOCK_TICKS_PER_SEC >= 1000,
             "Baton-C needs a system tick of 1 ms or finer: "
             "set CONFIG_SYS_CLOCK_TICKS_PER_SEC=1000");

#define BATON_HAS_DE DT_ANY_INST_HAS_PROP_STATUS_OKAY(de_gpios)

/* ==============================================================================
 * TYPES
 * =============================================================================*/

struct baton_zephyr_config
{
    const struct device* uart;
#if BATON_HAS_DE
    struct gpio_dt_spec de;
#endif
    const uint8_t* slave_addresses;
    uint8_t slave_count;
    uint8_t role;
    uint8_t address;
    uint8_t master_address;
    uint32_t baudrate;
    uint16_t slot_time_ms;
    uint16_t ack_timeout_ms;
    int16_t max_retries; /* < 0: library default */
    uint8_t tx_gap_ms;
    uint8_t inter_byte_timeout_ms;
    uint16_t cycle_period_ms;
    uint32_t event_ttl_ms;
    k_thread_stack_t* stack;
    size_t stack_size;
};

struct baton_zephyr_data
{
    struct baton_node node;
    struct k_spinlock lock;
    k_spinlock_key_t lock_key;
    struct k_sem wake;
    struct k_thread thread;
    struct baton_zephyr_callbacks callbacks;
    const struct device* dev;

    /* Transmission in progress, owned by the UART interrupt once started. */
    const uint8_t* tx_data;
    size_t tx_length;
    size_t tx_position;
    size_t tx_echoed;
    bool tx_active;

    bool ready;
    bool started;
};

/* ==============================================================================
 * PRIVATE FUNCTIONS
 * =============================================================================*/

static int to_errno(int result)
{
    switch (result)
    {
        case BATON_OK:
            return 0;
        case BATON_ERR_MSG_ID:
            return -ENOMSG;
        case BATON_ERR_LEN:
            return -EMSGSIZE;
        case BATON_ERR_ADDR:
            return -EADDRNOTAVAIL;
        case BATON_ERR_FULL:
            return -ENOBUFS;
        case BATON_ERR_NOT_INIT:
            return -ENODEV;
        default:
            return -EINVAL;
    }
}

static void driver_enable(const struct device* dev, bool enable)
{
#if BATON_HAS_DE
    const struct baton_zephyr_config* config = dev->config;

    if (config->de.port != NULL)
    {
        (void) gpio_pin_set_dt(&config->de, enable ? 1 : 0);
    }
#else
    ARG_UNUSED(dev);
    ARG_UNUSED(enable);
#endif
}

static void uart_isr(const struct device* uart, void* user_data)
{
    const struct device* dev = user_data;
    struct baton_zephyr_data* data = dev->data;
    bool received = false;
    /* The same lock as the library's critical section: on SMP another CPU may
     * be inside baton_node_process() or baton_zephyr_send(). */
    const k_spinlock_key_t key = k_spin_lock(&data->lock);

    for (;;)
    {
        /* Returns int up to Zephyr 4.4 and void from 4.5: call it as a statement. */
        uart_irq_update(uart);
        if (uart_irq_is_pending(uart) <= 0)
        {
            break;
        }

        if (uart_irq_rx_ready(uart) > 0)
        {
            uint8_t bytes[16];
            const int count = uart_fifo_read(uart, bytes, sizeof(bytes));

            for (int i = 0; i < count; ++i)
            {
                baton_node_rx_isr(&data->node, bytes[i]);
                if (data->tx_active)
                {
                    /* Our own byte came back: once the last one is here, the
                     * last stop bit has left the transceiver. */
                    data->tx_echoed++;
                    if (data->tx_echoed >= data->tx_length)
                    {
                        data->tx_active = false;
                        driver_enable(dev, false);
                    }
                }
            }
            received = received || (count > 0);
        }

        if (uart_irq_tx_ready(uart) > 0)
        {
            if (data->tx_position < data->tx_length)
            {
                const int sent = uart_fifo_fill(uart, &data->tx_data[data->tx_position],
                                                (int) (data->tx_length - data->tx_position));

                data->tx_position += (sent > 0) ? (size_t) sent : 0U;
            }
            if (data->tx_position >= data->tx_length)
            {
                uart_irq_tx_disable(uart);
            }
        }
    }

    k_spin_unlock(&data->lock, key);

    if (received)
    {
        k_sem_give(&data->wake);
    }
}

/* ---- Library hooks ---------------------------------------------------------- */

static int hook_write(void* context, const uint8_t* bytes, size_t length)
{
    const struct device* dev = context;
    const struct baton_zephyr_config* config = dev->config;
    struct baton_zephyr_data* data = dev->data;

    if (length == 0U)
    {
        return -EINVAL;
    }

    /* The library writes only after the previous frame ended (echo) or timed
     * out, so a transmission still marked active here lost its echo: the new
     * frame replaces it. */
    K_SPINLOCK(&data->lock)
    {
        uart_irq_tx_disable(config->uart);
        data->tx_data = bytes;
        data->tx_length = length;
        data->tx_position = 0U;
        data->tx_echoed = 0U;
        data->tx_active = true;
        driver_enable(dev, true);
    }
    uart_irq_tx_enable(config->uart);
    return 0;
}

static void hook_enter_critical(void* context)
{
    const struct device* dev = context;
    struct baton_zephyr_data* data = dev->data;
    const k_spinlock_key_t key = k_spin_lock(&data->lock);

    data->lock_key = key;
}

static void hook_exit_critical(void* context)
{
    const struct device* dev = context;
    struct baton_zephyr_data* data = dev->data;

    k_spin_unlock(&data->lock, data->lock_key);
}

static void hook_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* bytes,
                         uint8_t length)
{
    const struct device* dev = context;
    struct baton_zephyr_data* data = dev->data;

    if (data->callbacks.on_message != NULL)
    {
        data->callbacks.on_message(data->callbacks.context, src, msg_id, bytes, length);
    }
}

static void hook_drop(void* context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes)
{
    const struct device* dev = context;
    struct baton_zephyr_data* data = dev->data;

    LOG_DBG("%s: event 0x%02x to 0x%02x dropped (missed 0x%08x)", dev->name, msg_id, dst,
            missed_nodes);
    if (data->callbacks.on_drop != NULL)
    {
        data->callbacks.on_drop(data->callbacks.context, msg_id, dst, missed_nodes);
    }
}

static void hook_poll_done(void* context, uint8_t slave, bool responded, uint32_t nack_nodes)
{
    const struct device* dev = context;
    struct baton_zephyr_data* data = dev->data;

    if (data->callbacks.on_poll_done != NULL)
    {
        data->callbacks.on_poll_done(data->callbacks.context, slave, responded, nack_nodes);
    }
}

/* ---- Protocol thread -------------------------------------------------------- */

static void baton_thread(void* p1, void* p2, void* p3)
{
    const struct device* dev = p1;
    struct baton_zephyr_data* data = dev->data;

    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (;;)
    {
        baton_node_process(&data->node, k_uptime_get_32());
        (void) k_sem_take(&data->wake, K_MSEC(CONFIG_BATON_PROCESS_PERIOD_MS));
    }
}

/* ---- Device initialization -------------------------------------------------- */

static int baton_zephyr_init(const struct device* dev)
{
    const struct baton_zephyr_config* config = dev->config;
    struct baton_zephyr_data* data = dev->data;
    struct baton_node_config node_config;
    int result;

    if (!device_is_ready(config->uart))
    {
        LOG_ERR("%s: UART %s not ready", dev->name, config->uart->name);
        return -ENODEV;
    }

#if BATON_HAS_DE
    if (config->de.port != NULL)
    {
        if (!gpio_is_ready_dt(&config->de))
        {
            LOG_ERR("%s: driver enable GPIO not ready", dev->name);
            return -ENODEV;
        }
        result = gpio_pin_configure_dt(&config->de, GPIO_OUTPUT_INACTIVE);
        if (result != 0)
        {
            return result;
        }
    }
#endif

    data->dev = dev;
    k_sem_init(&data->wake, 0, 1);

    baton_node_default_config(&node_config);
    node_config.role = config->role;
    node_config.own_address = config->address;
    node_config.master_address = config->master_address;
    node_config.slave_addresses = config->slave_addresses;
    node_config.slave_count = config->slave_count;
    node_config.baudrate = config->baudrate;
    node_config.slot_time_ms =
        (config->slot_time_ms != 0U) ? config->slot_time_ms : node_config.slot_time_ms;
    node_config.ack_timeout_ms =
        (config->ack_timeout_ms != 0U) ? config->ack_timeout_ms : node_config.ack_timeout_ms;
    node_config.max_retries =
        (config->max_retries >= 0) ? (uint8_t) config->max_retries : node_config.max_retries;
    node_config.tx_gap_ms = (config->tx_gap_ms != 0U) ? config->tx_gap_ms : node_config.tx_gap_ms;
    node_config.inter_byte_timeout_ms = (config->inter_byte_timeout_ms != 0U) ?
                                            config->inter_byte_timeout_ms :
                                            node_config.inter_byte_timeout_ms;
    node_config.cycle_period_ms = config->cycle_period_ms;
    node_config.event_ttl_ms =
        (config->event_ttl_ms != 0U) ? config->event_ttl_ms : node_config.event_ttl_ms;
    node_config.rx_echo = true;
    node_config.write = hook_write;
    node_config.enter_critical = hook_enter_critical;
    node_config.exit_critical = hook_exit_critical;
    node_config.on_message = hook_message;
    node_config.on_drop = hook_drop;
    node_config.on_poll_done = hook_poll_done;
    node_config.context = (void*) dev;

    result = baton_node_init(&data->node, &node_config, k_uptime_get_32());
    if (result != BATON_OK)
    {
        LOG_ERR("%s: invalid configuration (topology, timing or message lists)", dev->name);
        return -EINVAL;
    }

    uart_irq_rx_disable(config->uart);
    uart_irq_tx_disable(config->uart);
    result = uart_irq_callback_user_data_set(config->uart, uart_isr, (void*) dev);
    if (result != 0)
    {
        LOG_ERR("%s: UART %s has no interrupt-driven API", dev->name, config->uart->name);
        return result;
    }

    (void) k_thread_create(&data->thread, config->stack, config->stack_size, baton_thread,
                           (void*) dev, NULL, NULL, K_PRIO_PREEMPT(CONFIG_BATON_THREAD_PRIORITY), 0,
                           K_FOREVER);
    (void) k_thread_name_set(&data->thread, dev->name);

    data->ready = true;
    LOG_INF("%s: %s 0x%02x on %s at %u bit/s", dev->name,
            (config->role == BATON_ROLE_MASTER) ? "master" : "slave", config->address,
            config->uart->name, config->baudrate);
    return 0;
}

/* ==============================================================================
 * PUBLIC FUNCTIONS
 * =============================================================================*/

int baton_zephyr_start(const struct device* dev, const struct baton_zephyr_callbacks* callbacks)
{
    const struct baton_zephyr_config* config = dev->config;
    struct baton_zephyr_data* data = dev->data;

    if (!data->ready)
    {
        return -ENODEV;
    }
    if (data->started)
    {
        return -EALREADY;
    }

    if (callbacks != NULL)
    {
        data->callbacks = *callbacks;
    }
    data->started = true;
    uart_irq_rx_enable(config->uart);
    k_thread_start(&data->thread);
    return 0;
}

int baton_zephyr_send(const struct device* dev, uint8_t dst, uint8_t msg_id, const void* bytes,
                      uint8_t length, uint8_t priority)
{
    struct baton_zephyr_data* data = dev->data;
    int result;

    if (!data->ready)
    {
        return -ENODEV;
    }
    result = to_errno(baton_node_send(&data->node, dst, msg_id, bytes, length, priority));
    if (result == 0)
    {
        k_sem_give(&data->wake);
    }
    return result;
}

int baton_zephyr_publish(const struct device* dev, uint8_t msg_id, const void* bytes,
                         uint8_t length)
{
    struct baton_zephyr_data* data = dev->data;
    int result;

    if (!data->ready)
    {
        return -ENODEV;
    }
    result = to_errno(baton_node_publish(&data->node, msg_id, bytes, length));
    if (result == 0)
    {
        k_sem_give(&data->wake);
    }
    return result;
}

int baton_zephyr_get_stats(const struct device* dev, struct baton_stats* stats)
{
    struct baton_zephyr_data* data = dev->data;

    if (stats == NULL)
    {
        return -EINVAL;
    }
    if (!data->ready)
    {
        return -ENODEV;
    }
    hook_enter_critical((void*) dev);
    baton_node_get_stats(&data->node, stats);
    hook_exit_critical((void*) dev);
    return 0;
}

/* ==============================================================================
 * DEVICE INSTANCES
 * =============================================================================*/

#if BATON_HAS_DE
#define BATON_DE_INIT(inst) .de = GPIO_DT_SPEC_INST_GET_OR(inst, de_gpios, {0}),
#else
#define BATON_DE_INIT(inst)
#endif

#define BATON_ZEPHYR_DEFINE(inst)                                                                  \
    BUILD_ASSERT(DT_INST_PROP_LEN(inst, slave_addresses) < BATON_MAX_NODES,                        \
                 "slave-addresses: more slaves than BATON_MAX_NODES - 1");                         \
    BUILD_ASSERT(DT_NODE_HAS_PROP(DT_INST_BUS(inst), current_speed),                               \
                 "the parent UART of a Baton node needs current-speed");                           \
    static const uint8_t baton_slaves_##inst[] = DT_INST_PROP(inst, slave_addresses);              \
    static K_KERNEL_STACK_DEFINE(baton_stack_##inst, CONFIG_BATON_THREAD_STACK_SIZE);              \
    static struct baton_zephyr_data baton_data_##inst;                                             \
    static const struct baton_zephyr_config baton_config_##inst = {                                \
        .uart = DEVICE_DT_GET(DT_INST_BUS(inst)),                                                  \
        BATON_DE_INIT(inst).slave_addresses = baton_slaves_##inst,                                 \
        .slave_count = (uint8_t) sizeof(baton_slaves_##inst),                                      \
        .role = (uint8_t) DT_INST_ENUM_IDX(inst, role),                                            \
        .address = (uint8_t) DT_INST_PROP(inst, address),                                          \
        .master_address = (uint8_t) DT_INST_PROP(inst, master_address),                            \
        .baudrate = DT_PROP(DT_INST_BUS(inst), current_speed),                                     \
        .slot_time_ms = DT_INST_PROP_OR(inst, slot_time_ms, 0),                                    \
        .ack_timeout_ms = DT_INST_PROP_OR(inst, ack_timeout_ms, 0),                                \
        .max_retries = DT_INST_PROP_OR(inst, max_retries, -1),                                     \
        .tx_gap_ms = DT_INST_PROP_OR(inst, tx_gap_ms, 0),                                          \
        .inter_byte_timeout_ms = DT_INST_PROP_OR(inst, inter_byte_timeout_ms, 0),                  \
        .cycle_period_ms = DT_INST_PROP_OR(inst, cycle_period_ms, 0),                              \
        .event_ttl_ms = DT_INST_PROP_OR(inst, event_ttl_ms, 0),                                    \
        .stack = baton_stack_##inst,                                                               \
        .stack_size = K_KERNEL_STACK_SIZEOF(baton_stack_##inst),                                   \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(inst, baton_zephyr_init, NULL, &baton_data_##inst, &baton_config_##inst, \
                          POST_KERNEL, CONFIG_BATON_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(BATON_ZEPHYR_DEFINE)
