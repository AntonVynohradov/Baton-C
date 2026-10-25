---
title: Baton-C — deterministic messaging over one UART wire
description: Master/slave messaging library in C for embedded systems on a single-wire UART bus.
template: home.html
hide:
  - navigation
  - toc
---

<span class="sfs-eyebrow">Quick start</span>

## A minimal slave in one file

The application owns one `struct baton_node`, gives it a write function for the UART and
a callback for received messages, passes every received byte from the UART interrupt and
calls `baton_node_process()` every millisecond. This slave publishes its uptime as a
state and acknowledges every `CMD_SET_OUTPUT` event with an `OUTPUT_REPORT` event.

```c
#include "baton.h"

static const uint8_t slaves[] = {0x11U, 0x12U};
static struct baton_node node;

static int uart_write(void* context, const uint8_t* data, size_t length)
{
    (void) context;
    return board_uart_write(data, length); /* start a non-blocking transmission */
}

static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    (void) context;
    if ((msg_id == BATON_MSG_CMD_SET_OUTPUT) && (length == 2U))
    {
        (void) baton_node_send(&node, src, BATON_MSG_OUTPUT_REPORT, data, 2U, BATON_PRIO_HIGH);
    }
}

void uart_rx_irq_handler(uint8_t byte) /* called by the UART receive interrupt */
{
    baton_node_rx_isr(&node, byte);
}

int main(void)
{
    struct baton_node_config config;
    uint32_t next_status = 0U;

    baton_node_default_config(&config); /* 9600 bit/s, 250 ms slot, single-wire echo */
    config.own_address = 0x11U;
    config.master_address = 0x01U;
    config.slave_addresses = slaves;
    config.slave_count = 2U;
    config.write = uart_write;
    config.on_message = on_message;
    (void) baton_node_init(&node, &config, board_millis());

    for (;;)
    {
        const uint32_t now = board_millis();

        baton_node_process(&node, now);
        if ((int32_t) (now - next_status) >= 0)
        {
            uint8_t uptime[4];

            baton_put_u32le(uptime, now / 1000U);
            (void) baton_node_publish(&node, BATON_MSG_NODE_STATUS, uptime, 4U);
            next_status += 1000U;
        }
    }
}
```

`board_uart_write()` and `board_millis()` stand for your board's UART driver and tick. The
message lists (`NODE_STATUS`, `CMD_SET_OUTPUT`, `OUTPUT_REPORT`) come from
`baton_user_config.h`, shared by every node of the bus. The [examples](examples.md) are
complete programs for bare metal, Zephyr and a PC serial port.

## Find your way

| If you want to… | Read |
| --- | --- |
| Understand what the library does and does not do | [Introduction](overview.md) |
| Build it, run the tests, add it to your project | [Getting Started](guide.md) |
| Understand the layers, the slot schedule and the parser | [How It Works](architecture.md) |
| Write a master or a slave with it | [Using the Library](usage.md) |
| Add messages, change sizes or contribute | [Extending the Library](extending.md) |
| Look up the bytes on the wire | [Protocol](protocol.md) |
| Connect it to a UART, an RTOS or a build system | [Integration Guide](integration.md) |
| Look up a function | [API Reference](api.md) |
| Read everything offline or print it | [PDF user manual](https://antonvynohradov.github.io/baton-c/downloads/Baton-C_User_Manual.pdf) |
