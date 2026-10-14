# Porting

The library needs four things from the platform:

1. a millisecond counter (`getTimeMs`),
2. a non-blocking transmission start (`uartStartTx`),
3. a call to `Intercom_RxByteIsr()` for every received byte,
4. a notification that transmission has finished: the echo (`rxEcho = true`) or `Intercom_TxDoneIsr()`.

Optionally: a critical section, if you send from interrupts or other tasks.

## Single-wire half-duplex (e.g. STM32 HDSEL)

In single-wire mode the TX line is also the RX line, and the receiver hears its own bytes. Set `rxEcho = true`. The library compares the echo with the transmitted data, uses it to detect the end of transmission and counts collisions. The line needs a pull-up and an open-drain output if there are more than two nodes.

```c
#include "Intercom.h"
#include "stm32g0xx_ll_usart.h"

extern UART_HandleTypeDef huart2;          /* HAL_HalfDuplex_Init(&huart2), 9600 8N1 */

static uint32_t port_time(void *ctx)
{
    (void)ctx;
    return HAL_GetTick();
}

static bool port_tx(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    /* In half-duplex the HAL switches direction; RX stays enabled to hear the echo. */
    return HAL_UART_Transmit_IT(&huart2, (uint8_t *)data, len) == HAL_OK;
}

void intercom_port_init(void)
{
    LL_USART_EnableIT_RXNE(USART2);       /* byte by byte, no HAL_UART_Receive_IT */
    LL_USART_EnableIT_ERROR(USART2);
}

void USART2_IRQHandler(void)
{
    if (LL_USART_IsActiveFlag_RXNE(USART2))
    {
        Intercom_RxByteIsr(LL_USART_ReceiveData8(USART2));   /* reading clears RXNE */
    }
    if (LL_USART_IsActiveFlag_ORE(USART2)) { LL_USART_ClearFlag_ORE(USART2); }
    if (LL_USART_IsActiveFlag_FE(USART2))  { LL_USART_ClearFlag_FE(USART2); }
    if (LL_USART_IsActiveFlag_NE(USART2))  { LL_USART_ClearFlag_NE(USART2); }

    HAL_UART_IRQHandler(&huart2);         /* TX handling (Transmit_IT) */
}
```

Note: on some STM32 families in half-duplex mode `HAL_HalfDuplex_EnableTransmitter()` disables the receiver. If you see no echo, switch to `rxEcho = false` and call `Intercom_TxDoneIsr()` from `HAL_UART_TxCpltCallback()`.

## RS-485 / no echo

When the receiver is disabled during transmission (RE pin) or the hardware does not return an echo:

```c
cfg.rxEcho = false;

static bool port_tx(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    HAL_GPIO_WritePin(DE_GPIO_Port, DE_Pin, GPIO_PIN_SET);       /* driver ON */
    return HAL_UART_Transmit_DMA(&huart1, (uint8_t *)data, len) == HAL_OK;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)          /* TC flag, not TXE */
{
    if (huart == &huart1)
    {
        HAL_GPIO_WritePin(DE_GPIO_Port, DE_Pin, GPIO_PIN_RESET); /* driver OFF */
        Intercom_TxDoneIsr();
    }
}
```

Many MCUs have hardware DE pin control (e.g. `HAL_RS485Ex_Init`); then calling `Intercom_TxDoneIsr()` is all you need.

## Critical sections

They are needed only when `Intercom_Send()`/`Intercom_Publish()` are called from interrupts or from an RTOS task other than the one running `Intercom_Process()`. The library does not nest `enterCritical` calls.

```c
static uint32_t s_primask;

static void port_enter(void *ctx)
{
    (void)ctx;
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    s_primask = pm;
}

static void port_exit(void *ctx)
{
    (void)ctx;
    __set_PRIMASK(s_primask);
}

cfg.enterCritical = port_enter;
cfg.exitCritical  = port_exit;
```

In FreeRTOS you can use `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` in tasks, or the `_FROM_ISR` variants in interrupts. In that case store the `UBaseType_t` in a static variable, as in the example above.

`getTimeMs` must then be interrupt-safe. `HAL_GetTick()` satisfies this.

## RTOS

```c
void intercom_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    for (;;)
    {
        Intercom_Process();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1));
    }
}
```

- Give the task a high priority: it is short and never blocks.
- Callbacks (`onMessage` etc.) run in the context of this task. Hand longer processing off to other tasks through an RTOS queue.

## Main loop without an RTOS

Call `Intercom_Process()` on every loop iteration. If the loop takes longer than ~1 ms:

- increase `INTERCOM_RX_BUF_SIZE` (about 1 B/ms at 9600 bit/s),
- remember that `Process` latency slows down reactions. The slot budget has a 2 ms margin, but `ackTimeoutMs` and `txGapMs` should account for the real loop latency.

## Bring-up checklist

- [ ] `Intercom_Init()` returns `INTERCOM_OK` on every node
- [ ] The same `slaveAddrs` list and the same `IntercomConfig.h` on all nodes
- [ ] RX interrupt enabled after `Intercom_Init()`
- [ ] `rxEcho` matches the hardware: with a wrong setting `txTimeouts` and `collisions` grow
- [ ] On the master, `onPollDone` reports `responded = true` for every slave
- [ ] `Intercom_GetStats()`: `crcErrors`, `rxOverflows`, `collisions` ≈ 0 on a clean bus
