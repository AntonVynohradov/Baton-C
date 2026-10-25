/**
 * @file baton_config.h
 * @brief Baton build-time configuration: sizes and the application message lists.
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
 * @defgroup baton Baton API
 * @brief Public API of the Baton bus library.
 * @details This module groups the public interface and its supporting
 *          submodules: @ref baton_config, @ref baton_defs, @ref baton_protocol,
 *          @ref baton_link, @ref baton_queue and @ref baton_node.
 */

/**
 * @defgroup baton_config Configuration
 * @ingroup baton
 * @brief Build-time configuration for Baton.
 *
 * @details The application provides `baton_user_config.h` on the include path
 *          (copy `config/baton_user_config_template.h`). It must define the two
 *          message lists @ref BATON_STATE_LIST and @ref BATON_EVENT_LIST and may
 *          override any size below; a size can also be set with a compiler
 *          definition (for example `-DBATON_MAX_NODES=4U`). Every node of a bus
 *          must be built with the same message lists.
 *
 * @par Configuration elements in this group:
 * @li @ref BATON_MAX_NODES : nodes on the bus including the master.
 * @li @ref BATON_MAX_PAYLOAD : largest frame payload.
 * @li @ref BATON_MAX_MSGS_PER_FRAME : messages packed into one frame.
 * @li @ref BATON_QUEUE_HIGH_SIZE, @ref BATON_QUEUE_NORMAL_SIZE : event queues.
 * @li @ref BATON_RX_BUF_SIZE : receive ring filled from the UART interrupt.
 * @{
 */

#ifndef BATON_CONFIG_H
#define BATON_CONFIG_H

/** @name Includes */
/** @{ */

#include "baton_user_config.h"

/** @} */

/** @name Defines and macros */
/** @{ */

/**
 * @def BATON_MAX_NODES
 * @brief Number of nodes on the bus including the master (2..32).
 */
#ifndef BATON_MAX_NODES
#define BATON_MAX_NODES 8U
#endif

/**
 * @def BATON_MAX_PAYLOAD
 * @brief Largest frame payload in bytes (16..250). One message needs its length + 2.
 */
#ifndef BATON_MAX_PAYLOAD
#define BATON_MAX_PAYLOAD 64U
#endif

/**
 * @def BATON_MAX_MSGS_PER_FRAME
 * @brief Largest number of messages packed into one frame (1..64).
 */
#ifndef BATON_MAX_MSGS_PER_FRAME
#define BATON_MAX_MSGS_PER_FRAME 16U
#endif

/**
 * @def BATON_QUEUE_HIGH_SIZE
 * @brief Size of the HIGH priority event queue in bytes (64..65532).
 * @details Each queued event uses 16 + length bytes, rounded up to 4.
 */
#ifndef BATON_QUEUE_HIGH_SIZE
#define BATON_QUEUE_HIGH_SIZE 256U
#endif

/**
 * @def BATON_QUEUE_NORMAL_SIZE
 * @brief Size of the NORMAL priority event queue in bytes (64..65532).
 */
#ifndef BATON_QUEUE_NORMAL_SIZE
#define BATON_QUEUE_NORMAL_SIZE 512U
#endif

/**
 * @def BATON_RX_BUF_SIZE
 * @brief Receive ring buffer filled from the UART interrupt; power of two, 16..4096.
 */
#ifndef BATON_RX_BUF_SIZE
#define BATON_RX_BUF_SIZE 128U
#endif

#ifndef BATON_STATE_LIST
#error "baton_user_config.h must define BATON_STATE_LIST(X)"
#endif

#ifndef BATON_EVENT_LIST
#error "baton_user_config.h must define BATON_EVENT_LIST(X)"
#endif

#if (BATON_MAX_NODES < 2U) || (BATON_MAX_NODES > 32U)
#error "BATON_MAX_NODES must be in the range 2..32"
#endif

#if (BATON_MAX_PAYLOAD < 16U) || (BATON_MAX_PAYLOAD > 250U)
#error "BATON_MAX_PAYLOAD must be in the range 16..250"
#endif

#if (BATON_MAX_MSGS_PER_FRAME < 1U) || (BATON_MAX_MSGS_PER_FRAME > 64U)
#error "BATON_MAX_MSGS_PER_FRAME must be in the range 1..64"
#endif

#if (BATON_QUEUE_HIGH_SIZE < 64U) || (BATON_QUEUE_HIGH_SIZE > 65532U)
#error "BATON_QUEUE_HIGH_SIZE must be in the range 64..65532"
#endif

#if (BATON_QUEUE_NORMAL_SIZE < 64U) || (BATON_QUEUE_NORMAL_SIZE > 65532U)
#error "BATON_QUEUE_NORMAL_SIZE must be in the range 64..65532"
#endif

#if (BATON_RX_BUF_SIZE < 16U) || (BATON_RX_BUF_SIZE > 4096U) \
    || ((BATON_RX_BUF_SIZE & (BATON_RX_BUF_SIZE - 1U)) != 0U)
#error "BATON_RX_BUF_SIZE must be a power of two in the range 16..4096"
#endif

/** @} */

#endif /* BATON_CONFIG_H */

/** @} */
