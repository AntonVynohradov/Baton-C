

# File baton\_protocol.c



[**FileList**](files.md) **>** [**src**](dir_68267d1309a1af8e8297ef4c3efbcdba.md) **>** [**baton\_protocol.c**](baton__protocol_8c.md)

[Go to the source code of this file](baton__protocol_8c_source.md)



* `#include <string.h>`
* `#include "baton_protocol.h"`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**parse\_crc\_low**](#function-parse_crc_low) (struct [**baton\_parser**](structbaton__parser.md) \* parser, uint8\_t byte) <br>_Last CRC byte: the frame is complete, valid or not._  |
|  int | [**parse\_header**](#function-parse_header) (struct [**baton\_parser**](structbaton__parser.md) \* parser, uint8\_t byte) <br>_Store one of DST, SRC, CTRL, LEN; reject an invalid LEN as soon as it is read._  |
|  int | [**parse\_payload**](#function-parse_payload) (struct [**baton\_parser**](structbaton__parser.md) \* parser, uint8\_t byte) <br>_Store one payload byte; the bound check also catches a corrupted parser structure._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BATON\_CRC\_POLY**](baton__protocol_8c.md#define-baton_crc_poly)  `0x1021U`<br>_CRC-16/CCITT-FALSE polynomial._  |
| define  | [**PARSE\_CRC\_HIGH**](baton__protocol_8c.md#define-parse_crc_high)  `3U`<br> |
| define  | [**PARSE\_CRC\_LOW**](baton__protocol_8c.md#define-parse_crc_low)  `4U`<br> |
| define  | [**PARSE\_HEADER**](baton__protocol_8c.md#define-parse_header)  `1U`<br> |
| define  | [**PARSE\_HEADER\_BYTES**](baton__protocol_8c.md#define-parse_header_bytes)  `4U`<br>_Header bytes after SYNC: DST, SRC, CTRL, LEN._  |
| define  | [**PARSE\_PAYLOAD**](baton__protocol_8c.md#define-parse_payload)  `2U`<br> |
| define  | [**PARSE\_SYNC**](baton__protocol_8c.md#define-parse_sync)  `[**BATON\_PARSER\_IDLE**](group__baton__protocol.md#define-baton_parser_idle)`<br>_Parser states._  |

## Public Static Functions Documentation




### function parse\_crc\_low 

_Last CRC byte: the frame is complete, valid or not._ 
```C++
static int parse_crc_low (
    struct baton_parser * parser,
    uint8_t byte
) 
```




<hr>



### function parse\_header 

_Store one of DST, SRC, CTRL, LEN; reject an invalid LEN as soon as it is read._ 
```C++
static int parse_header (
    struct baton_parser * parser,
    uint8_t byte
) 
```




<hr>



### function parse\_payload 

_Store one payload byte; the bound check also catches a corrupted parser structure._ 
```C++
static int parse_payload (
    struct baton_parser * parser,
    uint8_t byte
) 
```




<hr>
## Macro Definition Documentation





### define BATON\_CRC\_POLY 

_CRC-16/CCITT-FALSE polynomial._ 
```C++
#define BATON_CRC_POLY `0x1021U`
```




<hr>



### define PARSE\_CRC\_HIGH 

```C++
#define PARSE_CRC_HIGH `3U`
```




<hr>



### define PARSE\_CRC\_LOW 

```C++
#define PARSE_CRC_LOW `4U`
```




<hr>



### define PARSE\_HEADER 

```C++
#define PARSE_HEADER `1U`
```




<hr>



### define PARSE\_HEADER\_BYTES 

_Header bytes after SYNC: DST, SRC, CTRL, LEN._ 
```C++
#define PARSE_HEADER_BYTES `4U`
```




<hr>



### define PARSE\_PAYLOAD 

```C++
#define PARSE_PAYLOAD `2U`
```




<hr>



### define PARSE\_SYNC 

_Parser states._ 
```C++
#define PARSE_SYNC `BATON_PARSER_IDLE`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `src/baton_protocol.c`

