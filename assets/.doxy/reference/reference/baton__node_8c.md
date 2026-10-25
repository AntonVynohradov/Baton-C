

# File baton\_node.c



[**FileList**](files.md) **>** [**src**](dir_68267d1309a1af8e8297ef4c3efbcdba.md) **>** [**baton\_node.c**](baton__node_8c.md)

[Go to the source code of this file](baton__node_8c_source.md)



* `#include <stddef.h>`
* `#include <string.h>`
* `#include "baton_node.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**msg\_desc**](structmsg__desc.md) <br>_Identifier and maximum length of one message._  |








## Public Static Attributes

| Type | Name |
| ---: | :--- |
|  const struct [**msg\_desc**](structmsg__desc.md) | [**state\_desc**](#variable-state_desc)   = `/* multi line expression */`<br> |
















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**chan\_abandon**](#function-chan_abandon) (struct [**baton\_channel**](structbaton__channel.md) \* channel) <br>_The content of the outstanding frame was abandoned: the next content uses a new seq._  |
|  void | [**chan\_commit**](#function-chan_commit) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t peer) <br>_Acknowledged: release all messages of the outstanding frame for this peer._  |
|  void | [**chan\_drop\_ref**](#function-chan_drop_ref) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t peer, uint16\_t ref) <br>_Remove an expired event from the outstanding frame of_ `peer` _(its inflight bit is set)._ |
|  void | [**chan\_remove\_at**](#function-chan_remove_at) (struct [**baton\_channel**](structbaton__channel.md) \* channel, uint8\_t i) <br> |
|  bool | [**config\_valid**](#function-config_valid) (const struct [**baton\_node\_config**](structbaton__node__config.md) \* config) <br> |
|  void | [**deliver**](#function-deliver) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t src\_index, const uint8\_t \* payload, uint16\_t length) <br> |
|  uint16\_t | [**emit**](#function-emit) (uint8\_t \* out, uint16\_t n, uint8\_t id, const uint8\_t \* data, uint8\_t length) <br> |
|  void | [**expire\_events**](#function-expire_events) (struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br> |
|  void | [**expire\_record**](#function-expire_record) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t qi, uint16\_t offset, const struct [**baton\_record**](structbaton__record.md) \* record) <br> |
|  bool | [**find\_desc**](#function-find_desc) (const struct [**msg\_desc**](structmsg__desc.md) \* table, uint8\_t count, uint8\_t id, uint8\_t \* index) <br> |
|  uint32\_t | [**frame\_ms**](#function-frame_ms) (const struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t bytes) <br> |
|  void | [**handle\_frame**](#function-handle_frame) (struct [**baton\_node**](structbaton__node.md) \* node, const struct [**baton\_frame**](structbaton__frame.md) \* frame, uint32\_t now\_ms) <br> |
|  void | [**housekeeping**](#function-housekeeping) (struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br> |
|  void | [**link\_service**](#function-link_service) (struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br> |
|  void | [**lock**](#function-lock) (const struct [**baton\_node**](structbaton__node.md) \* node) <br> |
|  void | [**master\_on\_resp**](#function-master_on_resp) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t src\_index, const struct [**baton\_frame**](structbaton__frame.md) \* frame) <br> |
|  void | [**master\_poll\_done**](#function-master_poll_done) (struct [**baton\_node**](structbaton__node.md) \* node, bool responded, uint32\_t nack\_nodes) <br> |
|  void | [**master\_send\_poll**](#function-master_send_poll) (struct [**baton\_node**](structbaton__node.md) \* node) <br> |
|  void | [**master\_service**](#function-master_service) (struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br> |
|  uint32\_t | [**min\_slot\_ms**](#function-min_slot_ms) (const struct [**baton\_node**](structbaton__node.md) \* node) <br>_Shortest slot in which a slave can still send one maximum-size message._  |
|  int | [**msg\_max\_length**](#function-msg_max_length) (uint8\_t id) <br>_Maximum length of a known message (state or event), -1 for an unknown id._  |
|  uint32\_t | [**node\_bit**](#function-node_bit) (uint8\_t index) <br> |
|  uint8\_t | [**node\_index**](#function-node_index) (const struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t address) <br> |
|  void | [**on\_tx\_done**](#function-on_tx_done) (struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br> |
|  int | [**pack**](#function-pack) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t peer, uint8\_t \* out, uint16\_t max\_length) <br>_Build the message part of a frame for_ `peer:` _the outstanding content again, or HIGH events, NORMAL events and dirty states._ |
|  int | [**pack\_again**](#function-pack_again) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t peer, uint8\_t \* out, uint16\_t max\_length) <br>_Rebuild the content of the unacknowledged frame (same seq) so the receiver can filter duplicates._  |
|  uint16\_t | [**pack\_events**](#function-pack_events) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t peer, uint8\_t \* out, uint16\_t n, uint16\_t max\_length) <br>_Take queued events for_ `peer` _, HIGH first, FIFO within a queue (never skip ahead)._ |
|  uint16\_t | [**pack\_states**](#function-pack_states) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t peer, uint8\_t \* out, uint16\_t n, uint16\_t max\_length) <br>_Take the dirty states for_ `peer` _._ |
|  void | [**queue\_reclaim**](#function-queue_reclaim) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t qi) <br> |
|  uint8\_t | [**ref\_kind**](#function-ref_kind) (uint16\_t ref) <br> |
|  uint16\_t | [**ref\_make**](#function-ref_make) (uint8\_t kind, uint16\_t value) <br> |
|  uint16\_t | [**ref\_value**](#function-ref_value) (uint16\_t ref) <br> |
|  void | [**rx\_content**](#function-rx_content) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t src\_index, uint8\_t ctrl, const uint8\_t \* payload, uint16\_t length) <br>_Deliver the content of a frame unless it is a retransmission that was already delivered._  |
|  void | [**slave\_ack\_timeout**](#function-slave_ack_timeout) (struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br> |
|  int32\_t | [**slave\_budget**](#function-slave_budget) (const struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br>_Payload bytes a DATA frame may still carry in this slot (time for the RESP is reserved)._  |
|  void | [**slave\_on\_ack**](#function-slave_on_ack) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t src\_index, uint8\_t ctrl, uint32\_t now\_ms) <br> |
|  void | [**slave\_on\_data**](#function-slave_on_data) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t src\_index, const struct [**baton\_frame**](structbaton__frame.md) \* frame) <br> |
|  void | [**slave\_on\_poll**](#function-slave_on_poll) (struct [**baton\_node**](structbaton__node.md) \* node, const struct [**baton\_frame**](structbaton__frame.md) \* frame, uint32\_t now\_ms) <br> |
|  void | [**slave\_send\_resp**](#function-slave_send_resp) (struct [**baton\_node**](structbaton__node.md) \* node) <br> |
|  void | [**slave\_slot\_step**](#function-slave_slot_step) (struct [**baton\_node**](structbaton__node.md) \* node, uint32\_t now\_ms) <br>_Continue the slot: DATA to the next peer that has something, else RESP._  |
|  int | [**slave\_tx\_data**](#function-slave_tx_data) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t peer, uint32\_t now\_ms) <br> |
|  uint16\_t | [**state\_offset**](#function-state_offset) (uint8\_t s) <br>_Offset of state s in the mailbox storage._  |
|  bool | [**timing\_valid**](#function-timing_valid) (const struct [**baton\_node**](structbaton__node.md) \* node) <br>_Timing must allow an ACK and at least one maximum-size exchange per slot._  |
|  bool | [**topology\_valid**](#function-topology_valid) (struct [**baton\_node**](structbaton__node.md) \* node) <br>_Build the node table (0 = master, 1..n = slaves) and find this node in it._  |
|  bool | [**try\_emit\_state**](#function-try_emit_state) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t \* out, uint16\_t \* n, uint16\_t limit, uint8\_t s, uint32\_t clear\_bit) <br>_Copy the current value of state s if it fits below_ `limit` _; clear its dirty bits._ |
|  void | [**tx\_request**](#function-tx_request) (struct [**baton\_node**](structbaton__node.md) \* node, uint8\_t \* buffer, size\_t capacity, uint8\_t dst, uint8\_t ctrl, uint8\_t payload\_length) <br>_Seal the frame in_ `buffer` _(payload already at BATON\_HEADER\_SIZE) and request it._ |
|  void | [**unlock**](#function-unlock) (const struct [**baton\_node**](structbaton__node.md) \* node) <br> |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**HOUSEKEEP\_PERIOD\_MS**](baton__node_8c.md#define-housekeep_period_ms)  `10U`<br>_Period of the TTL scan._  |
| define  | [**L\_IDLE**](baton__node_8c.md#define-l_idle)  `0U`<br>_Link (master/slave) states._  |
| define  | [**M\_POLL\_TX**](baton__node_8c.md#define-m_poll_tx)  `5U`<br> |
| define  | [**M\_WAIT\_RESP**](baton__node_8c.md#define-m_wait_resp)  `6U`<br> |
| define  | [**NO\_NODE**](baton__node_8c.md#define-no_node)  `0xFFU`<br>_Returned by node\_index() for an unknown address._  |
| define  | [**Q\_HIGH**](baton__node_8c.md#define-q_high)  `0U`<br>_Queue indexes; also the kind of a message reference._  |
| define  | [**Q\_NORMAL**](baton__node_8c.md#define-q_normal)  `1U`<br> |
| define  | [**REF\_STATE**](baton__node_8c.md#define-ref_state)  `2U`<br> |
| define  | [**RESP\_TIMEOUT\_MARGIN\_MS**](baton__node_8c.md#define-resp_timeout_margin_ms)  `10U`<br>_Margin added to the slot time before the master gives up waiting for a RESP._  |
| define  | [**STREAK\_MAX**](baton__node_8c.md#define-streak_max)  `255U`<br>_Saturation limit of the failure streak counters._  |
| define  | [**S\_ACK\_TX**](baton__node_8c.md#define-s_ack_tx)  `4U`<br> |
| define  | [**S\_DATA\_TX**](baton__node_8c.md#define-s_data_tx)  `1U`<br> |
| define  | [**S\_RESP\_TX**](baton__node_8c.md#define-s_resp_tx)  `3U`<br> |
| define  | [**S\_WAIT\_ACK**](baton__node_8c.md#define-s_wait_ack)  `2U`<br> |

## Public Static Attributes Documentation




### variable state\_desc 

```C++
const struct msg_desc state_desc[BATON_STATE_COUNT+1U];
```




<hr>
## Public Static Functions Documentation




### function chan\_abandon 

_The content of the outstanding frame was abandoned: the next content uses a new seq._ 
```C++
static void chan_abandon (
    struct baton_channel * channel
) 
```




<hr>



### function chan\_commit 

_Acknowledged: release all messages of the outstanding frame for this peer._ 
```C++
static void chan_commit (
    struct baton_node * node,
    uint8_t peer
) 
```




<hr>



### function chan\_drop\_ref 

_Remove an expired event from the outstanding frame of_ `peer` _(its inflight bit is set)._
```C++
static void chan_drop_ref (
    struct baton_node * node,
    uint8_t peer,
    uint16_t ref
) 
```




<hr>



### function chan\_remove\_at 

```C++
static void chan_remove_at (
    struct baton_channel * channel,
    uint8_t i
) 
```




<hr>



### function config\_valid 

```C++
static bool config_valid (
    const struct baton_node_config * config
) 
```




<hr>



### function deliver 

```C++
static void deliver (
    struct baton_node * node,
    uint8_t src_index,
    const uint8_t * payload,
    uint16_t length
) 
```




<hr>



### function emit 

```C++
static uint16_t emit (
    uint8_t * out,
    uint16_t n,
    uint8_t id,
    const uint8_t * data,
    uint8_t length
) 
```




<hr>



### function expire\_events 

```C++
static void expire_events (
    struct baton_node * node,
    uint32_t now_ms
) 
```




<hr>



### function expire\_record 

```C++
static void expire_record (
    struct baton_node * node,
    uint8_t qi,
    uint16_t offset,
    const struct baton_record * record
) 
```




<hr>



### function find\_desc 

```C++
static bool find_desc (
    const struct msg_desc * table,
    uint8_t count,
    uint8_t id,
    uint8_t * index
) 
```




<hr>



### function frame\_ms 

```C++
static uint32_t frame_ms (
    const struct baton_node * node,
    uint32_t bytes
) 
```




<hr>



### function handle\_frame 

```C++
static void handle_frame (
    struct baton_node * node,
    const struct baton_frame * frame,
    uint32_t now_ms
) 
```




<hr>



### function housekeeping 

```C++
static void housekeeping (
    struct baton_node * node,
    uint32_t now_ms
) 
```




<hr>



### function link\_service 

```C++
static void link_service (
    struct baton_node * node,
    uint32_t now_ms
) 
```




<hr>



### function lock 

```C++
static void lock (
    const struct baton_node * node
) 
```




<hr>



### function master\_on\_resp 

```C++
static void master_on_resp (
    struct baton_node * node,
    uint8_t src_index,
    const struct baton_frame * frame
) 
```




<hr>



### function master\_poll\_done 

```C++
static void master_poll_done (
    struct baton_node * node,
    bool responded,
    uint32_t nack_nodes
) 
```




<hr>



### function master\_send\_poll 

```C++
static void master_send_poll (
    struct baton_node * node
) 
```




<hr>



### function master\_service 

```C++
static void master_service (
    struct baton_node * node,
    uint32_t now_ms
) 
```




<hr>



### function min\_slot\_ms 

_Shortest slot in which a slave can still send one maximum-size message._ 
```C++
static uint32_t min_slot_ms (
    const struct baton_node * node
) 
```




<hr>



### function msg\_max\_length 

_Maximum length of a known message (state or event), -1 for an unknown id._ 
```C++
static int msg_max_length (
    uint8_t id
) 
```




<hr>



### function node\_bit 

```C++
static uint32_t node_bit (
    uint8_t index
) 
```




<hr>



### function node\_index 

```C++
static uint8_t node_index (
    const struct baton_node * node,
    uint8_t address
) 
```




<hr>



### function on\_tx\_done 

```C++
static void on_tx_done (
    struct baton_node * node,
    uint32_t now_ms
) 
```




<hr>



### function pack 

_Build the message part of a frame for_ `peer:` _the outstanding content again, or HIGH events, NORMAL events and dirty states._
```C++
static int pack (
    struct baton_node * node,
    uint8_t peer,
    uint8_t * out,
    uint16_t max_length
) 
```





**Returns:**

bytes written, 0 when there is nothing to send, -1 when the retransmission does not fit into `max_length`. 





        

<hr>



### function pack\_again 

_Rebuild the content of the unacknowledged frame (same seq) so the receiver can filter duplicates._ 
```C++
static int pack_again (
    struct baton_node * node,
    uint8_t peer,
    uint8_t * out,
    uint16_t max_length
) 
```



A state that grew and no longer fits is sent in a later frame. 

**Returns:**

bytes written, 0 when nothing is left (the frame was abandoned), -1 when the events do not fit into `max_length`. 





        

<hr>



### function pack\_events 

_Take queued events for_ `peer` _, HIGH first, FIFO within a queue (never skip ahead)._
```C++
static uint16_t pack_events (
    struct baton_node * node,
    uint8_t peer,
    uint8_t * out,
    uint16_t n,
    uint16_t max_length
) 
```



Called only when the channel has no outstanding content, so no record has the inflight bit of `peer` set (a bit is set only together with a channel reference). 


        

<hr>



### function pack\_states 

_Take the dirty states for_ `peer` _._
```C++
static uint16_t pack_states (
    struct baton_node * node,
    uint8_t peer,
    uint8_t * out,
    uint16_t n,
    uint16_t max_length
) 
```




<hr>



### function queue\_reclaim 

```C++
static void queue_reclaim (
    struct baton_node * node,
    uint8_t qi
) 
```




<hr>



### function ref\_kind 

```C++
static uint8_t ref_kind (
    uint16_t ref
) 
```




<hr>



### function ref\_make 

```C++
static uint16_t ref_make (
    uint8_t kind,
    uint16_t value
) 
```




<hr>



### function ref\_value 

```C++
static uint16_t ref_value (
    uint16_t ref
) 
```




<hr>



### function rx\_content 

_Deliver the content of a frame unless it is a retransmission that was already delivered._ 
```C++
static void rx_content (
    struct baton_node * node,
    uint8_t src_index,
    uint8_t ctrl,
    const uint8_t * payload,
    uint16_t length
) 
```




<hr>



### function slave\_ack\_timeout 

```C++
static void slave_ack_timeout (
    struct baton_node * node,
    uint32_t now_ms
) 
```




<hr>



### function slave\_budget 

_Payload bytes a DATA frame may still carry in this slot (time for the RESP is reserved)._ 
```C++
static int32_t slave_budget (
    const struct baton_node * node,
    uint32_t now_ms
) 
```




<hr>



### function slave\_on\_ack 

```C++
static void slave_on_ack (
    struct baton_node * node,
    uint8_t src_index,
    uint8_t ctrl,
    uint32_t now_ms
) 
```




<hr>



### function slave\_on\_data 

```C++
static void slave_on_data (
    struct baton_node * node,
    uint8_t src_index,
    const struct baton_frame * frame
) 
```




<hr>



### function slave\_on\_poll 

```C++
static void slave_on_poll (
    struct baton_node * node,
    const struct baton_frame * frame,
    uint32_t now_ms
) 
```




<hr>



### function slave\_send\_resp 

```C++
static void slave_send_resp (
    struct baton_node * node
) 
```




<hr>



### function slave\_slot\_step 

_Continue the slot: DATA to the next peer that has something, else RESP._ 
```C++
static void slave_slot_step (
    struct baton_node * node,
    uint32_t now_ms
) 
```



Pass 0 serves peers that answered recently, pass 1 the ones that failed before, so an unreachable node cannot eat the slot of the healthy ones. 


        

<hr>



### function slave\_tx\_data 

```C++
static int slave_tx_data (
    struct baton_node * node,
    uint8_t peer,
    uint32_t now_ms
) 
```





**Returns:**

1 frame requested, 0 nothing for this peer, -1 no time left or it does not fit. 





        

<hr>



### function state\_offset 

_Offset of state s in the mailbox storage._ 
```C++
static uint16_t state_offset (
    uint8_t s
) 
```




<hr>



### function timing\_valid 

_Timing must allow an ACK and at least one maximum-size exchange per slot._ 
```C++
static bool timing_valid (
    const struct baton_node * node
) 
```




<hr>



### function topology\_valid 

_Build the node table (0 = master, 1..n = slaves) and find this node in it._ 
```C++
static bool topology_valid (
    struct baton_node * node
) 
```




<hr>



### function try\_emit\_state 

_Copy the current value of state s if it fits below_ `limit` _; clear its dirty bits._
```C++
static bool try_emit_state (
    struct baton_node * node,
    uint8_t * out,
    uint16_t * n,
    uint16_t limit,
    uint8_t s,
    uint32_t clear_bit
) 
```




<hr>



### function tx\_request 

_Seal the frame in_ `buffer` _(payload already at BATON\_HEADER\_SIZE) and request it._
```C++
static void tx_request (
    struct baton_node * node,
    uint8_t * buffer,
    size_t capacity,
    uint8_t dst,
    uint8_t ctrl,
    uint8_t payload_length
) 
```




<hr>



### function unlock 

```C++
static void unlock (
    const struct baton_node * node
) 
```




<hr>
## Macro Definition Documentation





### define HOUSEKEEP\_PERIOD\_MS 

_Period of the TTL scan._ 
```C++
#define HOUSEKEEP_PERIOD_MS `10U`
```




<hr>



### define L\_IDLE 

_Link (master/slave) states._ 
```C++
#define L_IDLE `0U`
```




<hr>



### define M\_POLL\_TX 

```C++
#define M_POLL_TX `5U`
```




<hr>



### define M\_WAIT\_RESP 

```C++
#define M_WAIT_RESP `6U`
```




<hr>



### define NO\_NODE 

_Returned by node\_index() for an unknown address._ 
```C++
#define NO_NODE `0xFFU`
```




<hr>



### define Q\_HIGH 

_Queue indexes; also the kind of a message reference._ 
```C++
#define Q_HIGH `0U`
```




<hr>



### define Q\_NORMAL 

```C++
#define Q_NORMAL `1U`
```




<hr>



### define REF\_STATE 

```C++
#define REF_STATE `2U`
```




<hr>



### define RESP\_TIMEOUT\_MARGIN\_MS 

_Margin added to the slot time before the master gives up waiting for a RESP._ 
```C++
#define RESP_TIMEOUT_MARGIN_MS `10U`
```




<hr>



### define STREAK\_MAX 

_Saturation limit of the failure streak counters._ 
```C++
#define STREAK_MAX `255U`
```




<hr>



### define S\_ACK\_TX 

```C++
#define S_ACK_TX `4U`
```




<hr>



### define S\_DATA\_TX 

```C++
#define S_DATA_TX `1U`
```




<hr>



### define S\_RESP\_TX 

```C++
#define S_RESP_TX `3U`
```




<hr>



### define S\_WAIT\_ACK 

```C++
#define S_WAIT_ACK `2U`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `src/baton_node.c`

