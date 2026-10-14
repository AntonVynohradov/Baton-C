# Configuration

The configuration has two parts:

| Where | What | When |
| --- | --- | --- |
| `IntercomConfig.h` | Buffer sizes and the message list | Compile time (static memory) |
| `IntercomCfg` | Role, addresses, timings, hardware, callbacks | `Intercom_Init()` |

## IntercomConfig.h

Copy this file from `config/IntercomConfig_template.h` into your project. The library and the application must see the same file.

### Sizes

| Macro | Range | Template | Description |
| --- | --- | --- | --- |
| `INTERCOM_MAX_NODES` | 2..32 | 8 | Nodes on the bus including the master |
| `INTERCOM_MAX_PAYLOAD` | 16..250 | 64 | Maximum frame payload. Larger means less overhead but a longer minimum slot |
| `INTERCOM_MAX_MSGS_PER_FRAME` | 1..64 | 16 | Maximum number of messages per frame; each costs 2 B of RAM per node |
| `INTERCOM_QUEUE_HIGH_SIZE` | 64..65532 | 256 | HIGH event queue in bytes |
| `INTERCOM_QUEUE_NORMAL_SIZE` | 64..65532 | 512 | NORMAL event queue in bytes |
| `INTERCOM_RX_BUF_SIZE` | power of 2, 16..4096 | 128 | RX buffer between the ISR and `Process` |

Out-of-range values cause a compile error (`#error`).

**Queue size.** An event takes `16 + len` bytes rounded up to a multiple of 4. A queue should hold the events produced during `eventTtlMs` (when a recipient is unreachable) and while waiting for the node's own slot.

**RX buffer.** It must hold the bytes arriving between calls to `Intercom_Process()`. At 9600 bit/s that is about 1 byte per ms, so 128 B is enough for gaps up to ~100 ms. Overflows are counted in `rxOverflows`.

### Message list

```c
#define INTERCOM_STATE_LIST(X)      \
    X(NAME, ID, MAX_LEN)            \
    ...
#define INTERCOM_EVENT_LIST(X)      \
    X(NAME, ID, MAX_LEN)            \
    ...
```

| Parameter | Requirement |
| --- | --- |
| `NAME` | A C identifier. Generates `INTERCOM_MSG_NAME` and `INTERCOM_MSG_NAME_MAX_LEN` |
| `ID` | 0..255, unique across both lists (checked in `Intercom_Init`), the same on all nodes |
| `MAX_LEN` | 1..`INTERCOM_MAX_PAYLOAD − 3 − (INTERCOM_MAX_NODES+7)/8`; 1..60 for the template (checked at compile time) |

- **STATE**: a periodic or current value (measurement, status, setpoint). It is a mailbox, cannot overflow, and the latest value arrives.
- **EVENT**: a command, an alarm, an operation acknowledgement. It goes into a queue and is delivered exactly once and in order.

Either list may be empty (`#define INTERCOM_STATE_LIST(X)`).

**Shared payload types.** You can define the structures carried in messages in the same file. Use fixed-width types and a fixed layout, because nodes may have different architectures: byte order, alignment. Check `sizeof(T) <= INTERCOM_MSG_NAME_MAX_LEN`, e.g.:

```c
_Static_assert(sizeof(IntercomCmdStart) <= INTERCOM_MSG_CMD_START_MAX_LEN, "CMD_START too big");
```

## IntercomCfg

`Intercom_GetDefaultCfg()` sets the default values. The topology and hardware functions must be filled in by you.

### Topology

| Field | Description |
| --- | --- |
| `role` | `INTERCOM_ROLE_MASTER` or `INTERCOM_ROLE_SLAVE` |
| `ownAddr` | This node's address. Master: equal to `masterAddr`; slave: one of `slaveAddrs` |
| `masterAddr` | The master's address |
| `slaveAddrs`, `slaveCount` | List of slaves in polling order, **identical on every node**. 1..`INTERCOM_MAX_NODES−1` entries; addresses unique and different from `0xFF` |

Node index in masks (`missedNodes`, `nackNodes`): 0 = master, i = `slaveAddrs[i−1]`.

### Timings

| Field | Default | Description |
| --- | --- | --- |
| `baudrate` | 9600 | Used only to compute frame times; you configure the UART yourself |
| `slotTimeMs` | 250 | How long a slave owns the bus after a POLL. Minimum: [Protocol](protocol.md#minimum-slot-checked-in-intercom_init) |
| `ackTimeoutMs` | 20 | Wait for ACK after DATA. Must be > `F(7) + txGapMs` |
| `maxRetries` | 1 | DATA retransmissions within one slot. Unacknowledged content waits for the next slot |
| `txGapMs` | 2 | Silence before transmitting (turnaround) |
| `interByteTimeoutMs` | 5 | A gap that aborts the frame being received (≈ 5 bytes at 9600) |
| `cyclePeriodMs` | 0 | Master: minimum interval between cycle starts; 0 = back-to-back cycles |
| `eventTtlMs` | 5000 | After this time an undelivered event goes to `onDrop`; 0 = never |

**Choosing values for other baud rates.** Frame times scale as `1/baud`. At 115200 bit/s a 71 B frame takes ~7 ms, so e.g. `slotTimeMs = 40`, `ackTimeoutMs = 5`, `txGapMs = 1`, `interByteTimeoutMs = 2` are enough. Time resolution is 1 ms, so do not go below 1 ms gaps.

### UART and hardware

| Field | Required | Description |
| --- | --- | --- |
| `rxEcho` | – | `true`: the UART receives its own bytes (single-wire). `false`: no echo, call `Intercom_TxDoneIsr()` |
| `getTimeMs` | yes | Monotonic ms counter (`HAL_GetTick`). Must be ISR-safe if `Send`/`Publish` are called from interrupts |
| `uartStartTx` | yes | Starts a non-blocking transmission (IT/DMA). The buffer stays valid until TX ends. Returns `false` when the UART is busy |
| `enterCritical`, `exitCritical` | both or neither | Needed when `Send`/`Publish` are called from a context other than `Process` |
| `ctx` | – | Passed to all functions and callbacks |

### Callbacks

| Field | Description |
| --- | --- |
| `onMessage(src, id, data, len, ctx)` | A received message (state or event); `data` is valid only during the call |
| `onDrop(id, dst, missedNodes, ctx)` | An event not acknowledged within `eventTtlMs` by the nodes in the mask |
| `onPollDone(slave, responded, nackNodes, ctx)` | Master only: end of a slave's slot |

Callbacks are called from `Intercom_Process()`. They may call `Intercom_Send`/`Intercom_Publish`, but not `Intercom_Process` or `Intercom_Init`.
