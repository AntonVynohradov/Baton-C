

# Group baton\_config



[**Modules**](modules.md) **>** [**baton\_config**](group__baton__config.md)



_Build-time configuration for Baton._ [More...](#detailed-description)

































































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BATON\_MAX\_MSGS\_PER\_FRAME**](group__baton__config.md#define-baton_max_msgs_per_frame)  `16U`<br>_Largest number of messages packed into one frame (1..64)._  |
| define  | [**BATON\_MAX\_NODES**](group__baton__config.md#define-baton_max_nodes)  `8U`<br>_Number of nodes on the bus including the master (2..32)._  |
| define  | [**BATON\_MAX\_PAYLOAD**](group__baton__config.md#define-baton_max_payload)  `64U`<br>_Largest frame payload in bytes (16..250)._  |
| define  | [**BATON\_QUEUE\_HIGH\_SIZE**](group__baton__config.md#define-baton_queue_high_size)  `256U`<br>_Size of the HIGH priority event queue in bytes (64..65532)._  |
| define  | [**BATON\_QUEUE\_NORMAL\_SIZE**](group__baton__config.md#define-baton_queue_normal_size)  `512U`<br>_Size of the NORMAL priority event queue in bytes (64..65532)._  |
| define  | [**BATON\_RX\_BUF\_SIZE**](group__baton__config.md#define-baton_rx_buf_size)  `128U`<br>_Receive ring buffer filled from the UART interrupt; power of two, 16..4096._  |

## Detailed Description


The application provides `baton_user_config.h` on the include path (copy `config/baton_user_config_template.h`). It must define the two message lists BATON\_STATE\_LIST and BATON\_EVENT\_LIST and may override any size below; a size can also be set with a compiler definition (for example `-DBATON_MAX_NODES=4U`). Every node of a bus must be built with the same message lists.




**
**


* [**BATON\_MAX\_NODES**](group__baton__config.md#define-baton_max_nodes) : nodes on the bus including the master. 
* [**BATON\_MAX\_PAYLOAD**](group__baton__config.md#define-baton_max_payload) : largest frame payload. 
* [**BATON\_MAX\_MSGS\_PER\_FRAME**](group__baton__config.md#define-baton_max_msgs_per_frame) : messages packed into one frame. 
* [**BATON\_QUEUE\_HIGH\_SIZE**](group__baton__config.md#define-baton_queue_high_size), [**BATON\_QUEUE\_NORMAL\_SIZE**](group__baton__config.md#define-baton_queue_normal_size) : event queues. 
* [**BATON\_RX\_BUF\_SIZE**](group__baton__config.md#define-baton_rx_buf_size) : receive ring filled from the UART interrupt. 







    
## Macro Definition Documentation





### define BATON\_MAX\_MSGS\_PER\_FRAME 

_Largest number of messages packed into one frame (1..64)._ 
```
#define BATON_MAX_MSGS_PER_FRAME `16U`
```




<hr>



### define BATON\_MAX\_NODES 

_Number of nodes on the bus including the master (2..32)._ 
```
#define BATON_MAX_NODES `8U`
```




<hr>



### define BATON\_MAX\_PAYLOAD 

_Largest frame payload in bytes (16..250)._ 
```
#define BATON_MAX_PAYLOAD `64U`
```



One message needs its length + 2. 


        

<hr>



### define BATON\_QUEUE\_HIGH\_SIZE 

_Size of the HIGH priority event queue in bytes (64..65532)._ 
```
#define BATON_QUEUE_HIGH_SIZE `256U`
```



Each queued event uses 16 + length bytes, rounded up to 4. 


        

<hr>



### define BATON\_QUEUE\_NORMAL\_SIZE 

_Size of the NORMAL priority event queue in bytes (64..65532)._ 
```
#define BATON_QUEUE_NORMAL_SIZE `512U`
```




<hr>



### define BATON\_RX\_BUF\_SIZE 

_Receive ring buffer filled from the UART interrupt; power of two, 16..4096._ 
```
#define BATON_RX_BUF_SIZE `128U`
```




<hr>

------------------------------


