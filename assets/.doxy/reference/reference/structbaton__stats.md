

# Struct baton\_stats



[**ClassList**](annotated.md) **>** [**baton\_stats**](structbaton__stats.md)



_Diagnostic counters, see_ [_**baton\_node\_get\_stats()**_](group__baton__node.md#function-baton_node_get_stats) _._

* `#include <baton_node.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**ack\_timeouts**](#variable-ack_timeouts)  <br>_Slave: no ACK for a DATA frame._  |
|  uint32\_t | [**bad\_messages**](#variable-bad_messages)  <br>_Unknown id or bad length inside a frame._  |
|  uint32\_t | [**collisions**](#variable-collisions)  <br>_Echo mismatch while transmitting._  |
|  uint32\_t | [**crc\_errors**](#variable-crc_errors)  <br>_Frames rejected: CRC mismatch or invalid LEN._  |
|  uint32\_t | [**duplicates**](#variable-duplicates)  <br>_Retransmitted content filtered out._  |
|  uint32\_t | [**events\_dropped**](#variable-events_dropped)  <br>_Events dropped by the TTL._  |
|  uint32\_t | [**queue\_full**](#variable-queue_full)  <br>[_**baton\_node\_send()**_](group__baton__node.md#function-baton_node_send) _rejected for lack of space._ |
|  uint16\_t | [**queue\_high\_used**](#variable-queue_high_used)  <br>_Bytes in use in the HIGH queue._  |
|  uint16\_t | [**queue\_normal\_used**](#variable-queue_normal_used)  <br>_Bytes in use in the NORMAL queue._  |
|  uint32\_t | [**resp\_timeouts**](#variable-resp_timeouts)  <br>_Master: a slave did not answer a POLL._  |
|  uint32\_t | [**retries**](#variable-retries)  <br>_Slave: DATA retransmissions._  |
|  uint32\_t | [**rx\_frames**](#variable-rx_frames)  <br>_Valid frames seen on the bus (any destination)._  |
|  uint32\_t | [**rx\_overflows**](#variable-rx_overflows)  <br>_Bytes lost because the receive ring was full._  |
|  uint32\_t | [**rx\_timeouts**](#variable-rx_timeouts)  <br>_Frames aborted by the inter-byte timeout._  |
|  uint32\_t | [**tx\_frames**](#variable-tx_frames)  <br>_Transmissions started._  |
|  uint32\_t | [**tx\_timeouts**](#variable-tx_timeouts)  <br>_Transmissions that failed to start or did not finish._  |












































## Public Attributes Documentation




### variable ack\_timeouts 

_Slave: no ACK for a DATA frame._ 
```C++
uint32_t baton_stats::ack_timeouts;
```




<hr>



### variable bad\_messages 

_Unknown id or bad length inside a frame._ 
```C++
uint32_t baton_stats::bad_messages;
```




<hr>



### variable collisions 

_Echo mismatch while transmitting._ 
```C++
uint32_t baton_stats::collisions;
```




<hr>



### variable crc\_errors 

_Frames rejected: CRC mismatch or invalid LEN._ 
```C++
uint32_t baton_stats::crc_errors;
```




<hr>



### variable duplicates 

_Retransmitted content filtered out._ 
```C++
uint32_t baton_stats::duplicates;
```




<hr>



### variable events\_dropped 

_Events dropped by the TTL._ 
```C++
uint32_t baton_stats::events_dropped;
```




<hr>



### variable queue\_full 

[_**baton\_node\_send()**_](group__baton__node.md#function-baton_node_send) _rejected for lack of space._
```C++
uint32_t baton_stats::queue_full;
```




<hr>



### variable queue\_high\_used 

_Bytes in use in the HIGH queue._ 
```C++
uint16_t baton_stats::queue_high_used;
```




<hr>



### variable queue\_normal\_used 

_Bytes in use in the NORMAL queue._ 
```C++
uint16_t baton_stats::queue_normal_used;
```




<hr>



### variable resp\_timeouts 

_Master: a slave did not answer a POLL._ 
```C++
uint32_t baton_stats::resp_timeouts;
```




<hr>



### variable retries 

_Slave: DATA retransmissions._ 
```C++
uint32_t baton_stats::retries;
```




<hr>



### variable rx\_frames 

_Valid frames seen on the bus (any destination)._ 
```C++
uint32_t baton_stats::rx_frames;
```




<hr>



### variable rx\_overflows 

_Bytes lost because the receive ring was full._ 
```C++
uint32_t baton_stats::rx_overflows;
```




<hr>



### variable rx\_timeouts 

_Frames aborted by the inter-byte timeout._ 
```C++
uint32_t baton_stats::rx_timeouts;
```




<hr>



### variable tx\_frames 

_Transmissions started._ 
```C++
uint32_t baton_stats::tx_frames;
```




<hr>



### variable tx\_timeouts 

_Transmissions that failed to start or did not finish._ 
```C++
uint32_t baton_stats::tx_timeouts;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_node.h`

