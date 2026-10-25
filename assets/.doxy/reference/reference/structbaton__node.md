

# Struct baton\_node



[**ClassList**](annotated.md) **>** [**baton\_node**](structbaton__node.md)



_Node state._ [More...](#detailed-description)

* `#include <baton_node.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**ack\_frame**](#variable-ack_frame)  <br>_ACK being sent._  |
|  struct [**baton\_channel**](structbaton__channel.md) | [**channel**](#variable-channel)  <br>_One channel per node index._  |
|  struct [**baton\_node\_config**](structbaton__node__config.md) | [**config**](#variable-config)  <br>_Copy of the configuration._  |
|  uint8\_t | [**current**](#variable-current)  <br>_Master: index of the polled slave - 1._  |
|  uint32\_t | [**cycle\_count**](#variable-cycle_count)  <br>_Master: number of cycles started._  |
|  uint32\_t | [**cycle\_start**](#variable-cycle_start)  <br>_Master: start of the current cycle._  |
|  bool | [**initialized**](#variable-initialized)  <br>[_**baton\_node\_init()**_](group__baton__node.md#function-baton_node_init) _succeeded._ |
|  uint32\_t | [**last\_housekeep\_ms**](#variable-last_housekeep_ms)  <br>_Last TTL scan._  |
|  struct [**baton\_link**](structbaton__link.md) | [**link**](#variable-link)  <br>_Byte-level link._  |
|  uint8\_t | [**link\_state**](#variable-link_state)  <br>_Master/slave state machine._  |
|  uint32\_t | [**link\_timer**](#variable-link_timer)  <br>_Start of the current wait._  |
|  uint8\_t | [**node\_address**](#variable-node_address)  <br>_Index 0 = master, 1..n = slaves._  |
|  uint8\_t | [**node\_count**](#variable-node_count)  <br>_Master + slaves._  |
|  uint32\_t | [**now\_ms**](#variable-now_ms)  <br>_Time of the last_ [_**baton\_node\_process()**_](group__baton__node.md#function-baton_node_process) _._ |
|  uint32\_t | [**others\_mask**](#variable-others_mask)  <br>_Node mask of all other nodes._  |
|  uint8\_t | [**poll\_seq**](#variable-poll_seq)  <br>_Slave: sequence number of the last POLL._  |
|  struct [**baton\_queue**](structbaton__queue.md) | [**queue**](#variable-queue)  <br>_[HIGH], [NORMAL]._  |
|  uint8\_t | [**queue\_high\_memory**](#variable-queue_high_memory)  <br>_HIGH queue storage._  |
|  uint8\_t | [**queue\_normal\_memory**](#variable-queue_normal_memory)  <br>_NORMAL queue storage._  |
|  uint8\_t | [**retries**](#variable-retries)  <br>_Slave: retransmissions to slot\_peer in this slot._  |
|  uint8\_t | [**self\_index**](#variable-self_index)  <br>_Index of this node._  |
|  uint8\_t | [**slot\_k**](#variable-slot_k)  <br>_Slave: iteration counter over the other slaves._  |
|  uint32\_t | [**slot\_nack**](#variable-slot_nack)  <br>_Slave: peers that did not acknowledge in this slot._  |
|  uint8\_t | [**slot\_pass**](#variable-slot_pass)  <br>_Slave: 0 = reachable peers, 1 = peers considered dead._  |
|  uint8\_t | [**slot\_peer**](#variable-slot_peer)  <br>_Slave: node index being served._  |
|  uint8\_t | [**slot\_rr**](#variable-slot_rr)  <br>_Slave: round-robin start, advanced every slot._  |
|  uint32\_t | [**slot\_start**](#variable-slot_start)  <br>_Slave: time of the POLL that started the slot._  |
|  uint8\_t | [**state\_data**](#variable-state_data)  <br>_State mailboxes, in list order._  |
|  uint32\_t | [**state\_dirty**](#variable-state_dirty)  <br>_Node mask: still to be sent to._  |
|  uint8\_t | [**state\_length**](#variable-state_length)  <br>_Current length of each state._  |
|  struct [**baton\_stats**](structbaton__stats.md) | [**stats**](#variable-stats)  <br>_Node-level counters._  |
|  uint8\_t | [**tx\_frame**](#variable-tx_frame)  <br>_POLL, RESP or DATA being sent._  |












































## Detailed Description


Treat the members as private; use the functions below. 


    
## Public Attributes Documentation




### variable ack\_frame 

_ACK being sent._ 
```C++
uint8_t baton_node::ack_frame[BATON_FRAME_OVERHEAD];
```




<hr>



### variable channel 

_One channel per node index._ 
```C++
struct baton_channel baton_node::channel[BATON_MAX_NODES];
```




<hr>



### variable config 

_Copy of the configuration._ 
```C++
struct baton_node_config baton_node::config;
```




<hr>



### variable current 

_Master: index of the polled slave - 1._ 
```C++
uint8_t baton_node::current;
```




<hr>



### variable cycle\_count 

_Master: number of cycles started._ 
```C++
uint32_t baton_node::cycle_count;
```




<hr>



### variable cycle\_start 

_Master: start of the current cycle._ 
```C++
uint32_t baton_node::cycle_start;
```




<hr>



### variable initialized 

[_**baton\_node\_init()**_](group__baton__node.md#function-baton_node_init) _succeeded._
```C++
bool baton_node::initialized;
```




<hr>



### variable last\_housekeep\_ms 

_Last TTL scan._ 
```C++
uint32_t baton_node::last_housekeep_ms;
```




<hr>



### variable link 

_Byte-level link._ 
```C++
struct baton_link baton_node::link;
```




<hr>



### variable link\_state 

_Master/slave state machine._ 
```C++
uint8_t baton_node::link_state;
```




<hr>



### variable link\_timer 

_Start of the current wait._ 
```C++
uint32_t baton_node::link_timer;
```




<hr>



### variable node\_address 

_Index 0 = master, 1..n = slaves._ 
```C++
uint8_t baton_node::node_address[BATON_MAX_NODES];
```




<hr>



### variable node\_count 

_Master + slaves._ 
```C++
uint8_t baton_node::node_count;
```




<hr>



### variable now\_ms 

_Time of the last_ [_**baton\_node\_process()**_](group__baton__node.md#function-baton_node_process) _._
```C++
uint32_t baton_node::now_ms;
```




<hr>



### variable others\_mask 

_Node mask of all other nodes._ 
```C++
uint32_t baton_node::others_mask;
```




<hr>



### variable poll\_seq 

_Slave: sequence number of the last POLL._ 
```C++
uint8_t baton_node::poll_seq;
```




<hr>



### variable queue 

_[HIGH], [NORMAL]._ 
```C++
struct baton_queue baton_node::queue[2];
```




<hr>



### variable queue\_high\_memory 

_HIGH queue storage._ 
```C++
uint8_t baton_node::queue_high_memory[BATON_QUEUE_HIGH_SIZE];
```




<hr>



### variable queue\_normal\_memory 

_NORMAL queue storage._ 
```C++
uint8_t baton_node::queue_normal_memory[BATON_QUEUE_NORMAL_SIZE];
```




<hr>



### variable retries 

_Slave: retransmissions to slot\_peer in this slot._ 
```C++
uint8_t baton_node::retries;
```




<hr>



### variable self\_index 

_Index of this node._ 
```C++
uint8_t baton_node::self_index;
```




<hr>



### variable slot\_k 

_Slave: iteration counter over the other slaves._ 
```C++
uint8_t baton_node::slot_k;
```




<hr>



### variable slot\_nack 

_Slave: peers that did not acknowledge in this slot._ 
```C++
uint32_t baton_node::slot_nack;
```




<hr>



### variable slot\_pass 

_Slave: 0 = reachable peers, 1 = peers considered dead._ 
```C++
uint8_t baton_node::slot_pass;
```




<hr>



### variable slot\_peer 

_Slave: node index being served._ 
```C++
uint8_t baton_node::slot_peer;
```




<hr>



### variable slot\_rr 

_Slave: round-robin start, advanced every slot._ 
```C++
uint8_t baton_node::slot_rr;
```




<hr>



### variable slot\_start 

_Slave: time of the POLL that started the slot._ 
```C++
uint32_t baton_node::slot_start;
```




<hr>



### variable state\_data 

_State mailboxes, in list order._ 
```C++
uint8_t baton_node::state_data[BATON_STATE_DATA_SIZE];
```




<hr>



### variable state\_dirty 

_Node mask: still to be sent to._ 
```C++
uint32_t baton_node::state_dirty[BATON_STATE_COUNT+1U];
```




<hr>



### variable state\_length 

_Current length of each state._ 
```C++
uint8_t baton_node::state_length[BATON_STATE_COUNT+1U];
```




<hr>



### variable stats 

_Node-level counters._ 
```C++
struct baton_stats baton_node::stats;
```




<hr>



### variable tx\_frame 

_POLL, RESP or DATA being sent._ 
```C++
uint8_t baton_node::tx_frame[BATON_FRAME_MAX];
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_node.h`

