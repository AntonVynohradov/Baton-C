/*******************************************************************************
 * Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
 *
 * Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Description: Serial port for Windows (Win32 communications API)
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
 * @file serial_port_win32.c
 * @brief serial_port.h on Windows (Win32 communications API).
 */

/* ==============================================================================
 * INCLUDES
 * =============================================================================*/

#include <stdio.h>
#include <windows.h>

#include "serial_port.h"

/* ==============================================================================
 * PRIVATE VARIABLES (static)
 * =============================================================================*/

static HANDLE port_handle = INVALID_HANDLE_VALUE;

/* ==============================================================================
 * PUBLIC FUNCTION IMPLEMENTATIONS
 * =============================================================================*/

int serial_open(const char* name, uint32_t baud_rate)
{
    char path[64];
    DCB dcb = {0};

    /* The \\.\ prefix is required for COM10 and above. */
    (void) snprintf(path, sizeof(path), "\\\\.\\%s", name);
    port_handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (port_handle == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(port_handle, &dcb))
    {
        serial_close();
        return -1;
    }
    dcb.BaudRate = baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    if (!SetCommState(port_handle, &dcb))
    {
        serial_close();
        return -1;
    }
    (void) PurgeComm(port_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return 0;
}

int serial_write(const uint8_t* data, size_t length)
{
    DWORD written = 0;

    if (!WriteFile(port_handle, data, (DWORD) length, &written, NULL) || (written != length))
    {
        return -1;
    }
    return 0;
}

int serial_read(uint8_t* data, size_t capacity, uint32_t timeout_ms)
{
    /* Return as soon as bytes are available, or after timeout_ms without any. */
    COMMTIMEOUTS timeouts = {MAXDWORD, MAXDWORD, timeout_ms, 0, 0};
    DWORD count = 0;

    if (!SetCommTimeouts(port_handle, &timeouts)
        || !ReadFile(port_handle, data, (DWORD) capacity, &count, NULL))
    {
        return -1;
    }
    return (int) count;
}

void serial_close(void)
{
    if (port_handle != INVALID_HANDLE_VALUE)
    {
        (void) CloseHandle(port_handle);
        port_handle = INVALID_HANDLE_VALUE;
    }
}

uint32_t host_millis(void)
{
    return (uint32_t) GetTickCount64();
}
