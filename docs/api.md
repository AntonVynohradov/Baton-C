# API

All declarations are in `Intercom.h`. No function blocks.

## Return codes: `Intercom_Status`

| Code | Value | Meaning |
| --- | --- | --- |
| `INTERCOM_OK` | 0 | Success |
| `INTERCOM_ERR_PARAM` | −1 | Invalid argument or inconsistent configuration |
| `INTERCOM_ERR_NOT_INIT` | −2 | No successful `Intercom_Init()` |
| `INTERCOM_ERR_MSG_ID` | −3 | Unknown identifier or wrong class (`Send` for a state, `Publish` for an event) |
| `INTERCOM_ERR_LEN` | −4 | `len` greater than the message's `MAX_LEN` |
| `INTERCOM_ERR_ADDR` | −5 | Unknown destination address or own address |
| `INTERCOM_ERR_FULL` | −6 | No room in the queue. Try again later; the library state is unchanged |

---

## Intercom_GetDefaultCfg

```c
void Intercom_GetDefaultCfg(IntercomCfg *cfg);
```

Zeroes the structure and sets defaults for 9600 bit/s: 250 ms slot, 20 ms ACK, 1 retry, 2 ms gap, 5 ms inter-byte timeout, 5 s TTL, `rxEcho = true`. The topology and hardware functions must be filled in.

## Intercom_Init

```c
Intercom_Status Intercom_Init(const IntercomCfg *cfg);
```

Validates the configuration, resets the library state (queues, channels, counters) and starts the protocol. The `cfg` structure is copied. Call it before enabling the UART RX interrupt. Calling it again restarts the library and discards the queue contents.

Returns `INTERCOM_ERR_PARAM` when:

- `getTimeMs`, `uartStartTx` or `slaveAddrs` is `NULL`, or only one of `enterCritical`/`exitCritical` is given,
- `slaveCount` is 0 or exceeds `INTERCOM_MAX_NODES − 1`; `baudrate`, `ackTimeoutMs` or `interByteTimeoutMs` is 0,
- addresses are duplicated or one of them equals `0xFF`,
- `ownAddr` does not match the role: a master must use `masterAddr`, a slave an address from `slaveAddrs`,
- message identifiers are duplicated,
- `ackTimeoutMs ≤ F(7) + txGapMs` or `slotTimeMs` is shorter than the minimum ([Protocol](protocol.md#minimum-slot-checked-in-intercom_init)),
- the largest event does not fit into one of the queues.

## Intercom_Process

```c
void Intercom_Process(void);
```

Does all of the protocol work: draining bytes from the RX buffer, parsing frames, state machines, timers, starting transmissions, releasing and expiring events, and callbacks. Call it often, ideally every ≤ 1 ms. Less frequent calls slow down reactions (gaps, ACK) and require a larger `INTERCOM_RX_BUF_SIZE`.

Execution time is bounded by buffer sizes: the number of bytes in RX, the number of records in the queues and the number of states. It does not depend on time spent waiting for the bus.

## Intercom_Send

```c
Intercom_Status Intercom_Send(uint8_t dstAddr, uint8_t msgId, const void *data,
                              uint8_t len, Intercom_Prio prio);
```

Copies the event into the `prio` queue (`INTERCOM_PRIO_HIGH` or `INTERCOM_PRIO_NORMAL`) and returns immediately.

- `dstAddr` is a node address or `INTERCOM_ADDR_BROADCAST`, meaning all other nodes including the master. A broadcast takes one record in the queue, but every recipient acknowledges it separately.
- `msgId` must come from `INTERCOM_EVENT_LIST`.
- `len` may be 0; `data` may then be `NULL`.

Guarantees: the event is delivered exactly once, and events with the same sender, recipient and priority keep their order. If the event is not acknowledged within `eventTtlMs`, it is removed and `onDrop` is called.

An event from a slave to the master travels in RESP, from the master to a slave in POLL, and between slaves in DATA during the sender's slot.

## Intercom_Publish

```c
Intercom_Status Intercom_Publish(uint8_t msgId, const void *data, uint8_t len);
```

Sets a new value for a state from `INTERCOM_STATE_LIST` and marks it for sending to all other nodes. Further calls before it is sent overwrite the value, so the latest one arrives. The function never returns `ERR_FULL`.

A state is sent only after a change, i.e. after `Publish`. If you want periodic refreshes, call `Publish` periodically.

## Intercom_RxByteIsr

```c
void Intercom_RxByteIsr(uint8_t byte);
```

Call it from the RX interrupt for every received byte, including the echo of your own transmission. It only stores the byte in a ring buffer (a few instructions, no locks).

## Intercom_TxDoneIsr

```c
void Intercom_TxDoneIsr(void);
```

Call it from the *transmission complete* interrupt, i.e. after the last stop bit has been sent. Required only with `rxEcho = false`, e.g. for RS-485 with the receiver disabled during transmission.

## Intercom_GetStats

```c
void Intercom_GetStats(Intercom_Stats *stats);
```

Copies the diagnostic counters:

| Field | Meaning |
| --- | --- |
| `txFrames`, `rxFrames` | Frames sent; valid frames seen on the bus |
| `crcErrors` | Bad CRC or invalid length |
| `rxTimeouts` | Frame aborted by an inter-byte gap |
| `rxOverflows` | Bytes lost due to a full RX buffer: enlarge the buffer or call `Process` more often |
| `collisions` | Echo differs from the transmitted bytes (noise or two transmitters) |
| `txTimeouts` | `uartStartTx` returned `false`, or no echo / TX-done arrived |
| `ackTimeouts`, `retries` | Missing ACK; DATA retransmissions |
| `respTimeouts` | Master: a slave did not answer a POLL |
| `duplicates` | Filtered retransmissions (content already delivered) |
| `badMessages` | Unknown ID or wrong message length in a valid frame |
| `eventsDropped` | Events removed after TTL |
| `queueFull` | Rejected `Send` calls |
| `queueHighUsed`, `queueNormalUsed` | Current queue usage in bytes |

## Callbacks

```c
void onMessage (uint8_t srcAddr, uint8_t msgId, const uint8_t *data, uint8_t len, void *ctx);
void onDrop    (uint8_t msgId, uint8_t dstAddr, uint32_t missedNodes, void *ctx);
void onPollDone(uint8_t slaveAddr, bool responded, uint32_t nackNodes, void *ctx);
```

- **`onMessage`** is called for every received message. `data` points into the receive buffer and is valid only during the call. Copy the data or forward it with `Send`, which copies it. The data may be unaligned, so use `memcpy` for structures.
- **`onDrop`** reports an event not acknowledged by the nodes in `missedNodes` (bit 0 = master, bit i = `slaveAddrs[i−1]`). A node may still have received it if only the ACK was lost.
- **`onPollDone`** works on the master only. `responded = false` means a RESP timeout. `nackNodes` are the nodes that did not acknowledge DATA from the polled slave in this slot.

Callbacks must not call `Intercom_Process` or `Intercom_Init`.
