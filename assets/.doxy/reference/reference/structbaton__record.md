

# Struct baton\_record



[**ClassList**](annotated.md) **>** [**baton\_record**](structbaton__record.md)



_Header of one event record._ 

* `#include <baton_queue.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**dst**](#variable-dst)  <br>_Destination address given to_ [_**baton\_node\_send()**_](group__baton__node.md#function-baton_node_send) _._ |
|  uint8\_t | [**flags**](#variable-flags)  <br>_BATON\_REC\_VALID, BATON\_REC\_SKIP._  |
|  uint32\_t | [**inflight**](#variable-inflight)  <br>_Node mask: nodes with an unacknowledged frame containing it._  |
|  uint8\_t | [**length**](#variable-length)  <br>_Number of data bytes._  |
|  uint8\_t | [**msg\_id**](#variable-msg_id)  <br>_Message identifier._  |
|  uint32\_t | [**pending**](#variable-pending)  <br>_Node mask: nodes that have not acknowledged the event._  |
|  uint32\_t | [**stamp**](#variable-stamp)  <br>_Time of_ [_**baton\_node\_send()**_](group__baton__node.md#function-baton_node_send) _in ms._ |












































## Public Attributes Documentation




### variable dst 

_Destination address given to_ [_**baton\_node\_send()**_](group__baton__node.md#function-baton_node_send) _._
```C++
uint8_t baton_record::dst;
```




<hr>



### variable flags 

_BATON\_REC\_VALID, BATON\_REC\_SKIP._ 
```C++
uint8_t baton_record::flags;
```




<hr>



### variable inflight 

_Node mask: nodes with an unacknowledged frame containing it._ 
```C++
uint32_t baton_record::inflight;
```




<hr>



### variable length 

_Number of data bytes._ 
```C++
uint8_t baton_record::length;
```




<hr>



### variable msg\_id 

_Message identifier._ 
```C++
uint8_t baton_record::msg_id;
```




<hr>



### variable pending 

_Node mask: nodes that have not acknowledged the event._ 
```C++
uint32_t baton_record::pending;
```




<hr>



### variable stamp 

_Time of_ [_**baton\_node\_send()**_](group__baton__node.md#function-baton_node_send) _in ms._
```C++
uint32_t baton_record::stamp;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_queue.h`

