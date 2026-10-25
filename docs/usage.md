# Using the Library

This chapter shows how to set up a node, write a slave and a master, send events and
publish states, read the diagnostics, use the codec on its own, and avoid the common
mistakes. The complete programs are in the [examples](examples.md).

## Define the messages

Every node of a bus is built with the same `baton_user_config.h`. Each message is
`X(NAME, ID, MAX_LEN)`; the library generates `BATON_MSG_<NAME>` (the id) and
`BATON_MSG_<NAME>_MAX_LEN`:

```c
#define BATON_STATE_LIST(X)                                                                        \
    X(NODE_STATUS, 0x01, 4)                                                                        \
    X(TEMPERATURE, 0x02, 2)

#define BATON_EVENT_LIST(X)                                                                        \
    X(CMD_START, 0x10, 8)                                                                          \
    X(CMD_STOP, 0x11, 1)                                                                           \
    X(ALARM, 0x12, 16)
```

| Rule | Checked |
| --- | --- |
| IDs 0..255, unique across both lists | At compile time (duplicate `case` value, `_Static_assert`) |
| `MAX_LEN` 1..`BATON_MAX_MSG_LEN` | At compile time |
| Each event fits into both queues | At compile time |
| Same lists on every node | Not checkable — a mismatch shows as `bad_messages` |

Use a **state** for a value whose latest version matters (a measurement, a mode, a
heartbeat); use an **event** for something that must happen exactly once (a command, an
alarm, a button press).

## Set up a node

```c
static const uint8_t slaves[] = {0x11U, 0x12U, 0x13U}; /* polling order, same on every node */
static struct baton_node node;

int start_node(void)
{
    struct baton_node_config config;

    baton_node_default_config(&config);
    config.role = BATON_ROLE_SLAVE;
    config.own_address = 0x12U;
    config.master_address = 0x01U;
    config.slave_addresses = slaves;
    config.slave_count = 3U;
    config.write = uart_write;
    config.on_message = on_message;
    return baton_node_init(&node, &config, board_millis());
}
```

| Field | Default | Meaning |
| --- | --- | --- |
| `role` | `BATON_ROLE_SLAVE` | `BATON_ROLE_MASTER` or `BATON_ROLE_SLAVE` |
| `own_address`, `master_address` | — | The master uses `own_address == master_address` |
| `slave_addresses`, `slave_count` | — | Slaves in polling order, 1..`BATON_MAX_NODES` − 1; the array must stay valid |
| `baudrate` | 9600 | Used for frame durations and the slot budget |
| `slot_time_ms` | 250 | Longest slot; at least the minimum checked by init |
| `ack_timeout_ms` | 20 | Wait for an ACK after DATA |
| `max_retries` | 1 | DATA retransmissions per slot |
| `tx_gap_ms` | 2 | Silence before transmitting |
| `inter_byte_timeout_ms` | 5 | Gap that aborts a frame |
| `cycle_period_ms` | 0 | Master: shortest time between cycle starts |
| `event_ttl_ms` | 5000 | Undelivered events are dropped after this; 0 = never |
| `rx_echo` | `true` | The UART receives its own bytes (single wire); `false` for RS-485 without echo |
| `write` | — | Starts a non-blocking transmission (required) |
| `enter_critical`, `exit_critical` | `NULL` | Both or neither; needed to call send/publish from other contexts |
| `on_message`, `on_drop`, `on_poll_done` | `NULL` | Callbacks, called from `baton_node_process()` |
| `context` | `NULL` | Passed to every hook and callback |

`baton_node_init()` returns `BATON_ERR_PARAM` for an inconsistent configuration: a
missing write function, a broadcast or duplicate address, `own_address` not on the bus
or not matching the role, a slot shorter than one maximum exchange, an ACK timeout not
longer than an ACK frame plus the gap. It can be called again to restart the node.

## The write function

```c
static int uart_write(void* context, const uint8_t* data, size_t length)
{
    (void) context;
    return board_uart_write(data, length); /* 0 = started, negative = failed */
}
```

The function starts the transmission and returns; the bytes go out from the TX
interrupt or a DMA channel. The buffer stays valid until the transmission ends: with
`rx_echo` the library sees its last byte come back; otherwise call
`baton_node_tx_done_isr()` from the TX-complete interrupt. A blocking write also works
(see the [host example](examples.md#host-serial-master)), but it delays the processing
of received bytes by the duration of the frame.

## Run the node

```c
void uart_rx_irq_handler(uint8_t byte)
{
    baton_node_rx_isr(&node, byte);
}

void main_loop(void)
{
    for (;;)
    {
        baton_node_process(&node, board_millis());
        board_wait_for_tick(); /* about 1 ms */
    }
}
```

Call `baton_node_process()` at least every millisecond; it never blocks. All timing —
gaps, timeouts, the slot budget, TTL — is measured with the `now_ms` you pass, a
monotonic millisecond counter that may wrap.

## Write a slave

A slave reacts to the messages it receives and publishes its own:

```c
static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    (void) context;
    (void) src;
    if ((msg_id == BATON_MSG_CMD_STOP) && (length == 1U))
    {
        motor_stop(data[0]);
    }
}

void report_temperature(int16_t centi_degrees)
{
    uint8_t value[2];

    value[0] = (uint8_t) ((uint16_t) centi_degrees & 0xFFU);
    value[1] = (uint8_t) ((uint16_t) centi_degrees >> 8);
    (void) baton_node_publish(&node, BATON_MSG_TEMPERATURE, value, 2U);
}
```

Rules of a well-behaved slave:

- Keep `on_message` short; it runs inside `baton_node_process()`. Queue longer work.
- Encode data with an explicit byte order (as above), not by copying structs: every node
  must agree on the layout.
- Expect a message more than once only if it is a state (a new value) — events arrive
  exactly once.

## Write a master

The master runs the schedule by itself; its application sends and receives like a
slave and may watch the slots:

```c
static void on_poll_done(void* context, uint8_t slave, bool responded, uint32_t nack_nodes)
{
    (void) context;
    if (!responded)
    {
        mark_offline(slave); /* no RESP in this slot */
    }
    if (nack_nodes != 0U)
    {
        log_unreachable(slave, nack_nodes); /* the slave could not reach these nodes */
    }
}

int command_start(uint8_t slave, uint16_t speed)
{
    uint8_t data[3];

    data[0] = (uint8_t) (speed & 0xFFU);
    data[1] = (uint8_t) (speed >> 8);
    data[2] = 1U; /* mode */
    return baton_node_send(&node, slave, BATON_MSG_CMD_START, data, 3U, BATON_PRIO_HIGH);
}
```

Set `config.role = BATON_ROLE_MASTER`, `own_address = master_address`, and
`on_poll_done`. `cycle_period_ms` spaces the cycles when the bus should not be busy all
the time.

## Events and states

| | `baton_node_send()` (event) | `baton_node_publish()` (state) |
| --- | --- | --- |
| Destination | One node or `BATON_ADDR_BROADCAST` | Every other node |
| Storage | Copied into the HIGH or NORMAL queue | Overwrites the mailbox |
| Can fail for lack of space | Yes: `BATON_ERR_FULL` | Never |
| Delivery | Exactly once, in order per source, destination and priority | The latest value; intermediate ones may be skipped |
| Undeliverable | Dropped after `event_ttl_ms`, `on_drop` | Sent again until acknowledged |

When `baton_node_send()` returns `BATON_ERR_FULL`, keep the event and try again later,
or drop it deliberately — the library never drops silently.

## Return values

| Code | Value | Returned when |
| --- | --- | --- |
| `BATON_OK` | 0 | Success |
| `BATON_ERR_PARAM` | −1 | NULL data with a non-zero length, invalid configuration |
| `BATON_ERR_NOT_INIT` | −2 | The node is NULL or not initialized |
| `BATON_ERR_MSG_ID` | −3 | Unknown id, or a state id passed to send / an event id to publish |
| `BATON_ERR_LEN` | −4 | Length above the `MAX_LEN` of the message |
| `BATON_ERR_ADDR` | −5 | Unknown destination, or the node itself |
| `BATON_ERR_FULL` | −6 | The event queue has no room |

## Diagnostics

`baton_node_get_stats()` copies the counters (see [Error handling](architecture.md#error-handling)):

```c
void print_link_health(void)
{
    struct baton_stats stats;

    baton_node_get_stats(&node, &stats);
    log_value("crc_errors", stats.crc_errors);     /* noise on the line */
    log_value("ack_timeouts", stats.ack_timeouts); /* a peer does not answer */
    log_value("queue_used", stats.queue_normal_used);
}
```

Rising `crc_errors` and `rx_timeouts` point to the physical layer; `collisions` should
stay 0 — a non-zero value means two nodes transmitted at once (wrong topology on one
node, two masters, or a node with another slave list).

## The codec without the node

The codec works on its own, for example in a bus monitor that prints every frame:

```c
static struct baton_parser monitor;

void monitor_byte(uint8_t byte)
{
    if (baton_parser_feed(&monitor, byte) == 1)
    {
        const struct baton_frame* f = &monitor.frame;

        print_frame(f->src, f->dst, baton_ctrl_type(f->ctrl), baton_ctrl_seq(f->ctrl),
                    f->payload, f->length);
    }
}
```

Call `baton_parser_init(&monitor)` once before the first byte.

## Common mistakes

| Symptom | Cause |
| --- | --- |
| `baton_node_init()` returns `BATON_ERR_PARAM` | A field of the configuration, see [Set up a node](#set-up-a-node); often `slot_time_ms` too short for the baud rate and payload |
| The master polls, slaves never answer | Different `slave_addresses` order or `master_address` on some node; the slave UART does not receive (wiring, idle level) |
| `tx_timeouts` grows | The write function does not start the UART, or `rx_echo` is `true` on a transceiver without echo |
| `collisions` grows | Two masters, or a node with another topology; on RS-485 the transceiver is switched to receive too late |
| `bad_messages` grows | The nodes are built with different message lists |
| `rx_overflows` grows | `baton_node_process()` is called too rarely for `BATON_RX_BUF_SIZE` |
| Events dropped (`on_drop`) | Destination offline, or `event_ttl_ms` shorter than a few cycles |
| `BATON_ERR_FULL` | Queues too small for the traffic during `event_ttl_ms`, or a destination offline |
