

# Group baton\_link



[**Modules**](modules.md) **>** [**baton\_link**](group__baton__link.md)



_Byte-level half-duplex link between the UART and the node layer._ [More...](#detailed-description)
















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**baton\_link**](structbaton__link.md) <br>_Link state._  |
| struct | [**baton\_link\_config**](structbaton__link__config.md) <br>_Link configuration passed to_ [_**baton\_link\_init**_](group__baton__link.md#function-baton_link_init) _._ |
| struct | [**baton\_link\_counters**](structbaton__link__counters.md) <br>_Link diagnostic counters._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef int(\* | [**baton\_write\_fn**](#typedef-baton_write_fn)  <br>_Start a non-blocking transmission._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**baton\_link\_cancel**](#function-baton_link_cancel) (struct [**baton\_link**](structbaton__link.md) \* link) <br>_Drop a requested transmission that has not started yet._  |
|  const struct [**baton\_frame**](structbaton__frame.md) \* | [**baton\_link\_frame**](#function-baton_link_frame) (const struct [**baton\_link**](structbaton__link.md) \* link) <br>_Last frame reported by BATON\_LINK\_FRAME._  |
|  uint32\_t | [**baton\_link\_frame\_ms**](#function-baton_link_frame_ms) (const struct [**baton\_link**](structbaton__link.md) \* link, uint32\_t bytes) <br>_Duration of_ `bytes` _bytes on the wire (10 bits per byte), rounded up._ |
|  int | [**baton\_link\_init**](#function-baton_link_init) (struct [**baton\_link**](structbaton__link.md) \* link, const struct [**baton\_link\_config**](structbaton__link__config.md) \* config, uint32\_t now\_ms) <br>_Initialize a link and reset its parser, ring and counters._  |
|  int | [**baton\_link\_receive**](#function-baton_link_receive) (struct [**baton\_link**](structbaton__link.md) \* link, uint32\_t now\_ms) <br>_Process received bytes until something happens._  |
|  bool | [**baton\_link\_rx\_busy**](#function-baton_link_rx_busy) (const struct [**baton\_link**](structbaton__link.md) \* link, uint32\_t now\_ms) <br>_Whether a frame is being received right now._  |
|  void | [**baton\_link\_rx\_isr**](#function-baton_link_rx_isr) (struct [**baton\_link**](structbaton__link.md) \* link, uint8\_t byte) <br>_Store one received byte._  |
|  int | [**baton\_link\_send**](#function-baton_link_send) (struct [**baton\_link**](structbaton__link.md) \* link, const uint8\_t \* data, size\_t length) <br>_Request the transmission of a complete frame._  |
|  int | [**baton\_link\_transmit**](#function-baton_link_transmit) (struct [**baton\_link**](structbaton__link.md) \* link, uint32\_t now\_ms) <br>_Start a requested transmission when the bus is silent; supervise a running one._  |
|  void | [**baton\_link\_tx\_done\_isr**](#function-baton_link_tx_done_isr) (struct [**baton\_link**](structbaton__link.md) \* link) <br>_Signal the end of a transmission._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BATON\_LINK\_FRAME**](group__baton__link.md#define-baton_link_frame)  `1`<br>_A valid frame was received; read it with_ [_**baton\_link\_frame()**_](group__baton__link.md#function-baton_link_frame) _._ |
| define  | [**BATON\_LINK\_NONE**](group__baton__link.md#define-baton_link_none)  `0`<br>_Nothing happened; the receive ring is empty or the transmitter has nothing to report._  |
| define  | [**BATON\_LINK\_TX\_DONE**](group__baton__link.md#define-baton_link_tx_done)  `2`<br>_The frame passed to_ [_**baton\_link\_send()**_](group__baton__link.md#function-baton_link_send) _was transmitted, failed or timed out._ |

## Detailed Description


The link owns the receive ring (filled from the UART interrupt), a parser with an inter-byte timeout and a transmitter that waits for a silent bus, checks the echo of its own bytes on a single-wire UART and detects a missing end of transmission. It reports what happened as events; the node layer ([**Node**](group__baton__node.md)) uses it, applications normally do not call it directly.


The library never reads a clock: every function that needs the time takes `now_ms`, a monotonic millisecond counter that may wrap.




**
**


* Lifecycle: [**baton\_link\_init**](group__baton__link.md#function-baton_link_init). 
* Interrupt entry points: [**baton\_link\_rx\_isr**](group__baton__link.md#function-baton_link_rx_isr), [**baton\_link\_tx\_done\_isr**](group__baton__link.md#function-baton_link_tx_done_isr). 
* Processing: [**baton\_link\_receive**](group__baton__link.md#function-baton_link_receive), [**baton\_link\_transmit**](group__baton__link.md#function-baton_link_transmit), [**baton\_link\_send**](group__baton__link.md#function-baton_link_send), [**baton\_link\_cancel**](group__baton__link.md#function-baton_link_cancel). 
* Queries: [**baton\_link\_frame**](group__baton__link.md#function-baton_link_frame), [**baton\_link\_rx\_busy**](group__baton__link.md#function-baton_link_rx_busy), [**baton\_link\_frame\_ms**](group__baton__link.md#function-baton_link_frame_ms). 







    
## Public Types Documentation




### typedef baton\_write\_fn 

_Start a non-blocking transmission._ 
```
typedef int(* baton_write_fn) (void *context, const uint8_t *data, size_t length);
```



The buffer stays valid and unchanged until the link reports BATON\_LINK\_TX\_DONE. The UART driver sends the bytes in the background (interrupt or DMA).




**Parameters:**


* `context` Value of [**baton\_link\_config::context**](structbaton__link__config.md#variable-context). 
* `data` Complete frame, SYNC to CRC. 
* `length` Number of bytes in `data`. 



**Returns:**

0 when the transmission was started, a negative value otherwise. 





        

<hr>
## Public Functions Documentation




### function baton\_link\_cancel 

_Drop a requested transmission that has not started yet._ 
```
void baton_link_cancel (
    struct baton_link * link
) 
```





**Parameters:**


* `link` Link state; NULL is ignored. 




        

<hr>



### function baton\_link\_frame 

_Last frame reported by BATON\_LINK\_FRAME._ 
```
const struct baton_frame * baton_link_frame (
    const struct baton_link * link
) 
```





**Parameters:**


* `link` Link state. 



**Returns:**

Frame, valid until the next call of [**baton\_link\_receive()**](group__baton__link.md#function-baton_link_receive); NULL when `link` is NULL. 





        

<hr>



### function baton\_link\_frame\_ms 

_Duration of_ `bytes` _bytes on the wire (10 bits per byte), rounded up._
```
uint32_t baton_link_frame_ms (
    const struct baton_link * link,
    uint32_t bytes
) 
```





**Parameters:**


* `link` Link state. 
* `bytes` Number of bytes. 



**Returns:**

Milliseconds; 0 when `link` is NULL. 





        

<hr>



### function baton\_link\_init 

_Initialize a link and reset its parser, ring and counters._ 
```
int baton_link_init (
    struct baton_link * link,
    const struct baton_link_config * config,
    uint32_t now_ms
) 
```





**Parameters:**


* `link` Link state to initialize. 
* `config` Configuration copied into the link. 
* `now_ms` Current time. 



**Return value:**


* `0` Link ready. 
* `BATON_PARSE_ERROR` NULL argument, no write function, zero baud rate or timeout. 




        

<hr>



### function baton\_link\_receive 

_Process received bytes until something happens._ 
```
int baton_link_receive (
    struct baton_link * link,
    uint32_t now_ms
) 
```



Echo bytes of the own transmission are compared and consumed; the other bytes go to the parser. Call again after every event until it returns BATON\_LINK\_NONE.




**Parameters:**


* `link` Link state. 
* `now_ms` Current time. 



**Return value:**


* `BATON_LINK_FRAME` A valid frame is available through [**baton\_link\_frame()**](group__baton__link.md#function-baton_link_frame). 
* `BATON_LINK_TX_DONE` The echo of the own frame is complete. 
* `BATON_LINK_NONE` The ring is empty (or `link` is NULL). 




        

<hr>



### function baton\_link\_rx\_busy 

_Whether a frame is being received right now._ 
```
bool baton_link_rx_busy (
    const struct baton_link * link,
    uint32_t now_ms
) 
```





**Parameters:**


* `link` Link state. 
* `now_ms` Current time. 



**Returns:**

true when the parser is inside a frame and the inter-byte timeout has not expired. 





        

<hr>



### function baton\_link\_rx\_isr 

_Store one received byte._ 
```
void baton_link_rx_isr (
    struct baton_link * link,
    uint8_t byte
) 
```



Call from the UART receive interrupt.


A byte that does not fit into the ring is counted and dropped. 

**Parameters:**


* `link` Link state; NULL is ignored. 
* `byte` Received byte. 




        

<hr>



### function baton\_link\_send 

_Request the transmission of a complete frame._ 
```
int baton_link_send (
    struct baton_link * link,
    const uint8_t * data,
    size_t length
) 
```



The frame is sent by [**baton\_link\_transmit()**](group__baton__link.md#function-baton_link_transmit) after the bus has been silent for tx\_gap\_ms. The buffer must stay valid until the link reports BATON\_LINK\_TX\_DONE. A new request replaces a pending one.




**Parameters:**


* `link` Link state. 
* `data` Complete frame, SYNC to CRC. 
* `length` Number of bytes (1..BATON\_FRAME\_MAX). 



**Return value:**


* `0` Request stored. 
* `BATON_PARSE_ERROR` NULL argument or invalid length. 




        

<hr>



### function baton\_link\_transmit 

_Start a requested transmission when the bus is silent; supervise a running one._ 
```
int baton_link_transmit (
    struct baton_link * link,
    uint32_t now_ms
) 
```





**Parameters:**


* `link` Link state. 
* `now_ms` Current time. 



**Return value:**


* `BATON_LINK_TX_DONE` The transmission finished, failed to start or timed out. 
* `BATON_LINK_NONE` Nothing to report (or `link` is NULL). 




        

<hr>



### function baton\_link\_tx\_done\_isr 

_Signal the end of a transmission._ 
```
void baton_link_tx_done_isr (
    struct baton_link * link
) 
```



Call from the UART TX-complete interrupt.


Needed only when [**baton\_link\_config::rx\_echo**](structbaton__link__config.md#variable-rx_echo) is false. 

**Parameters:**


* `link` Link state; NULL is ignored. 




        

<hr>
## Macro Definition Documentation





### define BATON\_LINK\_FRAME 

_A valid frame was received; read it with_ [_**baton\_link\_frame()**_](group__baton__link.md#function-baton_link_frame) _._
```
#define BATON_LINK_FRAME `1`
```




<hr>



### define BATON\_LINK\_NONE 

_Nothing happened; the receive ring is empty or the transmitter has nothing to report._ 
```
#define BATON_LINK_NONE `0`
```




<hr>



### define BATON\_LINK\_TX\_DONE 

_The frame passed to_ [_**baton\_link\_send()**_](group__baton__link.md#function-baton_link_send) _was transmitted, failed or timed out._
```
#define BATON_LINK_TX_DONE `2`
```




<hr>

------------------------------


