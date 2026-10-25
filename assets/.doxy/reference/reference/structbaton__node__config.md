

# Struct baton\_node\_config



[**ClassList**](annotated.md) **>** [**baton\_node\_config**](structbaton__node__config.md)



_Node configuration passed to_ [_**baton\_node\_init**_](group__baton__node.md#function-baton_node_init) _; copied._[More...](#detailed-description)

* `#include <baton_node.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**ack\_timeout\_ms**](#variable-ack_timeout_ms)  <br>_Wait for an ACK after a DATA frame._  |
|  uint32\_t | [**baudrate**](#variable-baudrate)  <br>_Bit rate, used to compute frame durations._  |
|  void \* | [**context**](#variable-context)  <br>_Passed to every hook and callback._  |
|  uint16\_t | [**cycle\_period\_ms**](#variable-cycle_period_ms)  <br>_Master: shortest time between cycle starts (0 = back to back)._  |
|  [**baton\_critical\_fn**](group__baton__node.md#typedef-baton_critical_fn) | [**enter\_critical**](#variable-enter_critical)  <br>_Disable the UART interrupt (or all interrupts)._  |
|  uint32\_t | [**event\_ttl\_ms**](#variable-event_ttl_ms)  <br>_An undelivered event is dropped after this (0 = never)._  |
|  [**baton\_critical\_fn**](group__baton__node.md#typedef-baton_critical_fn) | [**exit\_critical**](#variable-exit_critical)  <br>_Restore it._  |
|  uint8\_t | [**inter\_byte\_timeout\_ms**](#variable-inter_byte_timeout_ms)  <br>_Silence that aborts a partially received frame._  |
|  uint8\_t | [**master\_address**](#variable-master_address)  <br>_Address of the master._  |
|  uint8\_t | [**max\_retries**](#variable-max_retries)  <br>_DATA retransmissions per slot (0 = none)._  |
|  [**baton\_drop\_fn**](group__baton__node.md#typedef-baton_drop_fn) | [**on\_drop**](#variable-on_drop)  <br>_An event expired._  |
|  [**baton\_message\_fn**](group__baton__node.md#typedef-baton_message_fn) | [**on\_message**](#variable-on_message)  <br>_A message was received._  |
|  [**baton\_poll\_done\_fn**](group__baton__node.md#typedef-baton_poll_done_fn) | [**on\_poll\_done**](#variable-on_poll_done)  <br>_Master: a slot finished._  |
|  uint8\_t | [**own\_address**](#variable-own_address)  <br>_Address of this node (master: master\_address)._  |
|  uint8\_t | [**role**](#variable-role)  <br>_BATON\_ROLE\_MASTER or BATON\_ROLE\_SLAVE._  |
|  bool | [**rx\_echo**](#variable-rx_echo)  <br>_true: the UART receives its own bytes (single wire); false: call_ [_**baton\_node\_tx\_done\_isr()**_](group__baton__node.md#function-baton_node_tx_done_isr) _._ |
|  const uint8\_t \* | [**slave\_addresses**](#variable-slave_addresses)  <br>_Slaves in polling order, identical on every node._  |
|  uint8\_t | [**slave\_count**](#variable-slave_count)  <br>_1..BATON\_MAX\_NODES - 1._  |
|  uint16\_t | [**slot\_time\_ms**](#variable-slot_time_ms)  <br>_Longest time a slave owns the bus after a POLL._  |
|  uint8\_t | [**tx\_gap\_ms**](#variable-tx_gap_ms)  <br>_Bus silence required before transmitting._  |
|  [**baton\_write\_fn**](group__baton__link.md#typedef-baton_write_fn) | [**write**](#variable-write)  <br>_Starts a non-blocking transmission (required)._  |












































## Detailed Description


Node mask convention: bit 0 = master, bit i (i &gt;= 1) = slave\_addresses[i - 1]. 


    
## Public Attributes Documentation




### variable ack\_timeout\_ms 

_Wait for an ACK after a DATA frame._ 
```C++
uint16_t baton_node_config::ack_timeout_ms;
```




<hr>



### variable baudrate 

_Bit rate, used to compute frame durations._ 
```C++
uint32_t baton_node_config::baudrate;
```




<hr>



### variable context 

_Passed to every hook and callback._ 
```C++
void* baton_node_config::context;
```




<hr>



### variable cycle\_period\_ms 

_Master: shortest time between cycle starts (0 = back to back)._ 
```C++
uint16_t baton_node_config::cycle_period_ms;
```




<hr>



### variable enter\_critical 

_Disable the UART interrupt (or all interrupts)._ 
```C++
baton_critical_fn baton_node_config::enter_critical;
```




<hr>



### variable event\_ttl\_ms 

_An undelivered event is dropped after this (0 = never)._ 
```C++
uint32_t baton_node_config::event_ttl_ms;
```




<hr>



### variable exit\_critical 

_Restore it._ 
```C++
baton_critical_fn baton_node_config::exit_critical;
```




<hr>



### variable inter\_byte\_timeout\_ms 

_Silence that aborts a partially received frame._ 
```C++
uint8_t baton_node_config::inter_byte_timeout_ms;
```




<hr>



### variable master\_address 

_Address of the master._ 
```C++
uint8_t baton_node_config::master_address;
```




<hr>



### variable max\_retries 

_DATA retransmissions per slot (0 = none)._ 
```C++
uint8_t baton_node_config::max_retries;
```




<hr>



### variable on\_drop 

_An event expired._ 
```C++
baton_drop_fn baton_node_config::on_drop;
```




<hr>



### variable on\_message 

_A message was received._ 
```C++
baton_message_fn baton_node_config::on_message;
```




<hr>



### variable on\_poll\_done 

_Master: a slot finished._ 
```C++
baton_poll_done_fn baton_node_config::on_poll_done;
```




<hr>



### variable own\_address 

_Address of this node (master: master\_address)._ 
```C++
uint8_t baton_node_config::own_address;
```




<hr>



### variable role 

_BATON\_ROLE\_MASTER or BATON\_ROLE\_SLAVE._ 
```C++
uint8_t baton_node_config::role;
```




<hr>



### variable rx\_echo 

_true: the UART receives its own bytes (single wire); false: call_ [_**baton\_node\_tx\_done\_isr()**_](group__baton__node.md#function-baton_node_tx_done_isr) _._
```C++
bool baton_node_config::rx_echo;
```




<hr>



### variable slave\_addresses 

_Slaves in polling order, identical on every node._ 
```C++
const uint8_t* baton_node_config::slave_addresses;
```




<hr>



### variable slave\_count 

_1..BATON\_MAX\_NODES - 1._ 
```C++
uint8_t baton_node_config::slave_count;
```




<hr>



### variable slot\_time\_ms 

_Longest time a slave owns the bus after a POLL._ 
```C++
uint16_t baton_node_config::slot_time_ms;
```




<hr>



### variable tx\_gap\_ms 

_Bus silence required before transmitting._ 
```C++
uint8_t baton_node_config::tx_gap_ms;
```




<hr>



### variable write 

_Starts a non-blocking transmission (required)._ 
```C++
baton_write_fn baton_node_config::write;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_node.h`

