# Zephyr Module

Baton-C is a [Zephyr](https://zephyrproject.org/) module. Add the repository to the west
manifest of an application, describe the node in devicetree, and the module creates the
node, owns the UART interrupts and runs the protocol thread. The application only sends,
publishes and handles callbacks.

| | |
| --- | --- |
| Module files | `zephyr/` (`module.yml`, `Kconfig`, `CMakeLists.txt`, `baton_zephyr.c`, `include/baton_zephyr.h`) |
| Devicetree binding | `zephyr/dts/bindings/baton/systemfromscratch,baton.yaml` |
| Sample | `zephyr/samples/baton_node`: a slave or master of the example bus |
| Test | `zephyr/tests/bus`: master and two slaves in one `native_sim` image |
| Zephyr versions | 4.2 and later (CI builds against `main`) |
| UART driver | Any driver with the interrupt-driven API (`CONFIG_UART_INTERRUPT_DRIVEN`) |

## Adding the module

Add Baton-C to the `projects:` of the application's `west.yml` and run `west update`:

```yaml
manifest:
  projects:
    - name: baton-c
      url: https://github.com/AntonVynohradov/Baton-C
      revision: v1.1.0
      path: modules/lib/baton-c
```

Without west, pass the repository to CMake: `-DZEPHYR_EXTRA_MODULES=/path/to/Baton-C`.

## Describing the node

A Baton node is a child of the UART it uses. The bit rate is the `current-speed` of that
UART:

```dts
&uart1 {
	status = "okay";
	current-speed = <9600>;

	baton0: baton {
		compatible = "systemfromscratch,baton";
		role = "slave";
		address = <0x12>;
		master-address = <0x01>;
		slave-addresses = [11 12];
	};
};
```

| Property | Required | Meaning |
| --- | --- | --- |
| `role` | yes | `"master"` or `"slave"` |
| `address` | yes | Address of this node (a master uses its `master-address`) |
| `master-address` | yes | Address of the master |
| `slave-addresses` | yes | Slaves in polling order, identical on every node |
| `de-gpios` | no | RS-485 driver enable: asserted when a frame starts, released on the echo of its last byte |
| `slot-time-ms`, `ack-timeout-ms`, `max-retries`, `tx-gap-ms`, `inter-byte-timeout-ms`, `cycle-period-ms`, `event-ttl-ms` | no | Timing of `struct baton_node_config`; the library default when absent |

Several nodes, on several UARTs, are allowed: each enabled node becomes its own device with
its own thread. The bus test runs three nodes in one image this way.

The UART must receive its own bytes: single-wire mode, an open-drain TX tied to RX with a
pull-up, or an RS-485 transceiver whose receiver stays enabled while it transmits (see
[The physical layer](integration.md#the-physical-layer)). The module runs the library with
`rx_echo`: the echo of the last byte ends a frame and a wrong echo is counted as a
collision. Keep the console and logging off the bus UART.

## Configuration

`CONFIG_BATON` is enabled automatically when the devicetree has an enabled
`systemfromscratch,baton` node.

| Kconfig option | Default | Meaning |
| --- | --- | --- |
| `CONFIG_BATON_USER_CONFIG_DIR` | `"baton"` | Folder with the application's `baton_user_config.h`, relative to the application |
| `CONFIG_BATON_THREAD_STACK_SIZE` | 1024 | Stack of the protocol thread of each node |
| `CONFIG_BATON_THREAD_PRIORITY` | 5 | Preemptible priority of the protocol thread |
| `CONFIG_BATON_PROCESS_PERIOD_MS` | 1 | Longest time between two protocol steps |
| `CONFIG_BATON_INIT_PRIORITY` | 80 | `POST_KERNEL` priority, after the UART and GPIO drivers |
| `CONFIG_BATON_LOG_LEVEL_*` | default | Log level of the module |

`baton_user_config.h` (message lists and buffer sizes) belongs to the application: copy
`config/baton_user_config_template.h`, as for any other integration. Every node of the bus
needs the same message lists.

The build stops with an error when `CONFIG_SYS_CLOCK_TICKS_PER_SEC` is below 1000. The
library times its bus gap (2 ms) and inter-byte timeout (5 ms) in milliseconds; a board
whose default tick is 10 ms needs `CONFIG_SYS_CLOCK_TICKS_PER_SEC=1000` in `prj.conf`.

## Using the node

The code below is a shortened form of the sample, which CI builds:

```c title="Simplified from zephyr/samples/baton_node/src/main.c"
#include "baton_zephyr.h"

static const struct device* const bus =
    DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(systemfromscratch_baton));

static void on_message(void* context, uint8_t src, uint8_t msg_id, const uint8_t* data,
                       uint8_t length)
{
    if ((msg_id == BATON_MSG_CMD_SET_OUTPUT) && (length == 2U))
    {
        /* Drive the output, then confirm to the sender. */
        (void) baton_zephyr_send(bus, src, BATON_MSG_OUTPUT_REPORT, data, 2U, BATON_PRIO_HIGH);
    }
}

int main(void)
{
    const struct baton_zephyr_callbacks callbacks = {.on_message = on_message};
    uint8_t status[4];

    (void) baton_zephyr_start(bus, &callbacks);
    for (;;)
    {
        k_sleep(K_SECONDS(1));
        baton_put_u32le(status, k_uptime_get_32() / 1000U);
        (void) baton_zephyr_publish(bus, BATON_MSG_NODE_STATUS, status, sizeof(status));
    }
}
```

| Function | Meaning |
| --- | --- |
| `baton_zephyr_start(dev, callbacks)` | Registers the callbacks, enables reception, starts the protocol thread |
| `baton_zephyr_send(dev, dst, msg_id, data, length, priority)` | `baton_node_send()`; any thread |
| `baton_zephyr_publish(dev, msg_id, data, length)` | `baton_node_publish()`; any thread |
| `baton_zephyr_get_stats(dev, stats)` | Copy of the diagnostic counters |

The functions return 0 or a negative errno: `-ENOMSG` (unknown message or wrong class),
`-EMSGSIZE` (too long), `-EADDRNOTAVAIL` (unknown address), `-ENOBUFS` (queue full, try
again later), `-EINVAL`, `-ENODEV` (the device did not initialize). Callbacks run in the
protocol thread of the node; post longer work to an application thread.

## How the module works

| Part | Zephyr mechanism |
| --- | --- |
| Receive | The UART interrupt passes every byte to `baton_node_rx_isr()` and wakes the protocol thread |
| Transmit | The write hook fills the TX FIFO from the frame the library keeps valid; the TX interrupt is disabled after the last byte |
| End of frame | The echo of the last byte; it also releases `de-gpios` |
| Critical section | A `k_spinlock` per node, taken by the UART interrupt and by the library hooks, so `send` and `publish` are safe from any thread, also on SMP |
| Time | `k_uptime_get_32()` |
| Processing | One thread per node: `baton_node_process()`, then `k_sem_take()` with a `CONFIG_BATON_PROCESS_PERIOD_MS` timeout, given by received bytes and new messages |
| Memory | `struct baton_node` and the thread stack are static per node; no heap |

Measured for the sample on the M2S010 MKR Kit (Cortex-M3, `-Os`, Zephyr 4.5):

| Item | Size |
| --- | --- |
| Library code (`src/`) | 5986 B |
| Module glue (`baton_zephyr.c`, with logging) | 1596 B |
| Node state with the example configuration | 1256 B RAM |
| Protocol thread stack | 1024 B RAM (`CONFIG_BATON_THREAD_STACK_SIZE`) |

## Testing on a PC

`zephyr/tests/bus` builds the master and both slaves of the example bus into one
`native_sim` image. Each node has its own emulated UART (`zephyr,uart-emul`); the test joins
them into one wire that delivers every byte to every connected node, the sender included.
The wire can corrupt bytes and disconnect a node:

| Test | Checks |
| --- | --- |
| `test_state_reaches_every_node` | A state published by a slave arrives at the master and the other slave |
| `test_events_master_to_slave_exactly_once` | 60 events, each delivered once and in order |
| `test_events_slave_to_slave_exactly_once` | The same between two slaves |
| `test_noisy_line_loses_nothing` | About one corrupted byte in 150: retransmissions deliver all 200 events, CRC errors are counted |
| `test_heavy_noise_every_loss_is_reported` | About one corrupted byte in 40: no event duplicated or reordered, and every event that misses its TTL is reported to its sender |
| `test_disconnected_slave_is_reported_and_recovers` | A cut slave is reported as silent, its event as undeliverable, the rest of the bus keeps working, and the slave is served again after reconnection |
| `test_api_errors`, `test_start_twice` | Error codes of the API |

Five scenarios run the suite with different noise patterns (`CONFIG_BATON_TEST_SEED`):

```bash
west init -l --mf zephyr/west.yml Baton-C && west update -o=--depth=1 -n
west twister -T Baton-C/zephyr -p native_sim/native/64 -p m2s010_mkr_kit/m2s010
```

The [Zephyr Module workflow](https://github.com/AntonVynohradov/Baton-C/actions/workflows/zephyr-module.yml)
runs the same command on every push.

## Limits

- The module needs the UART echo; RS-485 transceivers that disable their receiver while
  sending (no echo) are not supported by the module yet. The plain library supports them
  through `baton_node_tx_done_isr()`.
- `event-ttl-ms = <0>` selects the library default, not "never"; leave the TTL to the
  library or give a value.
- The module has not been run on hardware yet; see the roadmap in the README.
