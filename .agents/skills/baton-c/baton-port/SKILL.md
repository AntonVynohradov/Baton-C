---
name: baton-port
description: Integrate the Baton-C library into an application on a specific MCU — single-wire UART or RS-485 hookup, interrupt glue, critical sections, RTOS task, timing for the baud rate, baton_user_config.h for the application, bring-up checklist. Use when the user wants to port, wire up, or bring up Baton-C on hardware or in a firmware project.
---

# Porting and integrating Baton-C on an MCU

Port code goes into the **application project**, not into the library repository (usually
a Git submodule). The library stays free of hardware dependencies. Reference material:
`docs/integration.md`, `docs/usage.md`, `examples/bare_metal`; on Zephyr use the module
(`docs/zephyr.md`, `zephyr/samples/baton_node`) instead of writing glue.

## 1. Clarify with the user

Ask in one set of questions about anything the application project does not show:

- MCU and driver layer (STM32 HAL/LL, ESP-IDF, nRF, AVR, Zephyr, bare metal),
- line mode: single wire with echo → `rx_echo = true`; RS-485 or no echo →
  `rx_echo = false` + `baton_node_tx_done_isr()` from the TX-complete interrupt,
- baud rate (default 9600), number of nodes, role and addresses of this node,
- RTOS or superloop; will `baton_node_send()`/`baton_node_publish()` be called from
  interrupts or other tasks (then the critical-section hooks are required),
- how often the main loop or task can call `baton_node_process()` (≤ 1 ms).

## 2. Files in the application project

```
<app>/config/baton_user_config.h   copy of config/baton_user_config_template.h — SAME on every node
<app>/port/baton_port.c            write function, UART interrupt, critical section, init
<app>/port/baton_port.h            void baton_port_init(void); struct baton_node* baton_port_node(void);
```

CMake: `set(BATON_CONFIG_DIR <app>/config)`, `include(<lib>/baton.cmake)`,
`target_link_libraries(app PRIVATE baton)`; Zephyr also
`target_link_libraries(baton PUBLIC zephyr_interface)`. Other build systems: compile
`src/*.c` (C11) with the repository root, `inc/` and the config folder on the include path.

## 3. Glue requirements

| Item | Requirement |
| --- | --- |
| `now_ms` | Monotonic ms counter passed to `baton_node_init()` and `baton_node_process()`; `uint32_t` wrap is handled |
| `write(context, data, length)` | Start IT/DMA transmission and **return at once** (0, or negative on failure). The buffer stays valid until the end of the transmission |
| RX interrupt | `baton_node_rx_isr(&node, byte)` for every byte, the own echo included. Clear ORE/FE/NE, otherwise RX stalls |
| TX done (`rx_echo = false`) | `baton_node_tx_done_isr(&node)` from the **TC** (transmission complete) interrupt, not TXE; switch the RS-485 driver off there |
| `enter_critical` / `exit_critical` | Both or neither; the library does not nest them; save and restore PRIMASK or the RTOS state |

## 4. Patterns

**STM32 single wire (HDSEL), echo:**

```c
static int port_write(void* context, const uint8_t* data, size_t length)
{
    (void) context;
    return (HAL_UART_Transmit_IT(&huart2, (uint8_t*) data, (uint16_t) length) == HAL_OK) ? 0 : -1;
}

void USART2_IRQHandler(void)
{
    if (LL_USART_IsActiveFlag_RXNE(USART2))
    {
        baton_node_rx_isr(&node, LL_USART_ReceiveData8(USART2)); /* reading clears RXNE */
    }
    if (LL_USART_IsActiveFlag_ORE(USART2)) { LL_USART_ClearFlag_ORE(USART2); }
    if (LL_USART_IsActiveFlag_FE(USART2))  { LL_USART_ClearFlag_FE(USART2); }
    if (LL_USART_IsActiveFlag_NE(USART2))  { LL_USART_ClearFlag_NE(USART2); }
    HAL_UART_IRQHandler(&huart2); /* TX */
}
```

Enable RXNE with `LL_USART_EnableIT_RXNE()` (no `HAL_UART_Receive_IT`) after
`baton_node_init()`. On some families `HAL_HalfDuplex_EnableTransmitter()` disables the
receiver: no echo → `rx_echo = false` and `baton_node_tx_done_isr()` in
`HAL_UART_TxCpltCallback()`.

**RS-485 without echo:** DE pin on before `HAL_UART_Transmit_DMA()`, off in
`HAL_UART_TxCpltCallback()` followed by `baton_node_tx_done_isr(&node)`. With hardware DE
control (`HAL_RS485Ex_Init`) only the TX-done call is needed.

**Critical section (Cortex-M):**

```c
static uint32_t saved_primask;

static void port_enter(void* context)
{
    const uint32_t primask = __get_PRIMASK();
    (void) context;
    __disable_irq();
    saved_primask = primask;
}

static void port_exit(void* context)
{
    (void) context;
    __set_PRIMASK(saved_primask);
}
```

FreeRTOS: `taskENTER_CRITICAL()`/`taskEXIT_CRITICAL()` (the `_FROM_ISR` variants store
the `UBaseType_t` in a static). Zephyr: `irq_lock()`/`irq_unlock()`.

**RTOS task:** `for (;;) { baton_node_process(&node, now()); vTaskDelayUntil(&last, pdMS_TO_TICKS(1)); }`
at a high priority; callbacks run there — hand long work to other tasks through a queue.

## 5. Timing for the baud rate

Frame times scale with 1/baud; `baton_node_init()` rejects a slot that is too short
(formulas in `docs/protocol.md`, *Time budget*). With `BATON_MAX_PAYLOAD = 64`:

| baud | slot_time_ms | ack_timeout_ms | tx_gap_ms | inter_byte_timeout_ms |
| --- | --- | --- | --- | --- |
| 9600 | 250 (min 174) | 20 | 2 | 5 |
| 19200 | 130 | 12 | 2 | 3 |
| 115200 | 40 | 5 | 1 | 2 |

The 19200 and 115200 rows are starting points: check them with `baton_node_init()` for
the actual configuration. Add main-loop latency above ~1 ms to `ack_timeout_ms` and
`tx_gap_ms`; grow `BATON_RX_BUF_SIZE` by about `baud / 10000` bytes per ms between
`baton_node_process()` calls.

## 6. 8/16-bit MCUs

Not yet compiled with a 16-bit `int`. On AVR or MSP430: build with
`-Wall -Wextra -Wconversion` and fix warnings in the library following `baton-core-change`;
node masks (`state_dirty`, queue masks) are 32-bit and not atomic — when publish or send
run in an interrupt, the critical section is **mandatory**.

## 7. Bring-up checklist

- [ ] `baton_node_init()` returns `BATON_OK` on every node (else see `docs/usage.md`, *Set up a node*)
- [ ] Logic analyzer: first POLL of master 0x01 to 0x11 is `A5 11 01 20 01 00 7C 44`
- [ ] Master: `on_poll_done(slave, responded = true, …)` for every slave
- [ ] `baton_node_get_stats()` after a minute on a clean bus: `crc_errors`, `rx_overflows`,
      `collisions`, `tx_timeouts`, `bad_messages` ≈ 0
  - `tx_timeouts` > 0 and no RESP → wrong `rx_echo` or the TX-done call is missing
  - `collisions` > 0 → distorted echo (pull-up, open drain, levels) or a node outside its slot
  - `rx_overflows` > 0 → process called too rarely or the ring too small
- [ ] Send an event A → B and see it in `on_message`; publish a state and see it on all nodes
- [ ] Unplug one slave: the others keep working; `on_drop` reports its events after `event_ttl_ms`
