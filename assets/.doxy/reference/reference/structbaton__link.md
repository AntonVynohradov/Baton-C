

# Struct baton\_link



[**ClassList**](annotated.md) **>** [**baton\_link**](structbaton__link.md)



_Link state._ [More...](#detailed-description)

* `#include <baton_link.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  struct [**baton\_link\_config**](structbaton__link__config.md) | [**config**](#variable-config)  <br>_Copy of the configuration._  |
|  struct [**baton\_link\_counters**](structbaton__link__counters.md) | [**counters**](#variable-counters)  <br>_Diagnostic counters._  |
|  bool | [**echo\_error**](#variable-echo_error)  <br>_An echo byte did not match._  |
|  uint16\_t | [**echo\_index**](#variable-echo_index)  <br>_Echo bytes already compared._  |
|  uint32\_t | [**last\_rx\_ms**](#variable-last_rx_ms)  <br>_Last byte received from another node (fed to the parser)._  |
|  uint32\_t | [**last\_tx\_end\_ms**](#variable-last_tx_end_ms)  <br>_End of the last transmission._  |
|  struct [**baton\_parser**](structbaton__parser.md) | [**parser**](#variable-parser)  <br>_Receive stream parser._  |
|  volatile uint8\_t | [**rx\_buffer**](#variable-rx_buffer)  <br>_Receive ring (interrupt producer)._  |
|  volatile uint16\_t | [**rx\_head**](#variable-rx_head)  <br>_Next write position (interrupt)._  |
|  volatile uint32\_t | [**rx\_overflows**](#variable-rx_overflows)  <br>_Bytes lost because the ring was full._  |
|  volatile uint16\_t | [**rx\_tail**](#variable-rx_tail)  <br>_Next read position (process)._  |
|  const uint8\_t \* | [**tx\_data**](#variable-tx_data)  <br>_Frame being transmitted._  |
|  volatile bool | [**tx\_done\_flag**](#variable-tx_done_flag)  <br>_Set by_ [_**baton\_link\_tx\_done\_isr()**_](group__baton__link.md#function-baton_link_tx_done_isr) _._ |
|  uint16\_t | [**tx\_length**](#variable-tx_length)  <br>_Length of tx\_data._  |
|  uint32\_t | [**tx\_start\_ms**](#variable-tx_start_ms)  <br>_Start of the current transmission._  |
|  uint8\_t | [**tx\_state**](#variable-tx_state)  <br>_Idle, waiting for a silent bus, or busy._  |












































## Detailed Description


Treat the members as private; use the functions below. 


    
## Public Attributes Documentation




### variable config 

_Copy of the configuration._ 
```C++
struct baton_link_config baton_link::config;
```




<hr>



### variable counters 

_Diagnostic counters._ 
```C++
struct baton_link_counters baton_link::counters;
```




<hr>



### variable echo\_error 

_An echo byte did not match._ 
```C++
bool baton_link::echo_error;
```




<hr>



### variable echo\_index 

_Echo bytes already compared._ 
```C++
uint16_t baton_link::echo_index;
```




<hr>



### variable last\_rx\_ms 

_Last byte received from another node (fed to the parser)._ 
```C++
uint32_t baton_link::last_rx_ms;
```




<hr>



### variable last\_tx\_end\_ms 

_End of the last transmission._ 
```C++
uint32_t baton_link::last_tx_end_ms;
```




<hr>



### variable parser 

_Receive stream parser._ 
```C++
struct baton_parser baton_link::parser;
```




<hr>



### variable rx\_buffer 

_Receive ring (interrupt producer)._ 
```C++
volatile uint8_t baton_link::rx_buffer[BATON_RX_BUF_SIZE];
```




<hr>



### variable rx\_head 

_Next write position (interrupt)._ 
```C++
volatile uint16_t baton_link::rx_head;
```




<hr>



### variable rx\_overflows 

_Bytes lost because the ring was full._ 
```C++
volatile uint32_t baton_link::rx_overflows;
```




<hr>



### variable rx\_tail 

_Next read position (process)._ 
```C++
volatile uint16_t baton_link::rx_tail;
```




<hr>



### variable tx\_data 

_Frame being transmitted._ 
```C++
const uint8_t* baton_link::tx_data;
```




<hr>



### variable tx\_done\_flag 

_Set by_ [_**baton\_link\_tx\_done\_isr()**_](group__baton__link.md#function-baton_link_tx_done_isr) _._
```C++
volatile bool baton_link::tx_done_flag;
```




<hr>



### variable tx\_length 

_Length of tx\_data._ 
```C++
uint16_t baton_link::tx_length;
```




<hr>



### variable tx\_start\_ms 

_Start of the current transmission._ 
```C++
uint32_t baton_link::tx_start_ms;
```




<hr>



### variable tx\_state 

_Idle, waiting for a silent bus, or busy._ 
```C++
uint8_t baton_link::tx_state;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_link.h`

