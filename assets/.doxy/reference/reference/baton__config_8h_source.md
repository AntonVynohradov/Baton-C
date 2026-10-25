

# File baton\_config.h

[**File List**](files.md) **>** [**inc**](dir_bfccd401955b95cf8c75461437045ac0.md) **>** [**baton\_config.h**](baton__config_8h.md)

[Go to the documentation of this file](baton__config_8h.md)


```C++

#ifndef BATON_CONFIG_H
#define BATON_CONFIG_H

#include "baton_user_config.h"

#ifndef BATON_MAX_NODES
#define BATON_MAX_NODES 8U
#endif

#ifndef BATON_MAX_PAYLOAD
#define BATON_MAX_PAYLOAD 64U
#endif

#ifndef BATON_MAX_MSGS_PER_FRAME
#define BATON_MAX_MSGS_PER_FRAME 16U
#endif

#ifndef BATON_QUEUE_HIGH_SIZE
#define BATON_QUEUE_HIGH_SIZE 256U
#endif

#ifndef BATON_QUEUE_NORMAL_SIZE
#define BATON_QUEUE_NORMAL_SIZE 512U
#endif

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

#endif /* BATON_CONFIG_H */

```


