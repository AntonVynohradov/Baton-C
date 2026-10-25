/**
 * @file baton.h
 * @brief Umbrella header of the Baton bus library.
 *
 * Project: Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
 * Author: Anton Vynohradov
 * Email: avynohradov@systemfromscratch.com
 *
 * @copyright Copyright (c) 2026 Anton Vynohradov
 * @license Apache License 2.0
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
 */

/**
 * @addtogroup baton Baton API
 * @brief Public API of the Baton bus library.
 *
 * @details Include this single header to use the library:
 * @li baton_config.h   build-time sizes and the application message lists
 * @li baton_defs.h     frame layout, frame types, control-byte and byte-order helpers
 * @li baton_protocol.h CRC-16, incremental parser, encoder
 * @li baton_link.h     half-duplex link: receive ring, parser timeout, transmitter
 * @li baton_queue.h    event queue (used by the node)
 * @li baton_node.h     master/slave node: events, states, schedule
 * @{
 */

#ifndef BATON_H
#define BATON_H

/** @name Includes */
/** @{ */

#include "baton_config.h"
#include "baton_defs.h"
#include "baton_protocol.h"
#include "baton_link.h"
#include "baton_queue.h"
#include "baton_node.h"

/** @} */

/**
 * @name Library version
 * @details Normally injected by cmake/version.cmake from the Git tag; the
 *          values below apply to builds that do not use CMake.
 */
/** @{ */

#ifndef BATON_VERSION_MAJOR
#define BATON_VERSION_MAJOR 1
#endif

#ifndef BATON_VERSION_MINOR
#define BATON_VERSION_MINOR 0
#endif

#ifndef BATON_VERSION_PATCH
#define BATON_VERSION_PATCH 0
#endif

/** Version string "MAJOR.MINOR.PATCH". */
#ifndef BATON_VERSION
#define BATON_VERSION "1.0.0"
#endif

/** @} */

#endif /* BATON_H */

/** @} */
