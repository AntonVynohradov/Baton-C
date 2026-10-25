

# Group baton\_defs



[**Modules**](modules.md) **>** [**baton\_defs**](group__baton__defs.md)



_Frame layout, frame types, payload prefixes and byte-order helpers._ [More...](#detailed-description)








































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**baton\_ctrl**](#function-baton_ctrl) (uint8\_t type, bool first, uint8\_t seq) <br>_Build a control byte._  |
|  bool | [**baton\_ctrl\_first**](#function-baton_ctrl_first) (uint8\_t ctrl) <br>_FIRST flag of a control byte._  |
|  uint8\_t | [**baton\_ctrl\_seq**](#function-baton_ctrl_seq) (uint8\_t ctrl) <br>_Sequence number of a control byte._  |
|  uint8\_t | [**baton\_ctrl\_type**](#function-baton_ctrl_type) (uint8\_t ctrl) <br>_Frame type of a control byte._  |
|  uint16\_t | [**baton\_get\_u16be**](#function-baton_get_u16be) (const uint8\_t \* data) <br>_Read a big-endian 16-bit value._  |
|  uint32\_t | [**baton\_get\_u32le**](#function-baton_get_u32le) (const uint8\_t \* data) <br>_Read a little-endian 32-bit value._  |
|  void | [**baton\_put\_u16be**](#function-baton_put_u16be) (uint8\_t \* data, uint16\_t value) <br>_Write a 16-bit value in big-endian order._  |
|  void | [**baton\_put\_u32le**](#function-baton_put_u32le) (uint8\_t \* data, uint32\_t value) <br>_Write a 32-bit value in little-endian order._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BATON\_ACKB\_VALID**](group__baton__defs.md#define-baton_ackb_valid)  `0x80U`<br>_ACKB byte: bit 7 = valid, bits 3..0 = acknowledged sequence number._  |
| define  | [**BATON\_ADDR\_BROADCAST**](group__baton__defs.md#define-baton_addr_broadcast)  `0xFFU`<br>_Destination address meaning "every other node on the bus"._  |
| define  | [**BATON\_CRC\_SIZE**](group__baton__defs.md#define-baton_crc_size)  `2U`<br>_CRC-16, transmitted high byte first._  |
| define  | [**BATON\_FRAME\_OVERHEAD**](group__baton__defs.md#define-baton_frame_overhead)  `([**BATON\_HEADER\_SIZE**](group__baton__defs.md#define-baton_header_size) + [**BATON\_CRC\_SIZE**](group__baton__defs.md#define-baton_crc_size))`<br>_Bytes of a frame that are not payload._  |
| define  | [**BATON\_FT\_ACK**](group__baton__defs.md#define-baton_ft_ack)  `3U`<br>_Slave to slave: acknowledges one DATA frame._  |
| define  | [**BATON\_FT\_DATA**](group__baton__defs.md#define-baton_ft_data)  `2U`<br>_Slave to slave: messages, acknowledged with ACK._  |
| define  | [**BATON\_FT\_POLL**](group__baton__defs.md#define-baton_ft_poll)  `0U`<br>_Master to slave: grants the bus for one slot, may carry messages._  |
| define  | [**BATON\_FT\_RESP**](group__baton__defs.md#define-baton_ft_resp)  `1U`<br>_Slave to master: ends the slot, may carry messages._  |
| define  | [**BATON\_HEADER\_SIZE**](group__baton__defs.md#define-baton_header_size)  `5U`<br>_SYNC, DST, SRC, CTRL and LEN._  |
| define  | [**BATON\_SEQ\_MASK**](group__baton__defs.md#define-baton_seq_mask)  `0x0FU`<br>_Mask of the sequence number in CTRL and ACKB._  |
| define  | [**BATON\_SYNC**](group__baton__defs.md#define-baton_sync)  `0xA5U`<br>_First byte of every frame._  |
| define  | [**BATON\_TLV\_HEADER\_SIZE**](group__baton__defs.md#define-baton_tlv_header_size)  `2U`<br>_One message inside a payload: id, length, data (TLV)._  |

## Detailed Description


The wire format is described in `docs/protocol.md`: `SYNC | DST | SRC | CTRL | LEN | PAYLOAD[LEN] | CRC16 (big endian)`, CRC-16/CCITT-FALSE over DST..PAYLOAD, `CTRL = TYPE[7:6] | FIRST[5] | reserved[4] | SEQ[3:0]`. 


    
## Public Static Functions Documentation




### function baton\_ctrl 

_Build a control byte._ 
```
static inline uint8_t baton_ctrl (
    uint8_t type,
    bool first,
    uint8_t seq
) 
```





**Parameters:**


* `type` Frame type (BATON\_FT\_\*), 0..3. 
* `first` FIRST flag: set until the receiver acknowledged a frame after a restart. 
* `seq` Sequence number, only bits 3..0 are used. 



**Returns:**

Control byte. 





        

<hr>



### function baton\_ctrl\_first 

_FIRST flag of a control byte._ 
```
static inline bool baton_ctrl_first (
    uint8_t ctrl
) 
```




<hr>



### function baton\_ctrl\_seq 

_Sequence number of a control byte._ 
```
static inline uint8_t baton_ctrl_seq (
    uint8_t ctrl
) 
```




<hr>



### function baton\_ctrl\_type 

_Frame type of a control byte._ 
```
static inline uint8_t baton_ctrl_type (
    uint8_t ctrl
) 
```




<hr>



### function baton\_get\_u16be 

_Read a big-endian 16-bit value._ 
```
static inline uint16_t baton_get_u16be (
    const uint8_t * data
) 
```




<hr>



### function baton\_get\_u32le 

_Read a little-endian 32-bit value._ 
```
static inline uint32_t baton_get_u32le (
    const uint8_t * data
) 
```




<hr>



### function baton\_put\_u16be 

_Write a 16-bit value in big-endian order._ 
```
static inline void baton_put_u16be (
    uint8_t * data,
    uint16_t value
) 
```




<hr>



### function baton\_put\_u32le 

_Write a 32-bit value in little-endian order._ 
```
static inline void baton_put_u32le (
    uint8_t * data,
    uint32_t value
) 
```




<hr>
## Macro Definition Documentation





### define BATON\_ACKB\_VALID 

_ACKB byte: bit 7 = valid, bits 3..0 = acknowledged sequence number._ 
```
#define BATON_ACKB_VALID `0x80U`
```




<hr>



### define BATON\_ADDR\_BROADCAST 

_Destination address meaning "every other node on the bus"._ 
```
#define BATON_ADDR_BROADCAST `0xFFU`
```




<hr>



### define BATON\_CRC\_SIZE 

_CRC-16, transmitted high byte first._ 
```
#define BATON_CRC_SIZE `2U`
```




<hr>



### define BATON\_FRAME\_OVERHEAD 

_Bytes of a frame that are not payload._ 
```
#define BATON_FRAME_OVERHEAD `( BATON_HEADER_SIZE + BATON_CRC_SIZE )`
```




<hr>



### define BATON\_FT\_ACK 

_Slave to slave: acknowledges one DATA frame._ 
```
#define BATON_FT_ACK `3U`
```




<hr>



### define BATON\_FT\_DATA 

_Slave to slave: messages, acknowledged with ACK._ 
```
#define BATON_FT_DATA `2U`
```




<hr>



### define BATON\_FT\_POLL 

_Master to slave: grants the bus for one slot, may carry messages._ 
```
#define BATON_FT_POLL `0U`
```




<hr>



### define BATON\_FT\_RESP 

_Slave to master: ends the slot, may carry messages._ 
```
#define BATON_FT_RESP `1U`
```




<hr>



### define BATON\_HEADER\_SIZE 

_SYNC, DST, SRC, CTRL and LEN._ 
```
#define BATON_HEADER_SIZE `5U`
```




<hr>



### define BATON\_SEQ\_MASK 

_Mask of the sequence number in CTRL and ACKB._ 
```
#define BATON_SEQ_MASK `0x0FU`
```




<hr>



### define BATON\_SYNC 

_First byte of every frame._ 
```
#define BATON_SYNC `0xA5U`
```




<hr>



### define BATON\_TLV\_HEADER\_SIZE 

_One message inside a payload: id, length, data (TLV)._ 
```
#define BATON_TLV_HEADER_SIZE `2U`
```




<hr>

------------------------------


