

# File baton\_protocol.h

[**File List**](files.md) **>** [**inc**](dir_bfccd401955b95cf8c75461437045ac0.md) **>** [**baton\_protocol.h**](baton__protocol_8h.md)

[Go to the documentation of this file](baton__protocol_8h.md)


```C++

#ifndef BATON_PROTOCOL_H
#define BATON_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "baton_config.h"
#include "baton_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BATON_FRAME_MAX (BATON_FRAME_OVERHEAD + BATON_MAX_PAYLOAD)

#define BATON_CRC_INIT 0xFFFFU

#define BATON_PARSER_IDLE 0U

#define BATON_PARSE_ERROR (-1)

struct baton_frame
{
    uint8_t dst;                        
    uint8_t src;                        
    uint8_t ctrl;                       
    uint8_t length;                     
    uint8_t payload[BATON_MAX_PAYLOAD]; 
};

struct baton_parser
{
    struct baton_frame frame; 
    uint16_t crc;             
    uint16_t index;           
    uint8_t state;            
};

uint16_t baton_crc16_update(uint16_t crc, uint8_t byte);

uint16_t baton_crc16(const uint8_t* data, size_t length);

void baton_parser_init(struct baton_parser* parser);

int baton_parser_feed(struct baton_parser* parser, uint8_t byte);

int baton_encode(uint8_t dst, uint8_t src, uint8_t ctrl, const uint8_t* payload,
                 size_t payload_length, uint8_t* output, size_t output_capacity,
                 size_t* output_length);

#ifdef __cplusplus
}
#endif

#endif /* BATON_PROTOCOL_H */

```


