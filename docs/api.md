# API Reference

This chapter lists every public header, macro, type and function. Applications normally
include `baton.h` and use the [node API](#node-api); the codec, the link and the queue
are public for tools and tests. The generated Doxygen reference is in the *Doxygen*
section of the website.

## Headers

| Header | Contents |
| --- | --- |
| `baton.h` | Umbrella: includes everything below; version macros |
| `inc/baton_config.h` | Sizes and range checks; includes the application's `baton_user_config.h` |
| `inc/baton_defs.h` | Frame layout, frame types, ACKB, CTRL and byte-order helpers |
| `inc/baton_protocol.h` | CRC-16, parser, encoder |
| `inc/baton_link.h` | Half-duplex link |
| `inc/baton_queue.h` | Event queue |
| `inc/baton_node.h` | Node: roles, return codes, configuration, statistics, application API |

## Version

| Macro | Value | Meaning |
| --- | --- | --- |
| `BATON_VERSION_MAJOR`, `BATON_VERSION_MINOR`, `BATON_VERSION_PATCH` | `1`, `0`, `0` | From the Git tag through CMake; fallbacks in `baton.h` |
| `BATON_VERSION` | `"1.0.0"` | Version string |

## Configuration macros

See [Configuration](configuration.md#build-time-parameters) for `BATON_MAX_NODES`,
`BATON_MAX_PAYLOAD`, `BATON_MAX_MSGS_PER_FRAME`, `BATON_QUEUE_HIGH_SIZE`,
`BATON_QUEUE_NORMAL_SIZE`, `BATON_RX_BUF_SIZE`, the message lists and the derived sizes.

Generated from the lists: `BATON_MSG_<NAME>` (enum `baton_msg_id`),
`BATON_MSG_<NAME>_MAX_LEN` (enum `baton_msg_max_len`), `BATON_STATE_COUNT`,
`BATON_EVENT_COUNT`, `BATON_STATE_DATA_SIZE`.

## Protocol constants (`baton_defs.h`)

| Macro | Value | Meaning |
| --- | --- | --- |
| `BATON_SYNC` | `0xA5` | First byte of a frame |
| `BATON_HEADER_SIZE` | 5 | SYNC, DST, SRC, CTRL, LEN |
| `BATON_CRC_SIZE` | 2 | CRC bytes |
| `BATON_FRAME_OVERHEAD` | 7 | Non-payload bytes |
| `BATON_ADDR_BROADCAST` | `0xFF` | "Every other node" for `baton_node_send()` |
| `BATON_FT_POLL`, `BATON_FT_RESP`, `BATON_FT_DATA`, `BATON_FT_ACK` | 0, 1, 2, 3 | Frame types |
| `BATON_ACKB_VALID` | `0x80` | Valid bit of ACKB |
| `BATON_SEQ_MASK` | `0x0F` | Sequence number bits |
| `BATON_TLV_HEADER_SIZE` | 2 | ID and LEN of a message |

| Function | Description |
| --- | --- |
| `uint8_t baton_ctrl(uint8_t type, bool first, uint8_t seq)` | Build a control byte |
| `uint8_t baton_ctrl_type(uint8_t ctrl)` | Frame type of a control byte |
| `bool baton_ctrl_first(uint8_t ctrl)` | FIRST flag |
| `uint8_t baton_ctrl_seq(uint8_t ctrl)` | Sequence number |
| `uint16_t baton_get_u16be(const uint8_t* data)` | Read big endian |
| `void baton_put_u16be(uint8_t* data, uint16_t value)` | Write big endian |
| `uint32_t baton_get_u32le(const uint8_t* data)` | Read little endian |
| `void baton_put_u32le(uint8_t* data, uint32_t value)` | Write little endian |

These are `static inline`; they do not check their pointer (like `memcpy()`).

## Node API

### Return codes

| Macro | Value |
| --- | --- |
| `BATON_OK` | 0 |
| `BATON_ERR_PARAM` | −1 |
| `BATON_ERR_NOT_INIT` | −2 |
| `BATON_ERR_MSG_ID` | −3 |
| `BATON_ERR_LEN` | −4 |
| `BATON_ERR_ADDR` | −5 |
| `BATON_ERR_FULL` | −6 |

### Roles, priorities, limits

| Macro | Value | Meaning |
| --- | --- | --- |
| `BATON_ROLE_MASTER`, `BATON_ROLE_SLAVE` | 0, 1 | `baton_node_config::role` |
| `BATON_PRIO_NORMAL`, `BATON_PRIO_HIGH` | 0, 1 | Queue of an event |
| `BATON_MASK_BYTES` | `(BATON_MAX_NODES + 7) / 8` | NACK mask size |
| `BATON_POLL_PREFIX`, `BATON_RESP_PREFIX` | 1, `1 + BATON_MASK_BYTES` | Payload prefixes |
| `BATON_MAX_MSG_LEN` | `BATON_MAX_PAYLOAD − BATON_RESP_PREFIX − 2` | Largest message |
| `BATON_DEAD_STREAK` | 3 | Failures before a node is treated as unreachable |
| `BATON_DEAD_POLL_DIV` | 4 | An unreachable slave is polled every 4th cycle |

### Callback types

| Type | Signature | Called |
| --- | --- | --- |
| `baton_write_fn` | `int (*)(void* context, const uint8_t* data, size_t length)` | To start a transmission; return 0 or negative |
| `baton_critical_fn` | `void (*)(void* context)` | Around the few instructions shared with interrupts |
| `baton_message_fn` | `void (*)(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data, uint8_t length)` | Once per received message; `data` valid only during the call |
| `baton_drop_fn` | `void (*)(void* context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes)` | When an event expired |
| `baton_poll_done_fn` | `void (*)(void* context, uint8_t slave, bool responded, uint32_t nack_nodes)` | Master: when a slot ended |

### `struct baton_node_config`

Described field by field in [Using the Library → Set up a node](usage.md#set-up-a-node).
`baton_node_init()` copies it.

### `struct baton_stats`

| Field | Meaning |
| --- | --- |
| `tx_frames` | Transmissions started |
| `rx_frames` | Valid frames seen on the bus, any destination |
| `crc_errors` | Frames rejected: CRC mismatch or invalid LEN |
| `rx_timeouts` | Frames aborted by the inter-byte timeout |
| `rx_overflows` | Bytes lost because the receive ring was full |
| `collisions` | Transmissions whose echo did not match |
| `tx_timeouts` | Transmissions that failed to start or did not end in time |
| `ack_timeouts` | Slave: DATA frames without ACK |
| `retries` | Slave: DATA retransmissions |
| `resp_timeouts` | Master: POLLs without RESP |
| `duplicates` | Retransmitted content filtered out |
| `bad_messages` | Unknown ids or bad lengths inside frames |
| `events_dropped` | Events removed by the TTL |
| `queue_full` | Events rejected by `baton_node_send()` |
| `queue_high_used`, `queue_normal_used` | Bytes in use in each queue |

### `struct baton_node`

The node state, owned by the caller (normally a `static` variable). Treat its members as
private; their layout may change in a minor version.

### `baton_node_default_config()`

```text
void baton_node_default_config(struct baton_node_config* config);
```

Fills `config` with zeros and the defaults for 9600 bit/s (slave, slot 250 ms, ACK
timeout 20 ms, 1 retry, gap 2 ms, inter-byte timeout 5 ms, TTL 5000 ms, echo on). NULL
is ignored.

### `baton_node_init()`

```text
int baton_node_init(struct baton_node* node, const struct baton_node_config* config,
                    uint32_t now_ms);
```

| Parameter | Description |
| --- | --- |
| `node` | Node state; cleared and initialized |
| `config` | Configuration; copied |
| `now_ms` | Current time |

| Return | When |
| --- | --- |
| `BATON_OK` | The node is running |
| `BATON_ERR_PARAM` | NULL argument or invalid configuration; the node is not initialized |

### `baton_node_process()`

```text
void baton_node_process(struct baton_node* node, uint32_t now_ms);
```

Runs the receive path, the transmitter, the state machine and the housekeeping (TTL,
queue release). Call every millisecond from one context. Ignored for a NULL or
uninitialized node.

### `baton_node_send()`

```text
int baton_node_send(struct baton_node* node, uint8_t dst, uint8_t msg_id, const void* data,
                    uint8_t length, uint8_t priority);
```

| Parameter | Description |
| --- | --- |
| `dst` | Destination address or `BATON_ADDR_BROADCAST` |
| `msg_id` | An EVENT id |
| `data`, `length` | Data, copied; `data` may be NULL when `length` is 0 |
| `priority` | `BATON_PRIO_NORMAL` or `BATON_PRIO_HIGH` |

| Return | When |
| --- | --- |
| `BATON_OK` | Queued |
| `BATON_ERR_NOT_INIT` | NULL or uninitialized node |
| `BATON_ERR_PARAM` | `data` NULL with `length` > 0 |
| `BATON_ERR_MSG_ID` | Not an EVENT id |
| `BATON_ERR_LEN` | `length` above `MAX_LEN` |
| `BATON_ERR_ADDR` | Unknown destination or the node itself |
| `BATON_ERR_FULL` | No room in the queue |

The TTL of the event starts at the time of the last `baton_node_process()` call.

### `baton_node_publish()`

```text
int baton_node_publish(struct baton_node* node, uint8_t msg_id, const void* data,
                       uint8_t length);
```

Stores the new value of a STATE and marks it for every other node. Returns `BATON_OK`,
`BATON_ERR_NOT_INIT`, `BATON_ERR_PARAM`, `BATON_ERR_MSG_ID` (not a STATE id) or
`BATON_ERR_LEN`; never fails for lack of space.

### `baton_node_rx_isr()`, `baton_node_tx_done_isr()`

```text
void baton_node_rx_isr(struct baton_node* node, uint8_t byte);
void baton_node_tx_done_isr(struct baton_node* node);
```

Interrupt entry points: store a received byte (counted in `rx_overflows` when the ring
is full); signal the end of a transmission when `rx_echo` is false. NULL is ignored.

### `baton_node_get_stats()`

```text
void baton_node_get_stats(const struct baton_node* node, struct baton_stats* stats);
```

Copies the counters; nothing is written when an argument is NULL.

## Codec (`baton_protocol.h`)

| Function | Returns |
| --- | --- |
| `uint16_t baton_crc16_update(uint16_t crc, uint8_t byte)` | Updated CRC register |
| `uint16_t baton_crc16(const uint8_t* data, size_t length)` | CRC of the buffer; `BATON_CRC_INIT` (0xFFFF) for NULL |
| `void baton_parser_init(struct baton_parser* parser)` | — (NULL ignored) |
| `int baton_parser_feed(struct baton_parser* parser, uint8_t byte)` | 1 frame ready in `parser->frame`; 0 more bytes needed; `BATON_PARSE_ERROR` (−1) invalid LEN, CRC mismatch, corrupted state or NULL |
| `int baton_encode(uint8_t dst, uint8_t src, uint8_t ctrl, const uint8_t* payload, size_t payload_length, uint8_t* output, size_t output_capacity, size_t* output_length)` | 0, or `BATON_PARSE_ERROR` for invalid input or too little capacity |

`struct baton_frame` has `dst`, `src`, `ctrl`, `length` and `payload[BATON_MAX_PAYLOAD]`.
`struct baton_parser` holds the frame being received, the CRC register, an index and the
state (`BATON_PARSER_IDLE` between frames). `BATON_FRAME_MAX` is the largest frame.

## Link (`baton_link.h`)

| Function | Returns |
| --- | --- |
| `int baton_link_init(struct baton_link*, const struct baton_link_config*, uint32_t now_ms)` | 0, or `BATON_PARSE_ERROR` for NULL, no write function, zero baud rate or timeout |
| `void baton_link_rx_isr(struct baton_link*, uint8_t byte)` | — |
| `void baton_link_tx_done_isr(struct baton_link*)` | — |
| `int baton_link_receive(struct baton_link*, uint32_t now_ms)` | `BATON_LINK_FRAME`, `BATON_LINK_TX_DONE` (echo complete) or `BATON_LINK_NONE` |
| `int baton_link_transmit(struct baton_link*, uint32_t now_ms)` | `BATON_LINK_TX_DONE` or `BATON_LINK_NONE` |
| `int baton_link_send(struct baton_link*, const uint8_t* data, size_t length)` | 0, or `BATON_PARSE_ERROR` |
| `void baton_link_cancel(struct baton_link*)` | — |
| `const struct baton_frame* baton_link_frame(const struct baton_link*)` | Last frame reported |
| `bool baton_link_rx_busy(const struct baton_link*, uint32_t now_ms)` | A frame is being received |
| `uint32_t baton_link_frame_ms(const struct baton_link*, uint32_t bytes)` | Wire time of `bytes` in ms, rounded up |

`struct baton_link_config`: `write`, `context`, `baudrate`, `tx_gap_ms`,
`inter_byte_timeout_ms`, `rx_echo`. `struct baton_link_counters`: `tx_frames`,
`rx_frames`, `crc_errors`, `rx_timeouts`, `collisions`, `tx_timeouts`.

## Event queue (`baton_queue.h`)

| Function | Description |
| --- | --- |
| `baton_queue_init(queue, memory, size)` | Empty queue over `memory`; size rounded down to 4 |
| `baton_queue_record_size(length)` | 16 + length rounded up to 4 |
| `baton_queue_alloc(queue, need, &offset)` | Reserve a record at the tail (with the critical section held) |
| `baton_queue_first(queue, head, tail)`, `baton_queue_next(queue, offset, tail)` | Walk the records of a head/tail snapshot |
| `baton_queue_used(queue)` | Bytes in use |
| `baton_queue_read`, `baton_queue_write`, `baton_queue_set_masks`, `baton_queue_set_flags` | Access the serialized header (`struct baton_record`) |
| `baton_queue_data(queue, offset)` | Data bytes of a record |

Every queue function checks its pointers; offsets come from the queue itself.
