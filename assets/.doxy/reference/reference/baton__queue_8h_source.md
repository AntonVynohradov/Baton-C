

# File baton\_queue.h

[**File List**](files.md) **>** [**inc**](dir_bfccd401955b95cf8c75461437045ac0.md) **>** [**baton\_queue.h**](baton__queue_8h.md)

[Go to the documentation of this file](baton__queue_8h.md)


```C++

#ifndef BATON_QUEUE_H
#define BATON_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BATON_REC_HEADER_SIZE 16U
#define BATON_REC_VALID 0x01U
#define BATON_REC_SKIP 0x02U

struct baton_record
{
    uint32_t pending;  
    uint32_t inflight; 
    uint32_t stamp;    
    uint8_t msg_id;    
    uint8_t length;    
    uint8_t dst;       
    uint8_t flags;     
};

struct baton_queue
{
    uint8_t* memory; 
    uint16_t size;   
    uint16_t head;   
    uint16_t tail;   
};

void baton_queue_init(struct baton_queue* queue, uint8_t* memory, uint16_t size);

uint16_t baton_queue_record_size(uint8_t length);

bool baton_queue_alloc(struct baton_queue* queue, uint16_t need, uint16_t* offset);

uint16_t baton_queue_first(const struct baton_queue* queue, uint16_t head, uint16_t tail);

uint16_t baton_queue_next(const struct baton_queue* queue, uint16_t offset, uint16_t tail);

uint16_t baton_queue_used(const struct baton_queue* queue);

void baton_queue_read(const struct baton_queue* queue, uint16_t offset,
                      struct baton_record* record);

void baton_queue_write(struct baton_queue* queue, uint16_t offset,
                       const struct baton_record* record);

void baton_queue_set_masks(struct baton_queue* queue, uint16_t offset, uint32_t pending,
                           uint32_t inflight);

void baton_queue_set_flags(struct baton_queue* queue, uint16_t offset, uint8_t flags);

uint8_t* baton_queue_data(struct baton_queue* queue, uint16_t offset);

#ifdef __cplusplus
}
#endif

#endif /* BATON_QUEUE_H */

```


