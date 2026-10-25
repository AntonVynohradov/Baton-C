# Extending the Library

Almost everything an application needs is done on top of the library: new messages are
configuration, new transports are a write function and an interrupt handler, data
models are application code. This chapter says where each extension belongs and how to
contribute changes to the library itself.

## Where to extend

| You want to… | Do it in | Library change needed? |
| --- | --- | --- |
| Add a message | `BATON_STATE_LIST` / `BATON_EVENT_LIST` of `baton_user_config.h` on every node | No |
| Larger frames, more nodes, bigger queues | The sizes in `baton_user_config.h` | No |
| Support another UART, RS-485 or a USB adapter | The write function and the receive interrupt | No |
| Give message data a structure | Encode and decode functions in the application | No |
| Monitor a bus | The codec alone (`baton_parser_feed()`) | No |
| Change timing (slot, timeouts, baud rate) | `struct baton_node_config` | No |
| Change the wire format or the delivery rules | `src/`, the Python twin, the protocol chapter | Yes — a new major version |

## Adding a message

1. Add `X(NAME, ID, MAX_LEN)` to the state or event list with a new ID.
2. Rebuild **every node** of the bus with the new list. A node with an old list counts
   the new message in `bad_messages` and ignores it.
3. Write an encode and a decode function for its data with an explicit byte order.

Changing the `MAX_LEN` of a message is compatible as long as the senders never use more
than the smallest `MAX_LEN` among the nodes.

## Adding a transport

The write function contract:

| Requirement | Why |
| --- | --- |
| Start the transmission and return 0, or return a negative value | The node never blocks; a failure is counted in `tx_timeouts` and treated as sent |
| Keep sending from the given buffer | The library keeps it unchanged until the end of the transmission |
| End: last echo byte (`rx_echo = true`) or `baton_node_tx_done_isr()` | The link uses it to start the next timer and to free the bus |
| Called only from `baton_node_process()` | It may use the same resources as the processing context |

Received bytes of every node — including the own echo — go to `baton_node_rx_isr()`.

## Data models

Keep the application data out of the library: define, per message, a C structure and
two functions that convert it to and from bytes. The examples encode the uptime with
`baton_put_u32le()` and decode it with `baton_get_u32le()` from `baton_defs.h`.

## Several buses

Use one `struct baton_node` per bus. Nodes share nothing, but they share the message
lists of the build; buses with different message lists need separate builds of the
library (for example two CMake targets with different `BATON_CONFIG_DIR`).

## Bridging buses

A gateway node is a member of two buses (two `struct baton_node`) and forwards in its
`on_message` callbacks: an event received on bus A is sent with `baton_node_send()` on
bus B. Delivery is then exactly once per hop; end-to-end confirmation, if needed, is an
application message (like `OUTPUT_REPORT` in the examples).

## Contributing to the library

1. **Code.** C11, no new dependencies, no heap, the MISRA rules used in the code (single
   exit, parenthesized expressions, no pointer arithmetic, `(void)` on ignored results).
2. **Tests.** Every behavior change needs a test; `coverage` must stay at 100 % lines and
   branches; the bus simulation must pass on 100 seeds (`tests/run_seeds.sh`). Update the
   Python twin when the codec changes.
3. **Analysis.** `misra-analyze` reports 0 violations, or the deviation is recorded in
   [MISRA C:2012](misra.md).
4. **Format.** `clang-format` reports no changes.
5. **Documentation.** The affected pages, the stack and size tables
   ([Configuration](configuration.md)), the diagrams (`tools/docs/diagrams.py`), and
   `CHANGELOG.md` under `[Unreleased]`.
6. **Compatibility.** The public API and the wire format stay stable within a major
   version.

Local definition of done:

```bash
cmake --preset "BATON[Debug]" && cmake --build "build/BATON[Debug]"
cmake --preset "BATON[UnitTests]" && cmake --build "build/BATON[UnitTests]" --target coverage
cmake --build "build/BATON[UnitTests]" --target baton_sim && tests/run_seeds.sh "build/BATON[UnitTests]" 100
cmake --preset "BATON[IntegrationTests]" && cmake --build "build/BATON[IntegrationTests]" --target integration_tests
cmake --build "build/BATON[Debug]" --target misra-analyze
cmake --preset "BATON[Examples]" && cmake --build "build/BATON[Examples]" && ctest --test-dir "build/BATON[Examples]" -L example
```
