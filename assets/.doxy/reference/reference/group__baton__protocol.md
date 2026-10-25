

# Group baton\_protocol



[**Modules**](modules.md) **>** [**baton\_protocol**](group__baton__protocol.md)



_CRC-16, incremental stream parser and frame encoder._ [More...](#detailed-description)
















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**baton\_frame**](structbaton__frame.md) <br>_One decoded frame (header fields and payload, without SYNC and CRC)._  |
| struct | [**baton\_parser**](structbaton__parser.md) <br>_Incremental parser state for one byte stream._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**baton\_crc16**](#function-baton_crc16) (const uint8\_t \* data, size\_t length) <br>_Calculate the CRC-16/CCITT-FALSE of a buffer (poly 0x1021, init 0xFFFF)._  |
|  uint16\_t | [**baton\_crc16\_update**](#function-baton_crc16_update) (uint16\_t crc, uint8\_t byte) <br>_Add one byte to a CRC-16/CCITT-FALSE register._  |
|  int | [**baton\_encode**](#function-baton_encode) (uint8\_t dst, uint8\_t src, uint8\_t ctrl, const uint8\_t \* payload, size\_t payload\_length, uint8\_t \* output, size\_t output\_capacity, size\_t \* output\_length) <br>_Encode one frame: SYNC, header, payload and CRC._  |
|  int | [**baton\_parser\_feed**](#function-baton_parser_feed) (struct [**baton\_parser**](structbaton__parser.md) \* parser, uint8\_t byte) <br>_Feed one byte to the incremental frame parser._  |
|  void | [**baton\_parser\_init**](#function-baton_parser_init) (struct [**baton\_parser**](structbaton__parser.md) \* parser) <br>_Reset a parser before starting or restarting a byte stream._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BATON\_CRC\_INIT**](group__baton__protocol.md#define-baton_crc_init)  `0xFFFFU`<br>_Initial value of the CRC-16/CCITT-FALSE register._  |
| define  | [**BATON\_FRAME\_MAX**](group__baton__protocol.md#define-baton_frame_max)  `([**BATON\_FRAME\_OVERHEAD**](group__baton__defs.md#define-baton_frame_overhead) + [**BATON\_MAX\_PAYLOAD**](group__baton__config.md#define-baton_max_payload))`<br>_Largest complete frame on the wire, SYNC to CRC._  |
| define  | [**BATON\_PARSER\_IDLE**](group__baton__protocol.md#define-baton_parser_idle)  `0U`<br>_Value of_ [_**baton\_parser::state**_](structbaton__parser.md#variable-state) _while the parser waits for SYNC (between frames)._ |
| define  | [**BATON\_PARSE\_ERROR**](group__baton__protocol.md#define-baton_parse_error)  `(-1)`<br>_Error returned for invalid input, malformed frames, or insufficient capacity._  |

## Detailed Description


Pure functions over caller-owned buffers: no state outside the structures passed in, safe to use from several contexts on different structures.




**
**


* CRC: [**baton\_crc16**](group__baton__protocol.md#function-baton_crc16), [**baton\_crc16\_update**](group__baton__protocol.md#function-baton_crc16_update). 
* Parser: [**baton\_parser\_init**](group__baton__protocol.md#function-baton_parser_init), [**baton\_parser\_feed**](group__baton__protocol.md#function-baton_parser_feed). 
* Encoder: [**baton\_encode**](group__baton__protocol.md#function-baton_encode). 







    
## Public Functions Documentation




### function baton\_crc16 

_Calculate the CRC-16/CCITT-FALSE of a buffer (poly 0x1021, init 0xFFFF)._ 
```
uint16_t baton_crc16 (
    const uint8_t * data,
    size_t length
) 
```





**Parameters:**


* `data` Bytes covered by the CRC. 
* `length` Number of bytes to process. 



**Returns:**

Calculated CRC; BATON\_CRC\_INIT when `data` is NULL. 





        

<hr>



### function baton\_crc16\_update 

_Add one byte to a CRC-16/CCITT-FALSE register._ 
```
uint16_t baton_crc16_update (
    uint16_t crc,
    uint8_t byte
) 
```





**Parameters:**


* `crc` Current register value (BATON\_CRC\_INIT before the first byte). 
* `byte` Next byte. 



**Returns:**

Updated register value. 





        

<hr>



### function baton\_encode 

_Encode one frame: SYNC, header, payload and CRC._ 
```
int baton_encode (
    uint8_t dst,
    uint8_t src,
    uint8_t ctrl,
    const uint8_t * payload,
    size_t payload_length,
    uint8_t * output,
    size_t output_capacity,
    size_t * output_length
) 
```



`payload` may point to `&output[BATON_HEADER_SIZE]`: a payload that was built in place is then not copied.




**Parameters:**


* `dst` Destination address. 
* `src` Source address. 
* `ctrl` Control byte, see [**baton\_ctrl()**](group__baton__defs.md#function-baton_ctrl). 
* `payload` Payload bytes, or NULL when `payload_length` is zero. 
* `payload_length` Number of payload bytes (at most BATON\_MAX\_PAYLOAD). 
* `output` Destination buffer for the complete frame. 
* `output_capacity` Capacity of `output` in bytes. 
* `output_length` Receives the frame length on success. 



**Return value:**


* `0` Frame encoded. 
* `BATON_PARSE_ERROR` Invalid input or insufficient output capacity. 




        

<hr>



### function baton\_parser\_feed 

_Feed one byte to the incremental frame parser._ 
```
int baton_parser_feed (
    struct baton_parser * parser,
    uint8_t byte
) 
```



The parser waits for SYNC, reads the header, rejects a LEN above BATON\_MAX\_PAYLOAD as soon as it is read, then reads the payload and the CRC and validates it. After a valid or invalid frame it waits for the next SYNC. Bytes of a rejected frame are not re-scanned for SYNC; the inter-byte timeout of the link resynchronizes a broken stream.




**Parameters:**


* `parser` Parser state. 
* `byte` Next byte from the input stream. 



**Return value:**


* `1` A complete valid frame is available in `parser->frame` until the next call. 
* `0` More bytes are required. 
* `BATON_PARSE_ERROR` NULL parser, LEN out of range, CRC mismatch or corrupted state. 




        

<hr>



### function baton\_parser\_init 

_Reset a parser before starting or restarting a byte stream._ 
```
void baton_parser_init (
    struct baton_parser * parser
) 
```





**Parameters:**


* `parser` Parser state to initialize; NULL is ignored. 




        

<hr>
## Macro Definition Documentation





### define BATON\_CRC\_INIT 

_Initial value of the CRC-16/CCITT-FALSE register._ 
```
#define BATON_CRC_INIT `0xFFFFU`
```




<hr>



### define BATON\_FRAME\_MAX 

_Largest complete frame on the wire, SYNC to CRC._ 
```
#define BATON_FRAME_MAX `( BATON_FRAME_OVERHEAD + BATON_MAX_PAYLOAD )`
```




<hr>



### define BATON\_PARSER\_IDLE 

_Value of_ [_**baton\_parser::state**_](structbaton__parser.md#variable-state) _while the parser waits for SYNC (between frames)._
```
#define BATON_PARSER_IDLE `0U`
```




<hr>



### define BATON\_PARSE\_ERROR 

_Error returned for invalid input, malformed frames, or insufficient capacity._ 
```
#define BATON_PARSE_ERROR `(-1)`
```




<hr>

------------------------------


