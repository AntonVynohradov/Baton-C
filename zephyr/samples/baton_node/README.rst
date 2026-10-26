.. _baton_node_sample:

Baton-C node
############

Overview
********

One node of the Baton-C example bus (master ``0x01``, slaves ``0x11`` and
``0x12``). The role and the address come from the ``systemfromscratch,baton``
devicetree node:

* a **slave** applies ``CMD_SET_OUTPUT`` to ``led0``, answers with
  ``OUTPUT_REPORT`` and publishes its uptime as the ``NODE_STATUS`` state every
  second;
* a **master** toggles output 0 of every slave once a second and prints the
  reports, the states and the slots in which a slave did not answer.

The message lists are ``examples/config/baton_user_config.h``, the same as the
PC master in ``examples/host_serial``, so a PC with a USB-UART adapter can be
the master of a Zephyr slave.

Building
********

.. code-block:: console

   west build -b m2s010_mkr_kit/m2s010 baton-c/zephyr/samples/baton_node

Wiring
******

All nodes share one line: an open-drain TX tied to RX with a pull-up, or
RS-485 transceivers whose receivers stay enabled while transmitting. The UART
must receive its own bytes; the echo ends every frame and detects collisions.
