/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Board support interface of the bare-metal example
 *
 * Author:      Anton Vynohradov
 * Email:       avynohradov@systemfromscratch.com
 *
 * License:     Apache License 2.0
 *
 * Copyright (c) 2026 Anton Vynohradov
 *
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/

/**
 * @file board.h
 * @brief Board support interface of the bare-metal example.
 * @details Implement these four functions for the target MCU (UART driver,
 *          millisecond tick, sleep). board_host.c implements them on a PC and
 *          simulates the rest of the bus, so the example runs as a test in CI.
 */

#ifndef BOARD_H
#define BOARD_H

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <stddef.h>
#include <stdint.h>

/* ==============================================================================
 * PUBLIC FUNCTION DECLARATIONS
 * =============================================================================*/

/**
 * @brief   Configure the UART (single-wire half duplex, 8N1) and enable its RX interrupt.
 * @param   baud_rate - UART bit rate
 */
void board_init(uint32_t baud_rate);

/**
 * @brief   Monotonic millisecond counter (SysTick); may wrap.
 */
uint32_t board_millis(void);

/**
 * @brief   Start a non-blocking transmission (interrupt or DMA driven).
 * @details The buffer stays valid until the library has seen the echo of the
 *          last byte. On a single-wire UART the receiver hears every
 *          transmitted byte and passes it to app_uart_rx_isr().
 * @param   data - Bytes to send
 * @param   length - Number of bytes
 * @return  0 when the transmission was started, negative on error
 */
int board_uart_write(const uint8_t* data, size_t length);

/**
 * @brief   Wait for the next interrupt (for example WFI on Cortex-M; the 1 ms tick wakes it).
 */
void board_idle(void);

/**
 * @brief   Called by the board's UART RX interrupt for every received byte.
 * @details Implemented by the application (main.c).
 * @param   byte - Received byte
 */
void app_uart_rx_isr(uint8_t byte);

#endif /* BOARD_H */
