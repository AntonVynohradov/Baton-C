

# File baton\_protocol.h



[**FileList**](files.md) **>** [**inc**](dir_bfccd401955b95cf8c75461437045ac0.md) **>** [**baton\_protocol.h**](baton__protocol_8h.md)

[Go to the source code of this file](baton__protocol_8h_source.md)

_Baton codec: CRC-16/CCITT-FALSE, incremental frame parser and frame encoder._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include "baton_config.h"`
* `#include "baton_defs.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**baton\_frame**](structbaton__frame.md) <br>_One decoded frame (header fields and payload, without SYNC and CRC)._  |
| struct | [**baton\_parser**](structbaton__parser.md) <br>_Incremental parser state for one byte stream._  |


















































## Detailed Description


Project: Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART Author: Anton Vynohradov Email: [avynohradov@systemfromscratch.com](mailto:avynohradov@systemfromscratch.com)




**Copyright:**

Copyright (c) 2026 Anton Vynohradov @license Apache License 2.0


Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at  Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.


SPDX-License-Identifier: Apache-2.0 


    

------------------------------
The documentation for this class was generated from the following file `inc/baton_protocol.h`

