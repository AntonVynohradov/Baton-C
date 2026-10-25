
# Class Member Variables



## a

* **ack\_frame** ([**baton\_node**](structbaton__node.md))
* **ack\_timeout\_ms** ([**baton\_node\_config**](structbaton__node__config.md))
* **ack\_timeouts** ([**baton\_stats**](structbaton__stats.md))


## b

* **baudrate** ([**baton\_link\_config**](structbaton__link__config.md), [**baton\_node\_config**](structbaton__node__config.md))
* **bad\_messages** ([**baton\_stats**](structbaton__stats.md))


## c

* **ctrl** ([**baton\_frame**](structbaton__frame.md))
* **config** ([**baton\_link**](structbaton__link.md), [**baton\_node**](structbaton__node.md))
* **counters** ([**baton\_link**](structbaton__link.md))
* **context** ([**baton\_link\_config**](structbaton__link__config.md), [**baton\_node\_config**](structbaton__node__config.md))
* **collisions** ([**baton\_link\_counters**](structbaton__link__counters.md), [**baton\_stats**](structbaton__stats.md))
* **crc\_errors** ([**baton\_link\_counters**](structbaton__link__counters.md), [**baton\_stats**](structbaton__stats.md))
* **channel** ([**baton\_node**](structbaton__node.md))
* **current** ([**baton\_node**](structbaton__node.md))
* **cycle\_count** ([**baton\_node**](structbaton__node.md))
* **cycle\_start** ([**baton\_node**](structbaton__node.md))
* **cycle\_period\_ms** ([**baton\_node\_config**](structbaton__node__config.md))
* **crc** ([**baton\_parser**](structbaton__parser.md))


## d

* **dst** ([**baton\_frame**](structbaton__frame.md), [**baton\_record**](structbaton__record.md))
* **duplicates** ([**baton\_stats**](structbaton__stats.md))


## e

* **echo\_error** ([**baton\_link**](structbaton__link.md))
* **echo\_index** ([**baton\_link**](structbaton__link.md))
* **enter\_critical** ([**baton\_node\_config**](structbaton__node__config.md))
* **event\_ttl\_ms** ([**baton\_node\_config**](structbaton__node__config.md))
* **exit\_critical** ([**baton\_node\_config**](structbaton__node__config.md))
* **events\_dropped** ([**baton\_stats**](structbaton__stats.md))


## f

* **fail\_streak** ([**baton\_channel**](structbaton__channel.md))
* **frame** ([**baton\_parser**](structbaton__parser.md))
* **flags** ([**baton\_record**](structbaton__record.md))


## h

* **head** ([**baton\_queue**](structbaton__queue.md))


## i

* **inflight** ([**baton\_channel**](structbaton__channel.md), [**baton\_record**](structbaton__record.md))
* **inflight\_count** ([**baton\_channel**](structbaton__channel.md))
* **inter\_byte\_timeout\_ms** ([**baton\_link\_config**](structbaton__link__config.md), [**baton\_node\_config**](structbaton__node__config.md))
* **initialized** ([**baton\_node**](structbaton__node.md))
* **index** ([**baton\_parser**](structbaton__parser.md))
* **id** ([**msg\_desc**](structmsg__desc.md))


## l

* **length** ([**baton\_frame**](structbaton__frame.md), [**baton\_record**](structbaton__record.md))
* **last\_rx\_ms** ([**baton\_link**](structbaton__link.md))
* **last\_tx\_end\_ms** ([**baton\_link**](structbaton__link.md))
* **last\_housekeep\_ms** ([**baton\_node**](structbaton__node.md))
* **link** ([**baton\_node**](structbaton__node.md))
* **link\_state** ([**baton\_node**](structbaton__node.md))
* **link\_timer** ([**baton\_node**](structbaton__node.md))


## m

* **master\_address** ([**baton\_node\_config**](structbaton__node__config.md))
* **max\_retries** ([**baton\_node\_config**](structbaton__node__config.md))
* **memory** ([**baton\_queue**](structbaton__queue.md))
* **msg\_id** ([**baton\_record**](structbaton__record.md))
* **max\_length** ([**msg\_desc**](structmsg__desc.md))


## n

* **node\_address** ([**baton\_node**](structbaton__node.md))
* **node\_count** ([**baton\_node**](structbaton__node.md))
* **now\_ms** ([**baton\_node**](structbaton__node.md))


## o

* **others\_mask** ([**baton\_node**](structbaton__node.md))
* **on\_drop** ([**baton\_node\_config**](structbaton__node__config.md))
* **on\_message** ([**baton\_node\_config**](structbaton__node__config.md))
* **on\_poll\_done** ([**baton\_node\_config**](structbaton__node__config.md))
* **own\_address** ([**baton\_node\_config**](structbaton__node__config.md))


## p

* **payload** ([**baton\_frame**](structbaton__frame.md))
* **parser** ([**baton\_link**](structbaton__link.md))
* **poll\_seq** ([**baton\_node**](structbaton__node.md))
* **pending** ([**baton\_record**](structbaton__record.md))


## q

* **queue** ([**baton\_node**](structbaton__node.md))
* **queue\_high\_memory** ([**baton\_node**](structbaton__node.md))
* **queue\_normal\_memory** ([**baton\_node**](structbaton__node.md))
* **queue\_full** ([**baton\_stats**](structbaton__stats.md))
* **queue\_high\_used** ([**baton\_stats**](structbaton__stats.md))
* **queue\_normal\_used** ([**baton\_stats**](structbaton__stats.md))


## r

* **resp\_seq** ([**baton\_channel**](structbaton__channel.md))
* **resp\_valid** ([**baton\_channel**](structbaton__channel.md))
* **rx\_last\_first** ([**baton\_channel**](structbaton__channel.md))
* **rx\_seq** ([**baton\_channel**](structbaton__channel.md))
* **rx\_valid** ([**baton\_channel**](structbaton__channel.md))
* **rx\_buffer** ([**baton\_link**](structbaton__link.md))
* **rx\_head** ([**baton\_link**](structbaton__link.md))
* **rx\_overflows** ([**baton\_link**](structbaton__link.md), [**baton\_stats**](structbaton__stats.md))
* **rx\_tail** ([**baton\_link**](structbaton__link.md))
* **rx\_echo** ([**baton\_link\_config**](structbaton__link__config.md), [**baton\_node\_config**](structbaton__node__config.md))
* **rx\_frames** ([**baton\_link\_counters**](structbaton__link__counters.md), [**baton\_stats**](structbaton__stats.md))
* **rx\_timeouts** ([**baton\_link\_counters**](structbaton__link__counters.md), [**baton\_stats**](structbaton__stats.md))
* **retries** ([**baton\_node**](structbaton__node.md), [**baton\_stats**](structbaton__stats.md))
* **role** ([**baton\_node\_config**](structbaton__node__config.md))
* **resp\_timeouts** ([**baton\_stats**](structbaton__stats.md))


## s

* **src** ([**baton\_frame**](structbaton__frame.md))
* **self\_index** ([**baton\_node**](structbaton__node.md))
* **slot\_k** ([**baton\_node**](structbaton__node.md))
* **slot\_nack** ([**baton\_node**](structbaton__node.md))
* **slot\_pass** ([**baton\_node**](structbaton__node.md))
* **slot\_peer** ([**baton\_node**](structbaton__node.md))
* **slot\_rr** ([**baton\_node**](structbaton__node.md))
* **slot\_start** ([**baton\_node**](structbaton__node.md))
* **state\_data** ([**baton\_node**](structbaton__node.md))
* **state\_dirty** ([**baton\_node**](structbaton__node.md))
* **state\_length** ([**baton\_node**](structbaton__node.md))
* **stats** ([**baton\_node**](structbaton__node.md))
* **slave\_addresses** ([**baton\_node\_config**](structbaton__node__config.md))
* **slave\_count** ([**baton\_node\_config**](structbaton__node__config.md))
* **slot\_time\_ms** ([**baton\_node\_config**](structbaton__node__config.md))
* **state** ([**baton\_parser**](structbaton__parser.md))
* **size** ([**baton\_queue**](structbaton__queue.md))
* **stamp** ([**baton\_record**](structbaton__record.md))


## t

* **tx\_first** ([**baton\_channel**](structbaton__channel.md))
* **tx\_seq** ([**baton\_channel**](structbaton__channel.md))
* **tx\_data** ([**baton\_link**](structbaton__link.md))
* **tx\_done\_flag** ([**baton\_link**](structbaton__link.md))
* **tx\_length** ([**baton\_link**](structbaton__link.md))
* **tx\_start\_ms** ([**baton\_link**](structbaton__link.md))
* **tx\_state** ([**baton\_link**](structbaton__link.md))
* **tx\_gap\_ms** ([**baton\_link\_config**](structbaton__link__config.md), [**baton\_node\_config**](structbaton__node__config.md))
* **tx\_frames** ([**baton\_link\_counters**](structbaton__link__counters.md), [**baton\_stats**](structbaton__stats.md))
* **tx\_timeouts** ([**baton\_link\_counters**](structbaton__link__counters.md), [**baton\_stats**](structbaton__stats.md))
* **tx\_frame** ([**baton\_node**](structbaton__node.md))
* **tail** ([**baton\_queue**](structbaton__queue.md))


## w

* **write** ([**baton\_link\_config**](structbaton__link__config.md), [**baton\_node\_config**](structbaton__node__config.md))




