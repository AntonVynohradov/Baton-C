# Integration Guide

The library does not touch hardware. An integration has three parts:

1. **Receive:** the UART receive interrupt passes every byte to `baton_node_rx_isr()`.
   It only writes the ring of the node, so it is safe in an interrupt.
2. **Transmit:** a write function that *starts* sending a complete frame and returns,
   plus — without echo — a call of `baton_node_tx_done_isr()` when the last byte left.
3. **Processing and application:** one context calls `baton_node_process()` every
   millisecond with the current time; the callbacks run there.

Complete, buildable programs for each pattern are described in [Examples](examples.md).

## The physical layer

| Wiring | `rx_echo` | Notes |
| --- | --- | --- |
| Single wire: TX and RX on one pin (STM32 half-duplex mode, or open-drain TX tied to RX) | `true` | The receiver hears every transmitted byte; the library compares the echo and ends the transmission with the last echo byte |
| Open-drain bus with pull-up, separate TX and RX pins tied together | `true` | Same as above |
| RS-485 with automatic or GPIO direction control, receiver disabled while sending | `false` | Call `baton_node_tx_done_isr()` from the TX-complete (shift register empty) interrupt, after switching the transceiver back to receive |
| RS-485 with the receiver always enabled | `true` | The echo also detects a transceiver that drives too late or too long |

Keep the line idle-high, 8N1, the same baud rate on every node. The turnaround time of a
transceiver must be shorter than `tx_gap_ms`.

## Bare metal

```c
void USART1_IRQHandler(void) /* example names for an STM32 */
{
    if (usart_rx_not_empty())
    {
        baton_node_rx_isr(&node, usart_read_byte());
    }
    if (usart_tx_empty() && (tx_position < tx_length))
    {
        usart_write_byte(tx_buffer[tx_position]);
        tx_position++;
    }
}
```

The write function stores the pointer and the length and enables the TX-empty
interrupt; the library keeps the buffer valid until the end of the transmission. The
main loop calls `baton_node_process(&node, millis)` every millisecond (SysTick). When
`baton_node_send()` is also called from another interrupt, set `enter_critical` and
`exit_critical` to disable and restore interrupts (`__disable_irq()` / restore the
previous PRIMASK).

## RTOS

| Part | Where |
| --- | --- |
| `baton_node_rx_isr()`, `baton_node_tx_done_isr()` | UART interrupt handler; no RTOS call needed, the ring is lock-free |
| `baton_node_process()` | One task with a 1 ms period (`vTaskDelayUntil`, `k_sleep(K_MSEC(1))`) at a priority above the application tasks that wait for bus data |
| `baton_node_send()`, `baton_node_publish()` | Any task, with `enter_critical`/`exit_critical` set to a short interrupt lock (`irq_lock()`, `taskENTER_CRITICAL()`) |
| Callbacks | Run in the protocol task; post to a queue of the application task for longer work |

The critical section must exclude the UART interrupt and every other context that calls
send or publish. A mutex is not enough when an interrupt is one of them.

## Adding the library to a CMake build

```cmake
set(BATON_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)   # folder with baton_user_config.h
include(path/to/Baton-C/baton.cmake)
target_link_libraries(app PRIVATE baton)
target_link_libraries(baton PUBLIC zephyr_interface)       # Zephyr applications only
```

`baton.cmake` defines the static library target `baton` with its include directories
(the repository root, `inc/` and `BATON_CONFIG_DIR`) and the version definitions. For a
Zephyr application, linking `zephyr_interface` compiles the library with the board's
architecture flags.

Without CMake: compile `src/baton_protocol.c`, `src/baton_link.c`, `src/baton_queue.c`
and `src/baton_node.c` with C11 and the three include folders.

## Host

On a PC or a Linux board the master or a monitor reads the serial port with a short
timeout and passes the bytes to `baton_node_rx_isr()` from the same thread that calls
`baton_node_process()`. A blocking write is acceptable there; call
`baton_node_tx_done_isr()` after it returns when the adapter has no echo. USB-UART
adapters add latency (often 1–16 ms); keep `tx_gap_ms` and `ack_timeout_ms` above it.

## Hardware bring-up

Baton-C has been verified on a simulated bus (unit tests, the bus simulation with noise
and a dead node, the bare-metal example on a simulated board); it has **not yet been
tested on hardware**. When bringing it up:

1. Check the line with a logic analyzer: idle high, 8N1, the configured baud rate, and
   the first POLL `A5 11 01 20 01 00 7C 44` (master 0x01 polling 0x11).
2. Start with the master and one slave; `on_poll_done` must report `responded`.
3. Check `collisions` (must stay 0) and `tx_timeouts` on every node — they reveal a wrong
   `rx_echo` setting or a transceiver that switches too late.
4. Add the other slaves and confirm the cycle time against
   [Protocol → Time budget](protocol.md#time-budget).
5. Pull the cable of one slave: the others must keep their slots, and `on_drop` must
   report the events to it after `event_ttl_ms`.
