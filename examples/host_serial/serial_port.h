/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Serial port interface of the host example
 *
 * Author:      Anton Vynohradov
 * Email:       avynohradov@systemfromscratch.com
 *
 * License:     Apache License 2.0
 *
 * Copyright (c) 2026 Anton Vynohradov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/

/**
 * @file serial_port.h
 * @brief Minimal serial port interface of the host example (one open port).
 * @details serial_port_posix.c implements it with termios, serial_port_win32.c
 *          with the Win32 communications API.
 */

#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <stddef.h>
#include <stdint.h>

/* ==============================================================================
 * PUBLIC FUNCTION DECLARATIONS
 * =============================================================================*/

/**
 * @brief   Open a port in raw 8N1 mode.
 * @param   name - Port name, for example /dev/ttyUSB0 or COM3
 * @param   baud_rate - Bit rate
 * @return  0 on success, negative on error
 */
int serial_open(const char* name, uint32_t baud_rate);

/**
 * @brief   Write all bytes.
 * @return  0 on success, negative on error
 */
int serial_write(const uint8_t* data, size_t length);

/**
 * @brief   Read what has arrived, waiting at most timeout_ms for the first byte.
 * @return  Number of bytes read (0 on timeout), negative on error
 */
int serial_read(uint8_t* data, size_t capacity, uint32_t timeout_ms);

/**
 * @brief   Close the port.
 */
void serial_close(void);

/**
 * @brief   Monotonic millisecond counter of the host.
 */
uint32_t host_millis(void);

#endif /* SERIAL_PORT_H */
