

# Struct baton\_queue



[**ClassList**](annotated.md) **>** [**baton\_queue**](structbaton__queue.md)



_Queue state._ 

* `#include <baton_queue.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**head**](#variable-head)  <br>_Oldest record (consumer)._  |
|  uint8\_t \* | [**memory**](#variable-memory)  <br>_Record storage, provided by the owner._  |
|  uint16\_t | [**size**](#variable-size)  <br>_Usable bytes, multiple of 4._  |
|  uint16\_t | [**tail**](#variable-tail)  <br>_Next free byte (producer)._  |












































## Public Attributes Documentation




### variable head 

_Oldest record (consumer)._ 
```C++
uint16_t baton_queue::head;
```




<hr>



### variable memory 

_Record storage, provided by the owner._ 
```C++
uint8_t* baton_queue::memory;
```




<hr>



### variable size 

_Usable bytes, multiple of 4._ 
```C++
uint16_t baton_queue::size;
```




<hr>



### variable tail 

_Next free byte (producer)._ 
```C++
uint16_t baton_queue::tail;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `inc/baton_queue.h`

