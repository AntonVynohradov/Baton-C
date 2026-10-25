

# Group baton\_queue



[**Modules**](modules.md) **>** [**baton\_queue**](group__baton__queue.md)



_Byte ring of variable-size event records, used by_ [_**Node**_](group__baton__node.md) _._[More...](#detailed-description)
















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**baton\_queue**](structbaton__queue.md) <br>_Queue state._  |
| struct | [**baton\_record**](structbaton__record.md) <br>_Header of one event record._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  bool | [**baton\_queue\_alloc**](#function-baton_queue_alloc) (struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t need, uint16\_t \* offset) <br>_Reserve_ `need` _bytes at the tail._ |
|  uint8\_t \* | [**baton\_queue\_data**](#function-baton_queue_data) (struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t offset) <br>_Data bytes of the record at_ `offset` _._ |
|  uint16\_t | [**baton\_queue\_first**](#function-baton_queue_first) (const struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t head, uint16\_t tail) <br>_Offset of the first record for a head/tail snapshot._  |
|  void | [**baton\_queue\_init**](#function-baton_queue_init) (struct [**baton\_queue**](structbaton__queue.md) \* queue, uint8\_t \* memory, uint16\_t size) <br>_Initialize an empty queue over_ `memory` _._ |
|  uint16\_t | [**baton\_queue\_next**](#function-baton_queue_next) (const struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t offset, uint16\_t tail) <br>_Offset of the record after the one at_ `offset` _._ |
|  void | [**baton\_queue\_read**](#function-baton_queue_read) (const struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t offset, struct [**baton\_record**](structbaton__record.md) \* record) <br>_Read the header of the record at_ `offset` _._ |
|  uint16\_t | [**baton\_queue\_record\_size**](#function-baton_queue_record_size) (uint8\_t length) <br>_Bytes a record with_ `length` _data bytes occupies._ |
|  void | [**baton\_queue\_set\_flags**](#function-baton_queue_set_flags) (struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t offset, uint8\_t flags) <br>_Update only the flags of the record at_ `offset` _._ |
|  void | [**baton\_queue\_set\_masks**](#function-baton_queue_set_masks) (struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t offset, uint32\_t pending, uint32\_t inflight) <br>_Update only the node masks of the record at_ `offset` _._ |
|  uint16\_t | [**baton\_queue\_used**](#function-baton_queue_used) (const struct [**baton\_queue**](structbaton__queue.md) \* queue) <br>_Bytes currently in use._  |
|  void | [**baton\_queue\_write**](#function-baton_queue_write) (struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t offset, const struct [**baton\_record**](structbaton__record.md) \* record) <br>_Write the complete header of the record at_ `offset` _._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BATON\_REC\_HEADER\_SIZE**](group__baton__queue.md#define-baton_rec_header_size)  `16U`<br>_Serialized size of a record header in bytes._  |
| define  | [**BATON\_REC\_SKIP**](group__baton__queue.md#define-baton_rec_skip)  `0x02U`<br>_Marker at the end of the buffer: continue at offset 0._  |
| define  | [**BATON\_REC\_VALID**](group__baton__queue.md#define-baton_rec_valid)  `0x01U`<br>_Record is complete and visible to the consumer._  |

## Detailed Description


Layout rules: 
* A record is a 16-byte header ([**baton\_record**](structbaton__record.md), serialized byte by byte) followed by the data, padded to a multiple of 4 bytes. 
* A record is never split at the end of the buffer. If it does not fit at the end, a SKIP marker is written there (when a header fits) and the record goes to offset 0. A reader that reaches an offset without room for a header, or a SKIP marker, continues at 0. 
* head == tail means empty; the producer resets both to 0 when the queue is empty, so the wrapped state never starts from a stale head. 
* Records are released only from the head.

The producer ([**baton\_node\_send()**](group__baton__node.md#function-baton_node_send)) may run in an interrupt; it calls [**baton\_queue\_alloc**](group__baton__queue.md#function-baton_queue_alloc) with the critical section held and publishes a record by setting [**BATON\_REC\_VALID**](group__baton__queue.md#define-baton_rec_valid) last. 


    
## Public Functions Documentation




### function baton\_queue\_alloc 

_Reserve_ `need` _bytes at the tail._
```
bool baton_queue_alloc (
    struct baton_queue * queue,
    uint16_t need,
    uint16_t * offset
) 
```



Call with the critical section held. 

**Parameters:**


* `queue` Queue state. 
* `need` Record size from [**baton\_queue\_record\_size()**](group__baton__queue.md#function-baton_queue_record_size). 
* `offset` Receives the record offset. 



**Returns:**

true when the space was reserved. 





        

<hr>



### function baton\_queue\_data 

_Data bytes of the record at_ `offset` _._
```
uint8_t * baton_queue_data (
    struct baton_queue * queue,
    uint16_t offset
) 
```




<hr>



### function baton\_queue\_first 

_Offset of the first record for a head/tail snapshot._ 
```
uint16_t baton_queue_first (
    const struct baton_queue * queue,
    uint16_t head,
    uint16_t tail
) 
```





**Returns:**

Offset of the first record, or `tail` when the queue is empty. 





        

<hr>



### function baton\_queue\_init 

_Initialize an empty queue over_ `memory` _._
```
void baton_queue_init (
    struct baton_queue * queue,
    uint8_t * memory,
    uint16_t size
) 
```





**Parameters:**


* `queue` Queue state. 
* `memory` Storage of `size` bytes. 
* `size` Storage size; rounded down to a multiple of 4. 




        

<hr>



### function baton\_queue\_next 

_Offset of the record after the one at_ `offset` _._
```
uint16_t baton_queue_next (
    const struct baton_queue * queue,
    uint16_t offset,
    uint16_t tail
) 
```





**Returns:**

Offset of the next record, or `tail` after the last one. 





        

<hr>



### function baton\_queue\_read 

_Read the header of the record at_ `offset` _._
```
void baton_queue_read (
    const struct baton_queue * queue,
    uint16_t offset,
    struct baton_record * record
) 
```




<hr>



### function baton\_queue\_record\_size 

_Bytes a record with_ `length` _data bytes occupies._
```
uint16_t baton_queue_record_size (
    uint8_t length
) 
```




<hr>



### function baton\_queue\_set\_flags 

_Update only the flags of the record at_ `offset` _._
```
void baton_queue_set_flags (
    struct baton_queue * queue,
    uint16_t offset,
    uint8_t flags
) 
```




<hr>



### function baton\_queue\_set\_masks 

_Update only the node masks of the record at_ `offset` _._
```
void baton_queue_set_masks (
    struct baton_queue * queue,
    uint16_t offset,
    uint32_t pending,
    uint32_t inflight
) 
```




<hr>



### function baton\_queue\_used 

_Bytes currently in use._ 
```
uint16_t baton_queue_used (
    const struct baton_queue * queue
) 
```




<hr>



### function baton\_queue\_write 

_Write the complete header of the record at_ `offset` _._
```
void baton_queue_write (
    struct baton_queue * queue,
    uint16_t offset,
    const struct baton_record * record
) 
```




<hr>
## Macro Definition Documentation





### define BATON\_REC\_HEADER\_SIZE 

_Serialized size of a record header in bytes._ 
```
#define BATON_REC_HEADER_SIZE `16U`
```




<hr>



### define BATON\_REC\_SKIP 

_Marker at the end of the buffer: continue at offset 0._ 
```
#define BATON_REC_SKIP `0x02U`
```




<hr>



### define BATON\_REC\_VALID 

_Record is complete and visible to the consumer._ 
```
#define BATON_REC_VALID `0x01U`
```




<hr>

------------------------------


