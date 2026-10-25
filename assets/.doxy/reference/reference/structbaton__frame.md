

# Struct baton\_frame



[**ClassList**](annotated.md) **>** [**baton\_frame**](structbaton__frame.md)



_One decoded frame (header fields and payload, without SYNC and CRC)._ 

* `#include <baton_protocol.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**ctrl**](#variable-ctrl)  <br>_Control byte, see_ [_**baton\_ctrl()**_](group__baton__defs.md#function-baton_ctrl) _._ |
|  uint8\_t | [**dst**](#variable-dst)  <br>_Destination address._  |
|  uint8\_t | [**length**](#variable-length)  <br>_Number of valid bytes in payload._  |
|  uint8\_t | [**payload**](#variable-payload)  <br>_Payload bytes._  |
|  uint8\_t | [**src**](#variable-src)  <br>_Source address._  |












































## Public Attributes Documentation




### variable ctrl 

_Control byte, see_ [_**baton\_ctrl()**_](group__baton__defs.md#function-baton_ctrl) _._
```C++
uint8_t baton_frame::ctrl;
```




<hr>



### variable dst 

_Destination address._ 
```C++
uint8_t baton_frame::dst;
```




<hr>



### variable length 

_Number of valid bytes in payload._ 
```C++
uint8_t baton_frame::length;
```




<hr>



### variable payload 

_Payload bytes._ 
```C++
uint8_t baton_frame::payload[BATON_MAX_PAYLOAD];
```




<hr>



### variable src 

_Source address._ 
```C++
uint8_t baton_frame::src;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_protocol.h`

