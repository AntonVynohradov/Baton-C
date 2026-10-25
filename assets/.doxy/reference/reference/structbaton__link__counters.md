

# Struct baton\_link\_counters



[**ClassList**](annotated.md) **>** [**baton\_link\_counters**](structbaton__link__counters.md)



_Link diagnostic counters._ 

* `#include <baton_link.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**collisions**](#variable-collisions)  <br>_Transmissions whose echo did not match._  |
|  uint32\_t | [**crc\_errors**](#variable-crc_errors)  <br>_Frames rejected: CRC mismatch or invalid LEN._  |
|  uint32\_t | [**rx\_frames**](#variable-rx_frames)  <br>_Valid frames received (any destination)._  |
|  uint32\_t | [**rx\_timeouts**](#variable-rx_timeouts)  <br>_Frames aborted by the inter-byte timeout._  |
|  uint32\_t | [**tx\_frames**](#variable-tx_frames)  <br>_Transmissions started._  |
|  uint32\_t | [**tx\_timeouts**](#variable-tx_timeouts)  <br>_Transmissions that failed to start or did not finish in time._  |












































## Public Attributes Documentation




### variable collisions 

_Transmissions whose echo did not match._ 
```C++
uint32_t baton_link_counters::collisions;
```




<hr>



### variable crc\_errors 

_Frames rejected: CRC mismatch or invalid LEN._ 
```C++
uint32_t baton_link_counters::crc_errors;
```




<hr>



### variable rx\_frames 

_Valid frames received (any destination)._ 
```C++
uint32_t baton_link_counters::rx_frames;
```




<hr>



### variable rx\_timeouts 

_Frames aborted by the inter-byte timeout._ 
```C++
uint32_t baton_link_counters::rx_timeouts;
```




<hr>



### variable tx\_frames 

_Transmissions started._ 
```C++
uint32_t baton_link_counters::tx_frames;
```




<hr>



### variable tx\_timeouts 

_Transmissions that failed to start or did not finish in time._ 
```C++
uint32_t baton_link_counters::tx_timeouts;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_link.h`

