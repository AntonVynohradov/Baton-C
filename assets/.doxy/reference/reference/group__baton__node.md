

# Group baton\_node



[**Modules**](modules.md) **>** [**baton\_node**](group__baton__node.md)



_One bus node (master or slave) on top of the_ [_**Link**_](group__baton__link.md) _._[More...](#detailed-description)
















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**baton\_channel**](structbaton__channel.md) <br>_Stop-and-wait state of the channel to one peer, both directions._  |
| struct | [**baton\_node**](structbaton__node.md) <br>_Node state._  |
| struct | [**baton\_node\_config**](structbaton__node__config.md) <br>_Node configuration passed to_ [_**baton\_node\_init**_](group__baton__node.md#function-baton_node_init) _; copied._ |
| struct | [**baton\_stats**](structbaton__stats.md) <br>_Diagnostic counters, see_ [_**baton\_node\_get\_stats()**_](group__baton__node.md#function-baton_node_get_stats) _._ |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef void(\* | [**baton\_critical\_fn**](#typedef-baton_critical_fn)  <br>_Enter or leave the critical section shared with interrupts._  |
| typedef void(\* | [**baton\_drop\_fn**](#typedef-baton_drop_fn)  <br>_An event was not delivered within event\_ttl\_ms._  |
| typedef void(\* | [**baton\_message\_fn**](#typedef-baton_message_fn)  <br>_A message (state or event) was received._  |
| enum  | [**baton\_msg\_id**](#enum-baton_msg_id)  <br>_Message identifiers: BATON\_MSG\_&lt;NAME&gt;._  |
| enum  | [**baton\_msg\_max\_len**](#enum-baton_msg_max_len)  <br>_Maximum data lengths: BATON\_MSG\_&lt;NAME&gt;\_MAX\_LEN._  |
| typedef void(\* | [**baton\_poll\_done\_fn**](#typedef-baton_poll_done_fn)  <br>_Master only: one poll slot finished._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**baton\_node\_default\_config**](#function-baton_node_default_config) (struct [**baton\_node\_config**](structbaton__node__config.md) \* config) <br>_Fill_ `config` _with defaults for 9600 bit/s._ |
|  void | [**baton\_node\_get\_stats**](#function-baton_node_get_stats) (const struct [**baton\_node**](structbaton__node.md) \* node, struct [**baton\_stats**](structbaton__stats.md) \* stats) <br>_Copy the diagnostic counters._  |
|  int | [**baton\_node\_init**](#function-baton_node_init) (struct [**baton\_node**](structbaton__node.md) \* node, const struct [**baton\_node\_config**](structbaton__node__config.md) \* config, uint32\_t now\_ms) <br>_Validate the configuration and start the node._  |
|  void | [**baton\_node\_process**](#function-baton_node_process) (struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br>_Run the protocol._  |
|  int | [**baton\_node\_publish**](#function-baton_node_publish) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t msg\_id, const void \* data, uint8\_t length) <br>_Update a state value; the latest value is delivered to every other node._  |
|  void | [**baton\_node\_rx\_isr**](#function-baton_node_rx_isr) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t byte) <br>_Store one received byte._  |
|  int | [**baton\_node\_send**](#function-baton_node_send) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t dst, uint8\_t msg\_id, const void \* data, uint8\_t length, uint8\_t priority) <br>_Queue an event for reliable delivery._  |
|  void | [**baton\_node\_tx\_done\_isr**](#function-baton_node_tx_done_isr) (struct [**baton\_node**](structbaton__node.md) \* node) <br>_Signal the end of a transmission (only needed when rx\_echo is false)._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BATON\_DEAD\_POLL\_DIV**](group__baton__node.md#define-baton_dead_poll_div)  `4U`<br>_The master polls an unreachable slave only every BATON\_DEAD\_POLL\_DIV-th cycle._  |
| define  | [**BATON\_DEAD\_STREAK**](group__baton__node.md#define-baton_dead_streak)  `3U`<br>_A node failing this many times in a row is treated as unreachable._  |
| define  | [**BATON\_ERR\_ADDR**](group__baton__node.md#define-baton_err_addr)  `(-5)`<br>_Unknown destination or own address._  |
| define  | [**BATON\_ERR\_FULL**](group__baton__node.md#define-baton_err_full)  `(-6)`<br>_Event queue has no room; try again later._  |
| define  | [**BATON\_ERR\_LEN**](group__baton__node.md#define-baton_err_len)  `(-4)`<br>_Data longer than the maximum length of the message._  |
| define  | [**BATON\_ERR\_MSG\_ID**](group__baton__node.md#define-baton_err_msg_id)  `(-3)`<br>_Unknown message id, or wrong class (state vs event)._  |
| define  | [**BATON\_ERR\_NOT\_INIT**](group__baton__node.md#define-baton_err_not_init)  `(-2)`<br>[_**baton\_node\_init()**_](group__baton__node.md#function-baton_node_init) _was not called or failed._ |
| define  | [**BATON\_ERR\_PARAM**](group__baton__node.md#define-baton_err_param)  `(-1)`<br>_Invalid argument or configuration._  |
| define  | [**BATON\_EVENT\_COUNT**](group__baton__node.md#define-baton_event_count)  `(0U BATON\_EVENT\_LIST(BATON\_X\_COUNT\_))`<br>_Number of EVENT messages._  |
| define  | [**BATON\_MASK\_BYTES**](group__baton__node.md#define-baton_mask_bytes)  `(([**BATON\_MAX\_NODES**](group__baton__config.md#define-baton_max_nodes) + 7U) / 8U)`<br>_Bytes of a node mask inside a RESP._  |
| define  | [**BATON\_MAX\_MSG\_LEN**](group__baton__node.md#define-baton_max_msg_len)  `([**BATON\_MAX\_PAYLOAD**](group__baton__config.md#define-baton_max_payload) - [**BATON\_RESP\_PREFIX**](group__baton__node.md#define-baton_resp_prefix) - [**BATON\_TLV\_HEADER\_SIZE**](group__baton__defs.md#define-baton_tlv_header_size))`<br>_Largest MAX\_LEN of a message: one message must fit into any frame type._  |
| define  | [**BATON\_OK**](group__baton__node.md#define-baton_ok)  `0`<br>_Success._  |
| define  | [**BATON\_POLL\_PREFIX**](group__baton__node.md#define-baton_poll_prefix)  `1U`<br>_POLL payload prefix: ACKB._  |
| define  | [**BATON\_PRIO\_HIGH**](group__baton__node.md#define-baton_prio_high)  `1U`<br>_HIGH event queue, packed before NORMAL events._  |
| define  | [**BATON\_PRIO\_NORMAL**](group__baton__node.md#define-baton_prio_normal)  `0U`<br>_NORMAL event queue._  |
| define  | [**BATON\_RESP\_PREFIX**](group__baton__node.md#define-baton_resp_prefix)  `(1U + [**BATON\_MASK\_BYTES**](group__baton__node.md#define-baton_mask_bytes))`<br>_RESP payload prefix: ACKB and the NACK node mask._  |
| define  | [**BATON\_ROLE\_MASTER**](group__baton__node.md#define-baton_role_master)  `0U`<br>_The node that polls the slaves._  |
| define  | [**BATON\_ROLE\_SLAVE**](group__baton__node.md#define-baton_role_slave)  `1U`<br>_A node that transmits only when polled._  |
| define  | [**BATON\_STATE\_COUNT**](group__baton__node.md#define-baton_state_count)  `(0U BATON\_STATE\_LIST(BATON\_X\_COUNT\_))`<br>_Number of STATE messages._  |
| define  | [**BATON\_STATE\_DATA\_SIZE**](group__baton__node.md#define-baton_state_data_size)  `(1U BATON\_STATE\_LIST(BATON\_X\_STATE\_SIZE\_))`<br>_Bytes of the state mailboxes (one spare byte keeps the array non-empty)._  |

## Detailed Description


The master passes the baton (POLL) to the slaves in a fixed order; only the polled slave transmits, for at most slot\_time\_ms, so the bus never has collisions. Messages are of two classes, defined by the application message lists in `baton_user_config.h`: 
* EVENT: queued (HIGH or NORMAL priority), acknowledged, retransmitted, delivered exactly once and in order per (source, destination, priority), dropped after event\_ttl\_ms with an on\_drop report; 
* STATE: a mailbox per state; the latest value is delivered to every other node, intermediate values may be skipped, it can never overflow.

Threading model: [**baton\_node\_init()**](group__baton__node.md#function-baton_node_init), [**baton\_node\_process()**](group__baton__node.md#function-baton_node_process) and [**baton\_node\_get\_stats()**](group__baton__node.md#function-baton_node_get_stats) run in one context (main loop or one RTOS task). [**baton\_node\_rx\_isr()**](group__baton__node.md#function-baton_node_rx_isr) and [**baton\_node\_tx\_done\_isr()**](group__baton__node.md#function-baton_node_tx_done_isr) are called from the UART interrupts. [**baton\_node\_send()**](group__baton__node.md#function-baton_node_send) and [**baton\_node\_publish()**](group__baton__node.md#function-baton_node_publish) may be called from any context only when enter\_critical and exit\_critical are configured.


All memory is inside struct [**baton\_node**](structbaton__node.md), owned by the caller. No function blocks, allocates memory or reads a clock.




**
**


* Lifecycle: [**baton\_node\_default\_config**](group__baton__node.md#function-baton_node_default_config), [**baton\_node\_init**](group__baton__node.md#function-baton_node_init). 
* Processing: [**baton\_node\_process**](group__baton__node.md#function-baton_node_process). 
* Messages: [**baton\_node\_send**](group__baton__node.md#function-baton_node_send), [**baton\_node\_publish**](group__baton__node.md#function-baton_node_publish). 
* Interrupt entry points: [**baton\_node\_rx\_isr**](group__baton__node.md#function-baton_node_rx_isr), [**baton\_node\_tx\_done\_isr**](group__baton__node.md#function-baton_node_tx_done_isr). 
* Diagnostics: [**baton\_node\_get\_stats**](group__baton__node.md#function-baton_node_get_stats). 







    
## Public Types Documentation




### typedef baton\_critical\_fn 

_Enter or leave the critical section shared with interrupts._ 
```
typedef void(* baton_critical_fn) (void *context);
```




<hr>



### typedef baton\_drop\_fn 

_An event was not delivered within event\_ttl\_ms._ 
```
typedef void(* baton_drop_fn) (void *context, uint8_t msg_id, uint8_t dst, uint32_t missed_nodes);
```





**Parameters:**


* `context` Value of [**baton\_node\_config::context**](structbaton__node__config.md#variable-context). 
* `msg_id` Message identifier. 
* `dst` Address given to [**baton\_node\_send()**](group__baton__node.md#function-baton_node_send). 
* `missed_nodes` Node mask of the nodes that did not acknowledge it (a node may still have received it when only its acknowledgement was lost). 




        

<hr>



### typedef baton\_message\_fn 

_A message (state or event) was received._ 
```
typedef void(* baton_message_fn) (void *context, uint8_t src, uint8_t msg_id, const uint8_t *data, uint8_t length);
```



Called from [**baton\_node\_process()**](group__baton__node.md#function-baton_node_process). `data` is valid only during the call. 


        

<hr>



### enum baton\_msg\_id 

_Message identifiers: BATON\_MSG\_&lt;NAME&gt;._ 
```
enum baton_msg_id {
    BATON_STATE_LIST =(BATON_X_MSG_ENUM_) BATON_EVENT_LIST(BATON_X_MSG_ENUM_) BATON_MSG_INVALID_ =
        0x100
};
```




<hr>



### enum baton\_msg\_max\_len 

_Maximum data lengths: BATON\_MSG\_&lt;NAME&gt;\_MAX\_LEN._ 
```
enum baton_msg_max_len {
    BATON_STATE_LIST =(BATON_X_MSG_ENUM_) BATON_EVENT_LIST(BATON_X_MSG_ENUM_) BATON_MSG_INVALID_ =
        0x100
};
```




<hr>



### typedef baton\_poll\_done\_fn 

_Master only: one poll slot finished._ 
```
typedef void(* baton_poll_done_fn) (void *context, uint8_t slave, bool responded, uint32_t nack_nodes);
```





**Parameters:**


* `context` Value of [**baton\_node\_config::context**](structbaton__node__config.md#variable-context). 
* `slave` Address of the polled slave. 
* `responded` The slave answered with a RESP. 
* `nack_nodes` Node mask of the nodes that did not acknowledge DATA from that slave. 




        

<hr>
## Public Functions Documentation




### function baton\_node\_default\_config 

_Fill_ `config` _with defaults for 9600 bit/s._
```
void baton_node_default_config (
    struct baton_node_config * config
) 
```



Topology, write function and callbacks must still be set. 

**Parameters:**


* `config` Configuration to fill; NULL is ignored. 




        

<hr>



### function baton\_node\_get\_stats 

_Copy the diagnostic counters._ 
```
void baton_node_get_stats (
    const struct baton_node * node,
    struct baton_stats * stats
) 
```





**Parameters:**


* `node` Node state. 
* `stats` Destination; nothing is written when an argument is NULL. 




        

<hr>



### function baton\_node\_init 

_Validate the configuration and start the node._ 
```
int baton_node_init (
    struct baton_node * node,
    const struct baton_node_config * config,
    uint32_t now_ms
) 
```



Can be called again to restart. 

**Parameters:**


* `node` Node state. 
* `config` Configuration copied into the node. 
* `now_ms` Current time. 



**Return value:**


* `BATON_OK` Node ready. 
* `BATON_ERR_PARAM` NULL argument, inconsistent topology, duplicate message ids, a timing that does not allow one maximum-size exchange per slot, or an event that does not fit into a queue. 




        

<hr>



### function baton\_node\_process 

_Run the protocol._ 
```
void baton_node_process (
    struct baton_node * node,
    uint32_t now_ms
) 
```



Call often (every 1 ms is recommended). Never blocks. 

**Parameters:**


* `node` Node state; ignored when NULL or not initialized. 
* `now_ms` Current time. 




        

<hr>



### function baton\_node\_publish 

_Update a state value; the latest value is delivered to every other node._ 
```
int baton_node_publish (
    struct baton_node * node,
    uint8_t msg_id,
    const void * data,
    uint8_t length
) 
```





**Parameters:**


* `node` Node state. 
* `msg_id` STATE message identifier. 
* `data` Data bytes, or NULL when `length` is zero. 
* `length` Number of data bytes (at most the MAX\_LEN of the message). 



**Returns:**

BATON\_OK or BATON\_ERR\_\* (see the return codes). Never fails for lack of space. 





        

<hr>



### function baton\_node\_rx\_isr 

_Store one received byte._ 
```
void baton_node_rx_isr (
    struct baton_node * node,
    uint8_t byte
) 
```



Call from the UART receive interrupt. 

**Parameters:**


* `node` Node state; NULL is ignored. 
* `byte` Received byte. 




        

<hr>



### function baton\_node\_send 

_Queue an event for reliable delivery._ 
```
int baton_node_send (
    struct baton_node * node,
    uint8_t dst,
    uint8_t msg_id,
    const void * data,
    uint8_t length,
    uint8_t priority
) 
```



The data is copied.


The TTL of the event starts at the time of the last [**baton\_node\_process()**](group__baton__node.md#function-baton_node_process) call.




**Parameters:**


* `node` Node state. 
* `dst` Node address or BATON\_ADDR\_BROADCAST. 
* `msg_id` EVENT message identifier. 
* `data` Data bytes, or NULL when `length` is zero. 
* `length` Number of data bytes (at most the MAX\_LEN of the message). 
* `priority` BATON\_PRIO\_NORMAL or BATON\_PRIO\_HIGH. 



**Returns:**

BATON\_OK or BATON\_ERR\_\* (see the return codes). 





        

<hr>



### function baton\_node\_tx\_done\_isr 

_Signal the end of a transmission (only needed when rx\_echo is false)._ 
```
void baton_node_tx_done_isr (
    struct baton_node * node
) 
```





**Parameters:**


* `node` Node state; NULL is ignored. 




        

<hr>
## Macro Definition Documentation





### define BATON\_DEAD\_POLL\_DIV 

_The master polls an unreachable slave only every BATON\_DEAD\_POLL\_DIV-th cycle._ 
```
#define BATON_DEAD_POLL_DIV `4U`
```




<hr>



### define BATON\_DEAD\_STREAK 

_A node failing this many times in a row is treated as unreachable._ 
```
#define BATON_DEAD_STREAK `3U`
```




<hr>



### define BATON\_ERR\_ADDR 

_Unknown destination or own address._ 
```
#define BATON_ERR_ADDR `(-5)`
```




<hr>



### define BATON\_ERR\_FULL 

_Event queue has no room; try again later._ 
```
#define BATON_ERR_FULL `(-6)`
```




<hr>



### define BATON\_ERR\_LEN 

_Data longer than the maximum length of the message._ 
```
#define BATON_ERR_LEN `(-4)`
```




<hr>



### define BATON\_ERR\_MSG\_ID 

_Unknown message id, or wrong class (state vs event)._ 
```
#define BATON_ERR_MSG_ID `(-3)`
```




<hr>



### define BATON\_ERR\_NOT\_INIT 

[_**baton\_node\_init()**_](group__baton__node.md#function-baton_node_init) _was not called or failed._
```
#define BATON_ERR_NOT_INIT `(-2)`
```




<hr>



### define BATON\_ERR\_PARAM 

_Invalid argument or configuration._ 
```
#define BATON_ERR_PARAM `(-1)`
```




<hr>



### define BATON\_EVENT\_COUNT 

_Number of EVENT messages._ 
```
#define BATON_EVENT_COUNT `(0U BATON_EVENT_LIST(BATON_X_COUNT_))`
```




<hr>



### define BATON\_MASK\_BYTES 

_Bytes of a node mask inside a RESP._ 
```
#define BATON_MASK_BYTES `(( BATON_MAX_NODES + 7U) / 8U)`
```




<hr>



### define BATON\_MAX\_MSG\_LEN 

_Largest MAX\_LEN of a message: one message must fit into any frame type._ 
```
#define BATON_MAX_MSG_LEN `( BATON_MAX_PAYLOAD - BATON_RESP_PREFIX - BATON_TLV_HEADER_SIZE )`
```




<hr>



### define BATON\_OK 

_Success._ 
```
#define BATON_OK `0`
```




<hr>



### define BATON\_POLL\_PREFIX 

_POLL payload prefix: ACKB._ 
```
#define BATON_POLL_PREFIX `1U`
```




<hr>



### define BATON\_PRIO\_HIGH 

_HIGH event queue, packed before NORMAL events._ 
```
#define BATON_PRIO_HIGH `1U`
```




<hr>



### define BATON\_PRIO\_NORMAL 

_NORMAL event queue._ 
```
#define BATON_PRIO_NORMAL `0U`
```




<hr>



### define BATON\_RESP\_PREFIX 

_RESP payload prefix: ACKB and the NACK node mask._ 
```
#define BATON_RESP_PREFIX `(1U + BATON_MASK_BYTES )`
```




<hr>



### define BATON\_ROLE\_MASTER 

_The node that polls the slaves._ 
```
#define BATON_ROLE_MASTER `0U`
```




<hr>



### define BATON\_ROLE\_SLAVE 

_A node that transmits only when polled._ 
```
#define BATON_ROLE_SLAVE `1U`
```




<hr>



### define BATON\_STATE\_COUNT 

_Number of STATE messages._ 
```
#define BATON_STATE_COUNT `(0U BATON_STATE_LIST(BATON_X_COUNT_))`
```




<hr>



### define BATON\_STATE\_DATA\_SIZE 

_Bytes of the state mailboxes (one spare byte keeps the array non-empty)._ 
```
#define BATON_STATE_DATA_SIZE `(1U BATON_STATE_LIST(BATON_X_STATE_SIZE_))`
```




<hr>

------------------------------


