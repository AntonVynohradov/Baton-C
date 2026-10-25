

# Group baton



[**Modules**](modules.md) **>** [**baton**](group__baton.md)



_Public API of the Baton bus library._ [More...](#detailed-description)












## Modules

| Type | Name |
| ---: | :--- |
| module | [**Configuration**](group__baton__config.md) <br>_Build-time configuration for Baton._  |
| module | [**Definitions**](group__baton__defs.md) <br>_Frame layout, frame types, payload prefixes and byte-order helpers._  |
| module | [**Link**](group__baton__link.md) <br>_Byte-level half-duplex link between the UART and the node layer._  |
| module | [**Node**](group__baton__node.md) <br>_One bus node (master or slave) on top of the_ [_**Link**_](group__baton__link.md) _._ |
| module | [**Codec**](group__baton__protocol.md) <br>_CRC-16, incremental stream parser and frame encoder._  |
| module | [**Event queue**](group__baton__queue.md) <br>_Byte ring of variable-size event records, used by_ [_**Node**_](group__baton__node.md) _._ |





















































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BATON\_VERSION**](group__baton.md#define-baton_version)  `"1.0.0"`<br>_Version string "MAJOR.MINOR.PATCH"._  |
| define  | [**BATON\_VERSION\_MAJOR**](group__baton.md#define-baton_version_major)  `1`<br> |
| define  | [**BATON\_VERSION\_MINOR**](group__baton.md#define-baton_version_minor)  `0`<br> |
| define  | [**BATON\_VERSION\_PATCH**](group__baton.md#define-baton_version_patch)  `0`<br> |

## Detailed Description


This module groups the public interface and its supporting submodules: [**Configuration**](group__baton__config.md), [**Definitions**](group__baton__defs.md), [**Codec**](group__baton__protocol.md), [**Link**](group__baton__link.md), [**Event queue**](group__baton__queue.md) and [**Node**](group__baton__node.md).


Include this single header to use the library: 
* [**baton\_config.h**](baton__config_8h.md) build-time sizes and the application message lists 
* [**baton\_defs.h**](baton__defs_8h.md) frame layout, frame types, control-byte and byte-order helpers 
* [**baton\_protocol.h**](baton__protocol_8h.md) CRC-16, incremental parser, encoder 
* [**baton\_link.h**](baton__link_8h.md) half-duplex link: receive ring, parser timeout, transmitter 
* [**baton\_queue.h**](baton__queue_8h.md) event queue (used by the node) 
* [**baton\_node.h**](baton__node_8h.md) master/slave node: events, states, schedule 




    
## Macro Definition Documentation





### define BATON\_VERSION 

_Version string "MAJOR.MINOR.PATCH"._ 
```
#define BATON_VERSION `"1.0.0"`
```




<hr>



### define BATON\_VERSION\_MAJOR 

```
#define BATON_VERSION_MAJOR `1`
```




<hr>



### define BATON\_VERSION\_MINOR 

```
#define BATON_VERSION_MINOR `0`
```




<hr>



### define BATON\_VERSION\_PATCH 

```
#define BATON_VERSION_PATCH `0`
```




<hr>

------------------------------


