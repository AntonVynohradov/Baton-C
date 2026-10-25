/*
 * Put before every C snippet of the documentation by tools/docs/check_snippets.py:
 * the library and the board functions the snippets stand in for.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "baton.h"

int board_uart_write(const uint8_t* data, size_t length);
uint32_t board_millis(void);
void board_wait_for_tick(void);
void motor_stop(uint8_t mode);
void mark_offline(uint8_t slave);
void log_unreachable(uint8_t slave, uint32_t nodes);
void log_value(const char* name, uint32_t value);
void print_frame(uint8_t src, uint8_t dst, uint8_t type, uint8_t seq, const uint8_t* payload,
                 uint8_t length);
bool usart_rx_not_empty(void);
uint8_t usart_read_byte(void);
bool usart_tx_empty(void);
void usart_write_byte(uint8_t byte);
extern const uint8_t* tx_buffer;
extern size_t tx_position;
extern size_t tx_length;
