/*
 * Copyright (c) 2026 Anton Vynohradov
 * SPDX-License-Identifier: Apache-2.0
 *
 * Baton-C node of the example bus. The role, address and topology come from
 * the "systemfromscratch,baton" devicetree node; the UART interrupts and the
 * protocol thread live in the Baton-C Zephyr module.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "baton_zephyr.h"

LOG_MODULE_REGISTER(baton_node, LOG_LEVEL_INF);

#define BATON_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(systemfromscratch_baton)
#define IS_MASTER (DT_ENUM_IDX(BATON_NODE, role) == BATON_ROLE_MASTER)

static const struct device* const bus = DEVICE_DT_GET(BATON_NODE);
static const uint8_t slaves[] = DT_PROP(BATON_NODE, slave_addresses);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    ARG_UNUSED(context);

    switch (msg_id)
    {
        case BATON_MSG_CMD_SET_OUTPUT:
            if (length == 2U)
            {
                if ((data[0] == 0U) && (led.port != NULL))
                {
                    (void) gpio_pin_set_dt(&led, data[1]);
                }
                /* Confirm to the sender, in the protocol thread. */
                (void) baton_zephyr_send(bus, src, BATON_MSG_OUTPUT_REPORT, data, 2U,
                                         BATON_PRIO_HIGH);
            }
            break;
        case BATON_MSG_OUTPUT_REPORT:
            if (length == 2U)
            {
                LOG_INF("0x%02x: output %u = %u", src, data[0], data[1]);
            }
            break;
        case BATON_MSG_NODE_STATUS:
            if (length == 4U)
            {
                LOG_INF("0x%02x: up %u s", src, baton_get_u32le(data));
            }
            break;
        default:
            break;
    }
}

static void on_drop(void* context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes)
{
    ARG_UNUSED(context);
    LOG_WRN("event 0x%02x to 0x%02x not delivered (nodes 0x%x)", msg_id, dst, missed_nodes);
}

static void on_poll_done(void* context, uint8_t slave, bool responded, uint32_t nack_nodes)
{
    ARG_UNUSED(context);
    ARG_UNUSED(nack_nodes);
    if (!responded)
    {
        LOG_WRN("0x%02x did not answer", slave);
    }
}

int main(void)
{
    const struct baton_zephyr_callbacks callbacks = {
        .on_message = on_message,
        .on_drop = on_drop,
        .on_poll_done = on_poll_done,
    };
    uint8_t output = 0U;
    int result;

    if (led.port != NULL)
    {
        (void) gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    }

    result = baton_zephyr_start(bus, &callbacks);
    if (result != 0)
    {
        LOG_ERR("cannot start the Baton node: %d", result);
        return result;
    }

    for (;;)
    {
        k_sleep(K_SECONDS(1));

        if (IS_MASTER)
        {
            const uint8_t command[2] = {0U, output};

            for (size_t i = 0; i < ARRAY_SIZE(slaves); ++i)
            {
                (void) baton_zephyr_send(bus, slaves[i], BATON_MSG_CMD_SET_OUTPUT, command,
                                         sizeof(command), BATON_PRIO_NORMAL);
            }
            output ^= 1U;
        }
        else
        {
            uint8_t status[4];

            baton_put_u32le(status, k_uptime_get_32() / 1000U);
            (void) baton_zephyr_publish(bus, BATON_MSG_NODE_STATUS, status, sizeof(status));
        }
    }

    return 0;
}
