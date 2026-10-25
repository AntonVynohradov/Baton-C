

# Struct baton\_link\_config



[**ClassList**](annotated.md) **>** [**baton\_link\_config**](structbaton__link__config.md)



_Link configuration passed to_ [_**baton\_link\_init**_](group__baton__link.md#function-baton_link_init) _._

* `#include <baton_link.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**baudrate**](#variable-baudrate)  <br>_Bit rate; frame durations assume 8N1 (10 bits per byte)._  |
|  void \* | [**context**](#variable-context)  <br>_Passed to write._  |
|  uint8\_t | [**inter\_byte\_timeout\_ms**](#variable-inter_byte_timeout_ms)  <br>_Silence that aborts a partially received frame (&gt; 0)._  |
|  bool | [**rx\_echo**](#variable-rx_echo)  <br>_true: the UART receives its own bytes (single wire)._  |
|  uint8\_t | [**tx\_gap\_ms**](#variable-tx_gap_ms)  <br>_Bus silence required before transmitting._  |
|  [**baton\_write\_fn**](group__baton__link.md#typedef-baton_write_fn) | [**write**](#variable-write)  <br>_Starts a transmission (required)._  |












































## Public Attributes Documentation




### variable baudrate 

_Bit rate; frame durations assume 8N1 (10 bits per byte)._ 
```C++
uint32_t baton_link_config::baudrate;
```




<hr>



### variable context 

_Passed to write._ 
```C++
void* baton_link_config::context;
```




<hr>



### variable inter\_byte\_timeout\_ms 

_Silence that aborts a partially received frame (&gt; 0)._ 
```C++
uint8_t baton_link_config::inter_byte_timeout_ms;
```




<hr>



### variable rx\_echo 

_true: the UART receives its own bytes (single wire)._ 
```C++
bool baton_link_config::rx_echo;
```




<hr>



### variable tx\_gap\_ms 

_Bus silence required before transmitting._ 
```C++
uint8_t baton_link_config::tx_gap_ms;
```




<hr>



### variable write 

_Starts a transmission (required)._ 
```C++
baton_write_fn baton_link_config::write;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_link.h`

