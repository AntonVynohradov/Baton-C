

# File baton\_node.h



[**FileList**](files.md) **>** [**inc**](dir_bfccd401955b95cf8c75461437045ac0.md) **>** [**baton\_node.h**](baton__node_8h.md)

[Go to the source code of this file](baton__node_8h_source.md)

_Baton node: master/slave bus schedule, reliable events, state mailboxes._ [More...](#detailed-description)

* `#include <stdbool.h>`
* `#include <stdint.h>`
* `#include "baton_config.h"`
* `#include "baton_defs.h"`
* `#include "baton_link.h"`
* `#include "baton_queue.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**baton\_channel**](structbaton__channel.md) <br>_Stop-and-wait state of the channel to one peer, both directions._  |
| struct | [**baton\_node**](structbaton__node.md) <br>_Node state._  |
| struct | [**baton\_node\_config**](structbaton__node__config.md) <br>_Node configuration passed to_ [_**baton\_node\_init**_](group__baton__node.md#function-baton_node_init) _; copied._ |
| struct | [**baton\_stats**](structbaton__stats.md) <br>_Diagnostic counters, see_ [_**baton\_node\_get\_stats()**_](group__baton__node.md#function-baton_node_get_stats) _._ |


















































## Detailed Description


Project: Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART Author: Anton Vynohradov Email: [avynohradov@systemfromscratch.com](mailto:avynohradov@systemfromscratch.com)




**Copyright:**

Copyright (c) 2026 Anton Vynohradov @license Apache License 2.0


Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at  Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.


SPDX-License-Identifier: Apache-2.0 


    

------------------------------
The documentation for this class was generated from the following file `inc/baton_node.h`

