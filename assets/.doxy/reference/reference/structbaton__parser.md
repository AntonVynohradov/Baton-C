

# Struct baton\_parser



[**ClassList**](annotated.md) **>** [**baton\_parser**](structbaton__parser.md)



_Incremental parser state for one byte stream._ [More...](#detailed-description)

* `#include <baton_protocol.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**crc**](#variable-crc)  <br>_Running CRC over DST..PAYLOAD._  |
|  struct [**baton\_frame**](structbaton__frame.md) | [**frame**](#variable-frame)  <br>_Frame being received; valid after feed returned 1._  |
|  uint16\_t | [**index**](#variable-index)  <br>_Bytes of the current field already received._  |
|  uint8\_t | [**state**](#variable-state)  <br>_Internal state; BATON\_PARSER\_IDLE between frames._  |












































## Detailed Description


The parser decodes directly into [**baton\_parser::frame**](structbaton__parser.md#variable-frame), so a decoded frame needs no second buffer. Keep one instance per input stream and initialize it before feeding bytes. 


    
## Public Attributes Documentation




### variable crc 

_Running CRC over DST..PAYLOAD._ 
```C++
uint16_t baton_parser::crc;
```




<hr>



### variable frame 

_Frame being received; valid after feed returned 1._ 
```C++
struct baton_frame baton_parser::frame;
```




<hr>



### variable index 

_Bytes of the current field already received._ 
```C++
uint16_t baton_parser::index;
```




<hr>



### variable state 

_Internal state; BATON\_PARSER\_IDLE between frames._ 
```C++
uint8_t baton_parser::state;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_protocol.h`

