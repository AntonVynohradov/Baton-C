

# File baton\_defs.h

[**File List**](files.md) **>** [**inc**](dir_bfccd401955b95cf8c75461437045ac0.md) **>** [**baton\_defs.h**](baton__defs_8h.md)

[Go to the documentation of this file](baton__defs_8h.md)


```C++

#ifndef BATON_DEFS_H
#define BATON_DEFS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BATON_SYNC 0xA5U
#define BATON_HEADER_SIZE 5U
#define BATON_CRC_SIZE 2U
#define BATON_FRAME_OVERHEAD (BATON_HEADER_SIZE + BATON_CRC_SIZE)
#define BATON_ADDR_BROADCAST 0xFFU

#define BATON_FT_POLL 0U
#define BATON_FT_RESP 1U
#define BATON_FT_DATA 2U
#define BATON_FT_ACK 3U

#define BATON_ACKB_VALID 0x80U
#define BATON_SEQ_MASK 0x0FU
#define BATON_TLV_HEADER_SIZE 2U

static inline uint8_t baton_ctrl(uint8_t type, bool first, uint8_t seq)
{
    const uint8_t first_bit = first ? 0x20U : 0x00U;

    return (uint8_t) ((uint8_t) ((uint8_t) (type & 0x03U) << 6U) | first_bit
                      | (uint8_t) (seq & BATON_SEQ_MASK));
}

static inline uint8_t baton_ctrl_type(uint8_t ctrl)
{
    return (uint8_t) (ctrl >> 6U);
}

static inline bool baton_ctrl_first(uint8_t ctrl)
{
    return (ctrl & 0x20U) != 0U;
}

static inline uint8_t baton_ctrl_seq(uint8_t ctrl)
{
    return (uint8_t) (ctrl & BATON_SEQ_MASK);
}

static inline uint16_t baton_get_u16be(const uint8_t* data)
{
    return (uint16_t) ((uint16_t) ((uint16_t) data[0] << 8U) | (uint16_t) data[1]);
}

static inline void baton_put_u16be(uint8_t* data, uint16_t value)
{
    data[0] = (uint8_t) (value >> 8U);
    data[1] = (uint8_t) (value & 0xFFU);
}

static inline uint32_t baton_get_u32le(const uint8_t* data)
{
    return (uint32_t) data[0] | ((uint32_t) data[1] << 8U) | ((uint32_t) data[2] << 16U)
           | ((uint32_t) data[3] << 24U);
}

static inline void baton_put_u32le(uint8_t* data, uint32_t value)
{
    data[0] = (uint8_t) (value & 0xFFU);
    data[1] = (uint8_t) ((value >> 8U) & 0xFFU);
    data[2] = (uint8_t) ((value >> 16U) & 0xFFU);
    data[3] = (uint8_t) (value >> 24U);
}

#ifdef __cplusplus
}
#endif

#endif /* BATON_DEFS_H */

```


