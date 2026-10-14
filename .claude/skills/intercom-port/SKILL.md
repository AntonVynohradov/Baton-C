---
name: intercom-port
description: Integrate the intercom library into an application on a specific MCU — UART single-wire or RS-485 hookup, ISR glue, critical sections, RTOS task, timing for the baud rate, config file for the application. Use when the user wants to port, wire up, or bring up intercom on hardware or in a firmware project.
---

# Porting and integrating intercom on an MCU

Port code goes into the **application project**, not into the submodule. The library stays free of hardware dependencies.

## 1. Clarify with the user

Ask about anything you cannot see in the application project in a single set of questions:

- MCU and driver layer (STM32 HAL/LL, ESP-IDF, nRF, AVR, bare metal),
- line mode:
  - single-wire half-duplex with echo → `rxEcho = true`,
  - RS-485 or a mode without echo → `rxEcho = false` + `Intercom_TxDoneIsr()`,
- baud rate (default 9600) and number of nodes, this node's role, addresses,
- RTOS or superloop; whether `Intercom_Send`/`Publish` will be called from ISRs or other tasks (then a critical section is needed),
- how fast the main loop spins. `Intercom_Process` should be called every ≤ 1 ms.

## 2. Files in the application project

```
<app>/Config/IntercomConfig.h        copy of config/IntercomConfig_template.h (shared message list!)
<app>/Port/intercom_port.c           hooks + ISR + init
<app>/Port/intercom_port.h           void intercom_port_init(void); (optional)
```

- `IntercomConfig.h` must be identical on all nodes (message IDs and MAX_LEN). Suggest keeping it in a shared location.
- Include paths: `libs/intercom/include` and `<app>/Config`. Sources: `libs/intercom/src/*.c`. In CMake, `INTERCOM_CONFIG_DIR` + `add_subdirectory` is enough (see `docs/getting-started.md`).

## 3. Hooks: requirements

| Hook | Requirement |
| --- | --- |
| `getTimeMs` | Monotonic ms counter; `uint32_t` wrap-around is handled. Must be ISR-safe if `Send` is called from an ISR |
| `uartStartTx(data, len)` | Start IT/DMA and **return immediately**. The library buffer is valid until TX ends. `false` when the UART is busy |
| RX ISR | `Intercom_RxByteIsr(b)` for every byte, including the echo. Clear the error flags (ORE/FE/NE), otherwise RX stalls |
| TX done (`rxEcho = false`) | `Intercom_TxDoneIsr()` from the **TC** (transmission complete) interrupt, not TXE. For RS-485 this is where you disable DE |
| `enterCritical`/`exitCritical` | Both or neither. The library does not nest calls; save and restore PRIMASK or the RTOS state |

Ready-made patterns for STM32 single-wire, RS-485, critical sections and FreeRTOS are in `docs/porting.md`. Use them as a base and adapt them to the specific family or SDK.

STM32 details:
- on some families half-duplex disables the receiver during TX, so there is no echo. Use `rxEcho = false` then,
- reading `RDR` clears `RXNE`; do not run `HAL_UART_Receive_IT` in parallel,
- enable RX only after `Intercom_Init()`.

## 4. Timings for the baud rate

Frame times scale as `1/baud`. `Intercom_Init` rejects a slot that is too short. The formulas are in `docs/protocol.md` (Time budget section).

| baud | slotTimeMs | ackTimeoutMs | txGapMs | interByteTimeoutMs |
| --- | --- | --- | --- | --- |
| 9600 | 250 | 20 | 2 | 5 |
| 19200 | 130 | 12 | 2 | 3 |
| 115200 | 40 | 5 | 1 | 2 |

These values assume `INTERCOM_MAX_PAYLOAD = 64`.

- If the main loop latency is greater than ~1 ms, add it to `ackTimeoutMs` and `txGapMs`.
- Increase `INTERCOM_RX_BUF_SIZE` by about `baud/10000` bytes for every ms of gap between `Process` calls.

## 5. 8/16-bit MCUs

The library has not been compiled with a 16-bit `int`. When porting to AVR or MSP430:

- compile with `-Wall -Wextra -Wconversion` and fix warnings in the library following the `intercom-core-change` skill,
- `ic_mask_t` and `stDirty` are 32-bit, and reading them is not atomic. If `Publish` is called from an ISR, a critical section is **mandatory**.

## 6. Bring-up checklist

- [ ] `Intercom_Init()` == `INTERCOM_OK` on every node (otherwise check the list in `docs/api.md#intercom_init`)
- [ ] Master: `onPollDone(slave, responded=true, ...)` for every slave
- [ ] `Intercom_GetStats()` after a minute of running on a clean bus:
  - `crcErrors`, `rxOverflows`, `collisions`, `txTimeouts`, `badMessages` ≈ 0,
  - `txTimeouts` > 0 and no RESP → wrong `rxEcho` or TX-done is not being called,
  - `collisions` > 0 → the echo comes back distorted (pull-up, open-drain, levels) or someone transmits outside their slot,
  - `rxOverflows` > 0 → `Process` called too rarely or RX buffer too small
- [ ] Exchange test: `Intercom_Send` from node A to B and receipt in `onMessage`; `Publish` a state and receipt on all nodes
- [ ] Disconnect one slave: the others keep working, and `onDrop` reports events for the disconnected node after `eventTtlMs`
