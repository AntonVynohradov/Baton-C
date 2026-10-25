# Examples

Three examples share one bus definition, `examples/config/baton_user_config.h`: master
0x01, slaves 0x11 and 0x12, the state `NODE_STATUS` (uptime in seconds) and the events
`CMD_SET_OUTPUT` and `OUTPUT_REPORT`.

| Example | Role | Platform | Built and run in CI |
| --- | --- | --- | --- |
| [Bare metal](#bare-metal) | Slave 0x11 | Any MCU; runs on a PC against a simulated board | Yes, as a self-checking test |
| [Host serial master](#host-serial-master) | Master 0x01 | Linux, macOS, Windows | Built |
| [Zephyr](#zephyr) | Slave 0x12 | Any Zephyr board | No — needs a Zephyr workspace; not compiled in this repository |

```bash
cmake --preset "BATON[Examples]"
cmake --build "build/BATON[Examples]"
ctest --test-dir "build/BATON[Examples]" -L example --output-on-failure
```

## Bare metal

| File | Content |
| --- | --- |
| `examples/bare_metal/board.h` | Four board functions: `board_init()`, `board_millis()`, `board_uart_write()`, `board_idle()`; the application provides `app_uart_rx_isr()` |
| `examples/bare_metal/main.c` | The slave: configuration, main loop, uptime state, output commands |
| `examples/bare_metal/board_host.c` | `board.h` on a PC: a simulated single-wire bus with the master and slave 0x12 as further Baton nodes; checks the results |

**Structure.** The UART receive interrupt calls `app_uart_rx_isr()`, which passes the
byte to `baton_node_rx_isr()`. The main loop calls `baton_node_process()` every
millisecond, publishes `NODE_STATUS` once per second, and in `on_message` applies
`CMD_SET_OUTPUT` and answers with an `OUTPUT_REPORT` event to the sender. Everything
runs in the main loop, so no critical section is configured.

**On a PC.** `board_idle()` advances a simulated clock by 1 ms and moves one byte per
transmitter over the bus; every byte reaches every node, the sender included (echo).
Before each of the three requests a burst of line noise — a broken header `A5 11 01 00`
with an invalid length, and a stray byte — is put on the idle bus. Real output:

```text
bare-metal example: slave 0x11 at 9600 bit/s on a simulated single-wire bus
  1502 ms  master -> 0x11  CMD_SET_OUTPUT output 0 = 0x01 (after line noise)
  1563 ms  master <- 0x11  OUTPUT_REPORT output 0 = 0x01
  2202 ms  0x12   -> 0x11  CMD_SET_OUTPUT output 1 = 0x55 (after line noise)
  2290 ms  0x12   <- 0x11  OUTPUT_REPORT output 1 = 0x55
  3106 ms  master -> 0x11  CMD_SET_OUTPUT output 2 = 0x00 (after line noise)
  3147 ms  master <- 0x11  OUTPUT_REPORT output 2 = 0x00
master: 274 polls (0 without answer), 3 CRC errors, 0 frames aborted by timeout
ok    master got 2 OUTPUT_REPORT events
ok    last report to the master: output 2 = 0x00
ok    slave 0x12 got 1 OUTPUT_REPORT event
ok    report to 0x12: output 1 = 0x55
ok    master sees the uptime state of 0x11
ok    slave 0x12 sees the uptime state of 0x11
ok    no collisions on the bus
EXAMPLE PASSED
```

The three CRC errors are the three noise bursts (their length byte is invalid).

**Porting.** Implement `board.h` for the MCU:

| Function | Implementation |
| --- | --- |
| `board_init()` | Clock the UART, configure single-wire half duplex 8N1 at the baud rate, enable the RX interrupt (and the TX interrupt in the driver) |
| `board_millis()` | A 1 ms SysTick counter |
| `board_uart_write()` | Store the buffer and start an interrupt or DMA transmission; return 0 |
| `board_idle()` | `__WFI()`; the SysTick interrupt wakes the loop every millisecond |

## Host serial master

| File | Content |
| --- | --- |
| `examples/host_serial/main.c` | Command-line master `baton_master` |
| `examples/host_serial/serial_port.h` | Five functions: open, write, read with timeout, close, millisecond clock |
| `examples/host_serial/serial_port_posix.c` | termios implementation (Linux, macOS) |
| `examples/host_serial/serial_port_win32.c` | Win32 implementation |

```text
baton_master <port> [--baud N] [--echo] [--seconds N] [--set ADDR OUTPUT VALUE]...
baton_master /dev/ttyUSB0 --set 0x11 0 1
baton_master COM3 --echo --seconds 30
```

The master polls 0x11 and 0x12, prints every message it receives and, once per second,
the number of polls, the answered ones and the CRC errors. Each `--set` queues a
`CMD_SET_OUTPUT` event. Use `--echo` with a single-wire adapter that receives its own
bytes; without it, the end of `serial_write()` is taken as the end of the transmission
(`baton_node_tx_done_isr()`). The example has not been run against hardware yet; without
an adapter it reports `cannot open <port>`.

## Zephyr

| File | Content |
| --- | --- |
| `examples/zephyr/src/main.c` | Slave 0x12: UART interrupt driver, one protocol thread |
| `examples/zephyr/CMakeLists.txt` | Includes `baton.cmake`, links `zephyr_interface` |
| `examples/zephyr/prj.conf` | Serial, interrupt-driven UART, no heap |
| `examples/zephyr/app.overlay.example` | The chosen node `baton,uart` |

```bash
west build -b <board> examples/zephyr -- -DDTC_OVERLAY_FILE=<board>.overlay
```

The UART interrupt passes received bytes to `baton_node_rx_isr()` and feeds the transmit
FIFO from the buffer of the write function; the transmission ends with the echo of the
last byte (`rx_echo`). A thread calls `baton_node_process()` every millisecond and
publishes the uptime. `enter_critical` and `exit_critical` use `irq_lock()` so that other
threads may send events. Disable the console and logging on the bus UART.

This example is not compiled in this repository (no Zephyr workspace in CI); treat it as
a starting point.
