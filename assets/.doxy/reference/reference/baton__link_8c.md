

# File baton\_link.c



[**FileList**](files.md) **>** [**src**](dir_68267d1309a1af8e8297ef4c3efbcdba.md) **>** [**baton\_link.c**](baton__link_8c.md)

[Go to the source code of this file](baton__link_8c_source.md)



* `#include <string.h>`
* `#include "baton_link.h"`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  bool | [**rx\_echo**](#function-rx_echo) (struct [**baton\_link**](structbaton__link.md) \* link, uint8\_t byte, uint32\_t now\_ms) <br>_Compare one byte with the echo of the own transmission._  |
|  int | [**rx\_parse**](#function-rx_parse) (struct [**baton\_link**](structbaton__link.md) \* link, uint8\_t byte, uint32\_t now\_ms) <br>_Feed one byte from another node to the parser._  |
|  int | [**tx\_finish**](#function-tx_finish) (struct [**baton\_link**](structbaton__link.md) \* link, uint32\_t now\_ms) <br>_End the current transmission and report it._  |
|  int | [**tx\_start**](#function-tx_start) (struct [**baton\_link**](structbaton__link.md) \* link, uint32\_t now\_ms) <br>_Start the requested transmission once the bus has been silent long enough._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RX\_MASK**](baton__link_8c.md#define-rx_mask)  `([**BATON\_RX\_BUF\_SIZE**](group__baton__config.md#define-baton_rx_buf_size) - 1U)`<br>_Index mask of the receive ring (its size is a power of two)._  |
| define  | [**TX\_BUSY**](baton__link_8c.md#define-tx_busy)  `2U`<br> |
| define  | [**TX\_IDLE**](baton__link_8c.md#define-tx_idle)  `0U`<br>_Transmitter states._  |
| define  | [**TX\_TIMEOUT\_MARGIN\_MS**](baton__link_8c.md#define-tx_timeout_margin_ms)  `5U`<br>_Margin added to the expected duration of a transmission before it times out._  |
| define  | [**TX\_WAIT\_GAP**](baton__link_8c.md#define-tx_wait_gap)  `1U`<br> |

## Public Static Functions Documentation




### function rx\_echo 

_Compare one byte with the echo of the own transmission._ 
```C++
static bool rx_echo (
    struct baton_link * link,
    uint8_t byte,
    uint32_t now_ms
) 
```





**Returns:**

true when the byte was an echo byte and the echo is now complete. 





        

<hr>



### function rx\_parse 

_Feed one byte from another node to the parser._ 
```C++
static int rx_parse (
    struct baton_link * link,
    uint8_t byte,
    uint32_t now_ms
) 
```




<hr>



### function tx\_finish 

_End the current transmission and report it._ 
```C++
static int tx_finish (
    struct baton_link * link,
    uint32_t now_ms
) 
```




<hr>



### function tx\_start 

_Start the requested transmission once the bus has been silent long enough._ 
```C++
static int tx_start (
    struct baton_link * link,
    uint32_t now_ms
) 
```




<hr>
## Macro Definition Documentation





### define RX\_MASK 

_Index mask of the receive ring (its size is a power of two)._ 
```C++
#define RX_MASK `( BATON_RX_BUF_SIZE - 1U)`
```




<hr>



### define TX\_BUSY 

```C++
#define TX_BUSY `2U`
```




<hr>



### define TX\_IDLE 

_Transmitter states._ 
```C++
#define TX_IDLE `0U`
```




<hr>



### define TX\_TIMEOUT\_MARGIN\_MS 

_Margin added to the expected duration of a transmission before it times out._ 
```C++
#define TX_TIMEOUT_MARGIN_MS `5U`
```




<hr>



### define TX\_WAIT\_GAP 

```C++
#define TX_WAIT_GAP `1U`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `src/baton_link.c`

