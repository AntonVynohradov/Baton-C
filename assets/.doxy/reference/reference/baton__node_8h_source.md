

# File baton\_node.h

[**File List**](files.md) **>** [**inc**](dir_bfccd401955b95cf8c75461437045ac0.md) **>** [**baton\_node.h**](baton__node_8h.md)

[Go to the documentation of this file](baton__node_8h.md)


```C++

#ifndef BATON_NODE_H
#define BATON_NODE_H

#include <stdbool.h>
#include <stdint.h>

#include "baton_config.h"
#include "baton_defs.h"
#include "baton_link.h"
#include "baton_queue.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BATON_OK 0              
#define BATON_ERR_PARAM (-1)    
#define BATON_ERR_NOT_INIT (-2) 
#define BATON_ERR_MSG_ID (-3)   
#define BATON_ERR_LEN (-4)      
#define BATON_ERR_ADDR (-5)     
#define BATON_ERR_FULL (-6)     
#define BATON_ROLE_MASTER 0U 
#define BATON_ROLE_SLAVE 1U  
#define BATON_PRIO_NORMAL 0U 
#define BATON_PRIO_HIGH 1U   
#define BATON_MASK_BYTES ((BATON_MAX_NODES + 7U) / 8U)
#define BATON_POLL_PREFIX 1U
#define BATON_RESP_PREFIX (1U + BATON_MASK_BYTES)
#define BATON_MAX_MSG_LEN (BATON_MAX_PAYLOAD - BATON_RESP_PREFIX - BATON_TLV_HEADER_SIZE)

#define BATON_DEAD_STREAK 3U
#define BATON_DEAD_POLL_DIV 4U

/* Deviation DEV-04 (Rule 20.10): token pasting builds the public names from the lists. */
/* cppcheck-suppress misra-c2012-20.10 */
#define BATON_X_MSG_ENUM_(name, id, max_length) BATON_MSG_##name = (id),
/* cppcheck-suppress misra-c2012-20.10 */
#define BATON_X_MSG_SIZE_(name, id, max_length) BATON_MSG_##name##_MAX_LEN = (max_length),
#define BATON_X_COUNT_(name, id, max_length) +1U
#define BATON_X_STATE_SIZE_(name, id, max_length) +(uint32_t) (max_length)
enum baton_msg_id
{
    BATON_STATE_LIST(BATON_X_MSG_ENUM_) BATON_EVENT_LIST(BATON_X_MSG_ENUM_) BATON_MSG_INVALID_ =
        0x100
};

enum baton_msg_max_len
{
    BATON_STATE_LIST(BATON_X_MSG_SIZE_) BATON_EVENT_LIST(BATON_X_MSG_SIZE_) BATON_MSG_SIZE_DUMMY_ =
        0
};

#define BATON_STATE_COUNT (0U BATON_STATE_LIST(BATON_X_COUNT_))
#define BATON_EVENT_COUNT (0U BATON_EVENT_LIST(BATON_X_COUNT_))
#define BATON_STATE_DATA_SIZE (1U BATON_STATE_LIST(BATON_X_STATE_SIZE_))

typedef void (*baton_critical_fn)(void* context);

typedef void (*baton_message_fn)(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                                 uint8_t length);

typedef void (*baton_drop_fn)(void* context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes);

typedef void (*baton_poll_done_fn)(void* context, uint8_t slave, bool responded,
                                   uint32_t nack_nodes);

struct baton_node_config
{
    /* Topology */
    uint8_t role;                   
    uint8_t own_address;            
    uint8_t master_address;         
    const uint8_t* slave_addresses; 
    uint8_t slave_count;            
    /* Timing (milliseconds unless stated) */
    uint32_t baudrate;             
    uint16_t slot_time_ms;         
    uint16_t ack_timeout_ms;       
    uint8_t max_retries;           
    uint8_t tx_gap_ms;             
    uint8_t inter_byte_timeout_ms; 
    uint16_t cycle_period_ms; 
    uint32_t event_ttl_ms;    
    /* UART */
    bool rx_echo;         
    baton_write_fn write; 
    /* Critical section (optional, both or neither) */
    baton_critical_fn enter_critical; 
    baton_critical_fn exit_critical;  
    /* Application callbacks (optional, called from baton_node_process()) */
    baton_message_fn on_message;     
    baton_drop_fn on_drop;           
    baton_poll_done_fn on_poll_done; 
    void* context; 
};

struct baton_stats
{
    uint32_t tx_frames;         
    uint32_t rx_frames;         
    uint32_t crc_errors;        
    uint32_t rx_timeouts;       
    uint32_t rx_overflows;      
    uint32_t collisions;        
    uint32_t tx_timeouts;       
    uint32_t ack_timeouts;      
    uint32_t retries;           
    uint32_t resp_timeouts;     
    uint32_t duplicates;        
    uint32_t bad_messages;      
    uint32_t events_dropped;    
    uint32_t queue_full;        
    uint16_t queue_high_used;   
    uint16_t queue_normal_used; 
};

struct baton_channel
{
    uint16_t inflight[BATON_MAX_MSGS_PER_FRAME]; 
    uint8_t inflight_count; 
    uint8_t tx_seq;         
    bool tx_first;          
    uint8_t rx_seq;         
    bool rx_valid;          
    bool rx_last_first;     
    uint8_t resp_seq;       
    bool resp_valid;        
    uint8_t fail_streak;    
};

struct baton_node
{
    struct baton_node_config config; 
    struct baton_link link;          
    bool initialized;                
    uint8_t node_address[BATON_MAX_NODES]; 
    uint8_t node_count;                    
    uint8_t self_index;                    
    uint32_t others_mask;                  
    uint8_t tx_frame[BATON_FRAME_MAX];       
    uint8_t ack_frame[BATON_FRAME_OVERHEAD]; 
    struct baton_queue queue[2];                          
    uint8_t queue_high_memory[BATON_QUEUE_HIGH_SIZE];     
    uint8_t queue_normal_memory[BATON_QUEUE_NORMAL_SIZE]; 
    uint8_t state_data[BATON_STATE_DATA_SIZE];    
    uint8_t state_length[BATON_STATE_COUNT + 1U]; 
    uint32_t state_dirty[BATON_STATE_COUNT + 1U]; 
    struct baton_channel channel[BATON_MAX_NODES]; 
    uint8_t link_state;  
    uint32_t link_timer; 
    uint32_t slot_start; 
    uint8_t slot_peer;   
    uint8_t slot_k;      
    uint8_t slot_rr;     
    uint8_t slot_pass;   
    uint8_t retries;     
    uint32_t slot_nack;  
    uint8_t poll_seq;    
    uint8_t current;      
    uint32_t cycle_start; 
    uint32_t cycle_count; 
    uint32_t now_ms;            
    uint32_t last_housekeep_ms; 
    struct baton_stats stats;   
};

void baton_node_default_config(struct baton_node_config* config);

int baton_node_init(struct baton_node* node, const struct baton_node_config* config,
                    uint32_t now_ms);

void baton_node_process(struct baton_node* node, uint32_t now_ms);

int baton_node_send(struct baton_node* node, uint8_t dst, uint8_t msg_id, const void* data,
                    uint8_t length, uint8_t priority);

int baton_node_publish(struct baton_node* node, uint8_t msg_id, const void* data, uint8_t length);

void baton_node_rx_isr(struct baton_node* node, uint8_t byte);

void baton_node_tx_done_isr(struct baton_node* node);

void baton_node_get_stats(const struct baton_node* node, struct baton_stats* stats);

#ifdef __cplusplus
}
#endif

#endif /* BATON_NODE_H */

```


