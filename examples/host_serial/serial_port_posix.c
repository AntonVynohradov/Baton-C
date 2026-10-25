/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Serial port for Linux and macOS (termios)
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
 * @file serial_port_posix.c
 * @brief serial_port.h on Linux and macOS (termios).
 */

/* cfmakeraw() is not part of POSIX.1 */
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "serial_port.h"

/* ==============================================================================
 * PRIVATE VARIABLES (static)
 * =============================================================================*/

static int port_fd = -1;

/* ==============================================================================
 * PRIVATE FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

static int baud_constant(uint32_t baud_rate, speed_t* speed)
{
    int result = 0;

    switch (baud_rate)
    {
        case 9600U:
            *speed = B9600;
            break;
        case 57600U:
            *speed = B57600;
            break;
        case 115200U:
            *speed = B115200;
            break;
        case 230400U:
            *speed = B230400;
            break;
#ifdef B1000000
        case 1000000U:
            *speed = B1000000;
            break;
        case 2000000U:
            *speed = B2000000;
            break;
        case 3000000U:
            *speed = B3000000;
            break;
        case 4000000U:
            *speed = B4000000;
            break;
#endif
        default:
            result = -1;
            break;
    }
    return result;
}

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

int serial_open(const char* name, uint32_t baud_rate)
{
    struct termios tio;
    speed_t speed;

    if (baud_constant(baud_rate, &speed) != 0)
    {
        return -1;
    }
    port_fd = open(name, O_RDWR | O_NOCTTY);
    if (port_fd < 0)
    {
        return -1;
    }
    if (tcgetattr(port_fd, &tio) != 0)
    {
        serial_close();
        return -1;
    }
    cfmakeraw(&tio);
    tio.c_cflag |= (tcflag_t) (CLOCAL | CREAD);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    if ((cfsetispeed(&tio, speed) != 0) || (cfsetospeed(&tio, speed) != 0)
        || (tcsetattr(port_fd, TCSANOW, &tio) != 0))
    {
        serial_close();
        return -1;
    }
    (void) tcflush(port_fd, TCIOFLUSH);
    return 0;
}

int serial_write(const uint8_t* data, size_t length)
{
    size_t written = 0U;

    while (written < length)
    {
        const ssize_t count = write(port_fd, &data[written], length - written);

        if (count <= 0)
        {
            return -1;
        }
        written += (size_t) count;
    }
    return (tcdrain(port_fd) == 0) ? 0 : -1;
}

int serial_read(uint8_t* data, size_t capacity, uint32_t timeout_ms)
{
    struct pollfd pfd = {port_fd, POLLIN, 0};
    const int ready = poll(&pfd, 1, (int) timeout_ms);
    ssize_t count;

    if (ready <= 0)
    {
        return ready;
    }
    count = read(port_fd, data, capacity);
    return (int) count;
}

void serial_close(void)
{
    if (port_fd >= 0)
    {
        (void) close(port_fd);
        port_fd = -1;
    }
}

uint32_t host_millis(void)
{
    struct timespec ts;

    (void) clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t) (((uint64_t) ts.tv_sec * 1000U) + ((uint64_t) ts.tv_nsec / 1000000U));
}
