

# File baton\_queue.c



[**FileList**](files.md) **>** [**src**](dir_68267d1309a1af8e8297ef4c3efbcdba.md) **>** [**baton\_queue.c**](baton__queue_8c.md)

[Go to the source code of this file](baton__queue_8c_source.md)



* `#include <stddef.h>`
* `#include <string.h>`
* `#include "baton_defs.h"`
* `#include "baton_queue.h"`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**align4**](#function-align4) (uint16\_t value) <br> |
|  uint16\_t | [**normalize**](#function-normalize) (const struct [**baton\_queue**](structbaton__queue.md) \* queue, uint16\_t offset, uint16\_t tail) <br>_Continue at offset 0 when no header fits at_ `offset` _or a SKIP marker is there._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**REC\_DST**](baton__queue_8c.md#define-rec_dst)  `14U`<br> |
| define  | [**REC\_FLAGS**](baton__queue_8c.md#define-rec_flags)  `15U`<br> |
| define  | [**REC\_INFLIGHT**](baton__queue_8c.md#define-rec_inflight)  `4U`<br> |
| define  | [**REC\_LENGTH**](baton__queue_8c.md#define-rec_length)  `13U`<br> |
| define  | [**REC\_MSG\_ID**](baton__queue_8c.md#define-rec_msg_id)  `12U`<br> |
| define  | [**REC\_PENDING**](baton__queue_8c.md#define-rec_pending)  `0U`<br>_Byte offsets of the header fields inside a record._  |
| define  | [**REC\_STAMP**](baton__queue_8c.md#define-rec_stamp)  `8U`<br> |

## Public Static Functions Documentation




### function align4 

```C++
static uint16_t align4 (
    uint16_t value
) 
```




<hr>



### function normalize 

_Continue at offset 0 when no header fits at_ `offset` _or a SKIP marker is there._
```C++
static uint16_t normalize (
    const struct baton_queue * queue,
    uint16_t offset,
    uint16_t tail
) 
```




<hr>
## Macro Definition Documentation





### define REC\_DST 

```C++
#define REC_DST `14U`
```




<hr>



### define REC\_FLAGS 

```C++
#define REC_FLAGS `15U`
```




<hr>



### define REC\_INFLIGHT 

```C++
#define REC_INFLIGHT `4U`
```




<hr>



### define REC\_LENGTH 

```C++
#define REC_LENGTH `13U`
```




<hr>



### define REC\_MSG\_ID 

```C++
#define REC_MSG_ID `12U`
```




<hr>



### define REC\_PENDING 

_Byte offsets of the header fields inside a record._ 
```C++
#define REC_PENDING `0U`
```




<hr>



### define REC\_STAMP 

```C++
#define REC_STAMP `8U`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `src/baton_queue.c`

