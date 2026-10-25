

# File baton\_link.h

[**File List**](files.md) **>** [**inc**](dir_bfccd401955b95cf8c75461437045ac0.md) **>** [**baton\_link.h**](baton__link_8h.md)

[Go to the documentation of this file](baton__link_8h.md)


```C++

#ifndef BATON_LINK_H
#define BATON_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "baton_protocol.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BATON_LINK_NONE 0
#define BATON_LINK_FRAME 1
#define BATON_LINK_TX_DONE 2

typedef int (*baton_write_fn)(void* context, const uint8_t* data, size_t length);

struct baton_link_config
{
    baton_write_fn write;          
    void* context;                 
    uint32_t baudrate;             
    uint8_t tx_gap_ms;             
    uint8_t inter_byte_timeout_ms; 
    bool rx_echo;                  
};

struct baton_link_counters
{
    uint32_t tx_frames;   
    uint32_t rx_frames;   
    uint32_t crc_errors;  
    uint32_t rx_timeouts; 
    uint32_t collisions;  
    uint32_t tx_timeouts; 
};

struct baton_link
{
    struct baton_link_config config;     
    struct baton_parser parser;          
    struct baton_link_counters counters; 
    volatile uint8_t rx_buffer[BATON_RX_BUF_SIZE]; 
    volatile uint16_t rx_head;                     
    volatile uint16_t rx_tail;                     
    volatile uint32_t rx_overflows;                
    volatile bool tx_done_flag;                    
    const uint8_t* tx_data;  
    uint16_t tx_length;      
    uint16_t echo_index;     
    bool echo_error;         
    uint8_t tx_state;        
    uint32_t tx_start_ms;    
    uint32_t last_tx_end_ms; 
    uint32_t last_rx_ms;     
};

int baton_link_init(struct baton_link* link, const struct baton_link_config* config,
                    uint32_t now_ms);

void baton_link_rx_isr(struct baton_link* link, uint8_t byte);

void baton_link_tx_done_isr(struct baton_link* link);

int baton_link_receive(struct baton_link* link, uint32_t now_ms);

int baton_link_transmit(struct baton_link* link, uint32_t now_ms);

int baton_link_send(struct baton_link* link, const uint8_t* data, size_t length);

void baton_link_cancel(struct baton_link* link);

const struct baton_frame* baton_link_frame(const struct baton_link* link);

bool baton_link_rx_busy(const struct baton_link* link, uint32_t now_ms);

uint32_t baton_link_frame_ms(const struct baton_link* link, uint32_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* BATON_LINK_H */

```


