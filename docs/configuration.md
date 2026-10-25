# Configuration

This chapter lists the build-time parameters with their ranges, the derived sizes, the
measured memory footprint, code size and stack usage, how to measure them again, and the
CMake options. The runtime parameters are described in
[Using the Library → Set up a node](usage.md#set-up-a-node).

## Build-time parameters

The application defines them in `baton_user_config.h` (or with `-D` compiler options);
`inc/baton_config.h` provides the defaults and rejects values out of range with `#error`.

| Macro | Default | Range | Meaning |
| --- | --- | --- | --- |
| `BATON_MAX_NODES` | 8 | 2..32 | Nodes on the bus including the master |
| `BATON_MAX_PAYLOAD` | 64 | 16..250 | Largest frame payload in bytes |
| `BATON_MAX_MSGS_PER_FRAME` | 16 | 1..64 | Messages packed into one frame |
| `BATON_QUEUE_HIGH_SIZE` | 256 | 64..65532 | HIGH event queue in bytes |
| `BATON_QUEUE_NORMAL_SIZE` | 512 | 64..65532 | NORMAL event queue in bytes |
| `BATON_RX_BUF_SIZE` | 128 | 16..4096, power of two | Receive ring filled by the interrupt |
| `BATON_STATE_LIST(X)` | — | required | STATE messages `X(NAME, ID, MAX_LEN)` |
| `BATON_EVENT_LIST(X)` | — | required | EVENT messages `X(NAME, ID, MAX_LEN)` |

## Derived sizes

| Macro | Value | Meaning |
| --- | --- | --- |
| `BATON_FRAME_MAX` | `7 + BATON_MAX_PAYLOAD` | Largest frame on the wire |
| `BATON_MASK_BYTES` | `(BATON_MAX_NODES + 7) / 8` | NACK mask in a RESP |
| `BATON_RESP_PREFIX` | `1 + BATON_MASK_BYTES` | ACKB and NACK |
| `BATON_MAX_MSG_LEN` | `BATON_MAX_PAYLOAD − BATON_RESP_PREFIX − 2` | Largest `MAX_LEN` of a message |
| `BATON_STATE_COUNT`, `BATON_EVENT_COUNT` | from the lists | Number of messages |
| `BATON_STATE_DATA_SIZE` | `1 + Σ MAX_LEN` of the states | State mailboxes |

Sizing hints: an event occupies `16 + MAX(len rounded up to 4)` bytes of its queue until
every destination acknowledged it, at most for `event_ttl_ms`. The receive ring must hold
the bytes that arrive between two calls of `baton_node_process()` plus the echo of a
maximum frame (`BATON_FRAME_MAX`).

## Memory footprint

All memory of a node is the caller's `struct baton_node`; the library has no global or
static variables except constant tables. Measured with `sizeof` for Arm Cortex-M4
(`arm-none-eabi-gcc` 15.2.1):

| Configuration | `struct baton_node` | of which queues | of which link (ring, parser, counters) |
| --- | --- | --- | --- |
| Template (8 nodes, payload 64, 16 msgs, queues 256 + 512, ring 128) | 1696 B | 768 B | 280 B |
| Unit tests (as template, ring 256) | 1824 B | 768 B | 408 B |
| Examples (4 nodes, payload 64, 8 msgs, queues 128 + 256, ring 128) | 1068 B | 384 B | 280 B |

Approximately: `BATON_RX_BUF_SIZE + BATON_QUEUE_HIGH_SIZE + BATON_QUEUE_NORMAL_SIZE +
2 × BATON_FRAME_MAX + 7 + BATON_MAX_NODES × (2 × BATON_MAX_MSGS_PER_FRAME + 10) +
state mailboxes + about 300 B` of configuration copy, counters and state variables.

## Code size

Library code (`text`) for Cortex-M4 Thumb-2, compiled with the configuration template,
`-ffunction-sections`, without the C library functions `memcpy`, `memset` and `memmove`
that the library calls:

| File | `-Os` | `-O0` |
| --- | ---: | ---: |
| `baton_protocol.c` | 932 B | 1070 B |
| `baton_link.c` | 602 B | 1376 B |
| `baton_queue.c` | 456 B | 1194 B |
| `baton_node.c` | 4630 B | 9094 B |
| **Total** | **6620 B** | **12734 B** |

The library has no initialized or zero-initialized data of its own (`data` and `bss` are 0).

## Stack usage

Worst-case stack of the entry points, from GCC `-fstack-usage -fcallgraph-info=su`,
following every direct call. Not included: the application functions the library calls
through pointers (`write`, `enter_critical`, `exit_critical`, `on_message`, `on_drop`,
`on_poll_done`) — add their own stack to the figure of the function that calls them —
and `memcpy`/`memset`/`memmove`.

| Function | Cortex-M4 `-Os` | Cortex-M4 `-O0` | x86-64 `-O2` |
| --- | ---: | ---: | ---: |
| `baton_node_process()` | 280 B | 400 B | 752 B |
| `baton_node_init()` | 80 B | 160 B | 216 B |
| `baton_node_send()` | 80 B | 112 B | 136 B |
| `baton_node_publish()` | 24 B | 64 B | 80 B |
| `baton_node_get_stats()` | 24 B | 56 B | 72 B |
| `baton_node_rx_isr()` | 8 B | 40 B | 16 B |
| `baton_node_tx_done_isr()` | 0 B | 32 B | 16 B |
| `baton_parser_feed()` | 24 B | 72 B | 16 B |
| `baton_encode()` | 48 B | 80 B | 80 B |

Toolchains: Arm GNU Toolchain 15.2.Rel1 (`arm-none-eabi-gcc` 15.2.1,
`-mcpu=cortex-m4 -mthumb`), MinGW-w64 GCC 14.2.0 (x86-64 Windows ABI, which reserves
32 bytes of shadow space per call).

Worst-case chains:

- `baton_node_process()` (`-Os`): process → slave slot step → DATA frame → pack →
  state copy → TLV emit, on the reception of a POLL.
- `baton_node_process()` (`-O0`): process → frame dispatch → POLL handler → slot step →
  RESP → pack → retransmission → state copy → TLV emit.

An interrupt that calls `baton_node_rx_isr()` adds 8 B (Cortex-M4 `-Os`) to the stack of
whatever it interrupts.

### How to measure again

```bash
mkdir -p build/stack
for f in src/*.c; do
  arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -Os -std=c11 -fstack-usage -fcallgraph-info=su \
    -I. -Iinc -I<folder of baton_user_config.h> -c "$f" -o "build/stack/$(basename "$f" .c).o" \
    -dumpdir build/stack/
done
python tools/docs/stack_usage.py build/stack
arm-none-eabi-size build/stack/*.o
```

Measure again after any change in `src/` that may change a frame, and update this page.

## CMake options

See [Getting Started → Build with presets](guide.md#build-with-presets).
