

# Struct baton\_channel



[**ClassList**](annotated.md) **>** [**baton\_channel**](structbaton__channel.md)



_Stop-and-wait state of the channel to one peer, both directions._ [More...](#detailed-description)

* `#include <baton_node.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**fail\_streak**](#variable-fail_streak)  <br>_Consecutive failures (slave: ACK, master: RESP)._  |
|  uint16\_t | [**inflight**](#variable-inflight)  <br>_Messages of the unacknowledged frame._  |
|  uint8\_t | [**inflight\_count**](#variable-inflight_count)  <br>
> 0: the last frame with content is not acknowledged yet. 
 |
|  uint8\_t | [**resp\_seq**](#variable-resp_seq)  <br>_Master: sequence number of the last RESP with content._  |
|  bool | [**resp\_valid**](#variable-resp_valid)  <br>_Master: resp\_seq is valid._  |
|  bool | [**rx\_last\_first**](#variable-rx_last_first)  <br>_The last delivered frame had the FIRST flag._  |
|  uint8\_t | [**rx\_seq**](#variable-rx_seq)  <br>_Sequence number of the last delivered frame._  |
|  bool | [**rx\_valid**](#variable-rx_valid)  <br>_rx\_seq is valid._  |
|  bool | [**tx\_first**](#variable-tx_first)  <br>_FIRST flag until the first acknowledgement after init._  |
|  uint8\_t | [**tx\_seq**](#variable-tx_seq)  <br>_Sequence number of the next or outstanding frame._  |












































## Detailed Description


Private. 


    
## Public Attributes Documentation




### variable fail\_streak 

_Consecutive failures (slave: ACK, master: RESP)._ 
```C++
uint8_t baton_channel::fail_streak;
```




<hr>



### variable inflight 

_Messages of the unacknowledged frame._ 
```C++
uint16_t baton_channel::inflight[BATON_MAX_MSGS_PER_FRAME];
```




<hr>



### variable inflight\_count 


> 0: the last frame with content is not acknowledged yet. 

```C++
uint8_t baton_channel::inflight_count;
```




<hr>



### variable resp\_seq 

_Master: sequence number of the last RESP with content._ 
```C++
uint8_t baton_channel::resp_seq;
```




<hr>



### variable resp\_valid 

_Master: resp\_seq is valid._ 
```C++
bool baton_channel::resp_valid;
```




<hr>



### variable rx\_last\_first 

_The last delivered frame had the FIRST flag._ 
```C++
bool baton_channel::rx_last_first;
```




<hr>



### variable rx\_seq 

_Sequence number of the last delivered frame._ 
```C++
uint8_t baton_channel::rx_seq;
```




<hr>



### variable rx\_valid 

_rx\_seq is valid._ 
```C++
bool baton_channel::rx_valid;
```




<hr>



### variable tx\_first 

_FIRST flag until the first acknowledgement after init._ 
```C++
bool baton_channel::tx_first;
```




<hr>



### variable tx\_seq 

_Sequence number of the next or outstanding frame._ 
```C++
uint8_t baton_channel::tx_seq;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_node.h`

