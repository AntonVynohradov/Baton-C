# Introduction

Baton-C is a C11 library for deterministic master/slave messaging between a few
microcontrollers on one shared UART line (single-wire half duplex, open-drain or RS-485,
9600 bit/s by default). The master passes a token — the *baton* — to each slave in turn;
only the node that holds it transmits. On top of this collision-free schedule the library
delivers application messages of two kinds: **events**, delivered exactly once and in
order, and **states**, of which the latest value always arrives. It is written for
embedded systems that must be predictable and reviewable: no heap, no RTOS, no clock
reads, MISRA C:2012 without violations and 100 % line and branch test coverage.

## What the library does

- **Schedules the bus.** The master polls the slaves in a fixed order. A polled slave may
  send DATA frames to other slaves and must end its slot with a RESP within
  `slot_time_ms`. The worst-case cycle depends only on the configuration, not on the
  traffic.
- **Delivers events reliably.** Events are queued (HIGH and NORMAL priority), packed
  several per frame, acknowledged, retransmitted with the same sequence number,
  filtered for duplicates, and dropped after `event_ttl_ms` with a report of the nodes
  that did not acknowledge them.
- **Delivers states.** A state is a mailbox: publishing overwrites it and marks it for
  every other node. It cannot overflow; intermediate values may be skipped.
- **Handles an unreliable bus.** CRC-16 per frame, inter-byte timeout, echo check of the
  own transmission, retries, and a schedule that keeps unreachable nodes from eating
  the time of the healthy ones.
- **Encodes and parses frames.** The codec can be used on its own, for example in a bus
  monitor.

## What the library does not do

| Concern | Why it is not in the library |
| --- | --- |
| UART driver, pin setup, transceiver direction | Hardware-specific; you pass received bytes in and provide a non-blocking write function. |
| Threads, interrupt priorities, locking | Every system has its own; the library offers two optional critical-section hooks. See the [integration guide](integration.md). |
| A clock | Every call that needs the time takes `now_ms`; the library never reads a timer. |
| Message meaning, data layouts, units | Application data; the library transports byte strings up to their `MAX_LEN`. |
| Arbitration between several masters | One master per bus is the design. |
| Broadcast frames | A broadcast event is sent to each node in its own frame and acknowledged by each one (see [design decisions](architecture.md#design-decisions)). |

## Who it is for

- **Small multi-MCU products** — a control board and a few sensor, actuator or display
  boards connected by one wire, where CAN is too expensive and a plain UART protocol has
  no delivery guarantee.
- **Safety- or certification-minded firmware** that needs bounded execution time, static
  memory and a documented MISRA C:2012 status.
- **Host tools** — a PC or a Linux board can be the master or a bus monitor through a
  USB-UART adapter (see the [examples](examples.md)).

## Key properties

| Property | Value |
| --- | --- |
| Language | C11, no warnings with `-Wall -Wextra -Wpedantic -Wconversion` |
| Dependencies | `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`, `<string.h>` only |
| Memory | No heap, no global variables; all state in a caller-owned `struct baton_node` |
| RAM per node | 1696 B with the configuration template (8 nodes, 64-byte payload, queues 256 + 512 B), [details](configuration.md#memory-footprint) |
| Code size | 6620 B on Cortex-M4 with `-Os` (GCC 15.2), without `memcpy`/`memset`/`memmove` |
| Stack | 280 B worst case in `baton_node_process()` on Cortex-M4 `-Os`, [details](configuration.md#stack-usage) |
| Execution time | Bounded by the configured sizes; no function blocks or waits |
| Coding standard | MISRA C:2012, 0 Cppcheck addon violations, [4 deviations](misra.md) |
| Tests | 100 % line and branch coverage; bus simulation with noise and dead nodes; C/Python cross-check |
| Thread safety | One processing context plus interrupts; see [Threading](architecture.md#threading-and-reentrancy) |
| License | [Apache License 2.0](LICENSE.md) |

## Terminology

Master
:   The node that polls the slaves; node index 0. Exactly one per bus.

Slave
:   A node that transmits only while it holds the baton; node index 1..n in the order of
    `slave_addresses`, which must be identical on every node.

Slot
:   The time between a POLL to a slave and its RESP, at most `slot_time_ms`.

Cycle
:   One round of POLLs over all slaves.

Frame
:   One unit on the wire: `SYNC DST SRC CTRL LEN PAYLOAD CRC`. Types: POLL, RESP, DATA, ACK.

Message
:   One application record (id, length, data) inside a frame payload (TLV). Several
    messages share one frame and one acknowledgement.

Event
:   A message class: queued, acknowledged, delivered exactly once, in order.

State
:   A message class: a mailbox whose latest value is delivered to every other node.

Channel
:   The stop-and-wait state between two nodes in one direction: a 4-bit sequence number,
    the FIRST flag and the list of messages in the unacknowledged frame.

Node mask
:   A 32-bit set of node indexes; bit 0 is the master, bit i is `slave_addresses[i - 1]`.

## Conventions in this documentation

- Hexadecimal bytes without prefix inside frames (`A5 11 01`), with `0x` in text and code.
- The CRC is big endian on the wire; node masks inside a RESP are little endian.
- Sizes are shown with the values of the configuration template
  (`config/baton_user_config_template.h`), timing with the defaults of
  `baton_node_default_config()` at 9600 bit/s.
