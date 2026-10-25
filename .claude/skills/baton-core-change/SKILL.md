---
name: baton-core-change
description: Rules, invariants and definition of done for modifying the Baton-C library (src/, inc/, baton.h) — wire format, link, queues, master/slave state machines, packing, deduplication, timing, public API. Use before changing any library source or adding a feature to the protocol.
---

# Changing the Baton-C library

Baton-C follows the SystemFromScratch® protocol-library blueprint (skill
`sfs-protocol-library`, reference implementation DXL2-C). First read `docs/protocol.md`
and `docs/architecture.md`: they describe the behavior you are changing.

## Scope

| Part | Files | Rule |
| --- | --- | --- |
| Umbrella | `baton.h` | Includes all public headers; fallback `BATON_VERSION_*` |
| Configuration | `inc/baton_config.h` | Sizes `#ifndef`-guarded and range-checked with `#error`; includes the application's `baton_user_config.h` |
| Definitions | `inc/baton_defs.h` | Wire constants and `static inline` helpers only, no state |
| Codec | `inc/baton_protocol.h`, `src/baton_protocol.c` | CRC-16/CCITT-FALSE, parser, encoder; pure functions |
| Link | `inc/baton_link.h`, `src/baton_link.c` | RX ring, parser timeout, transmitter (gap, echo, timeout); reports events |
| Queue | `inc/baton_queue.h`, `src/baton_queue.c` | Variable-size records, serialized header |
| Node | `inc/baton_node.h`, `src/baton_node.c` | FSMs, channels, packing, dedup, TTL, application API |
| Python twin | `tests/integration/baton/protocol.py` | Standard library only; must stay byte-identical to the C codec |

## Hard rules

- C11, no heap, no RTOS, no global mutable state, no clock reads (time comes in as
  `now_ms`); only `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`, `<string.h>`.
- Public symbols `baton_` / `BATON_`; structures `struct baton_…` (no typedef'd structs);
  Doxygen on every public item; `extern "C"` guards.
- MISRA C:2012: one `return` per function, parenthesized expressions, `(void)` on ignored
  results, no `++` inside expressions, array indexing instead of pointer arithmetic, no
  pointer casts between object types, no `break` out of loops (use Boolean flags).
  `misra-analyze` must report 0; a new deviation needs an entry in `docs/misra.md` and a
  `cppcheck-suppress` comment naming it.
- No unreachable code: 100 % line **and** branch coverage is a gate. If a branch cannot be
  reached, remove it and document the invariant in a comment (see `pack_events()`,
  `queue_reclaim()`, the echo check in `baton_link_receive()`).
- `-Wall -Wextra -Wpedantic -Wconversion` clean (tests build with `-Werror`).
- Style: `.clang-format`; file banners and section separators as in the existing files.
- Documentation numbers are measured; re-measure after changes (stack, size, RAM).

## Procedure

1. Wire format change (frame, CTRL, POLL/RESP prefixes, TLV, ACK semantics)? Every node
   must run the same version: bump the **major** version (`BATON_DEFAULT_VERSION` in
   `cmake/version.cmake` and the fallbacks in `baton.h`), update `docs/protocol.md`, the
   Python twin and the reference frames.
2. Check the change against the invariants below. If one breaks, stop and discuss it.
3. Implement; add a unit test (`tests/unit/gtest_baton_*.cpp`, register it in
   `baton.cmake`) and, for behavior across nodes, a simulation scenario
   (`tests/sim/baton_bus_sim.c`).
4. Verify with the `baton-testing` skill (definition of done below).
5. Update the docs: `api.md`, `usage.md`, `configuration.md` (sizes, stack, code size —
   re-measure), `architecture.md`, diagrams (`tools/docs/diagrams.py`), and
   `CHANGELOG.md` under `[Unreleased]`.

## Invariants (do not break)

### Determinism and resources

- **N1.** No function blocks. `baton_node_process()` is bounded by the buffer sizes.
- **N2.** Memory only inside `struct baton_node`, sized by `baton_user_config.h`. New
  limits get `#error` / `_Static_assert` checks.
- **N3.** Callbacks run only inside `baton_node_process()`. The ISR entry points only
  write the ring or a flag.

### Bus access

- **N4.** Only the polled slave transmits, within its slot. Other slaves send only ACKs to
  DATA addressed to them, and only in `L_IDLE`.
- **N5.** A slave sends DATA only if `slave_budget()` leaves time for an ACK and a maximum
  RESP; the RESP ends within `slot_time_ms` of the POLL. Every new step in the slot goes
  into the budget.
- **N6.** The master's RESP timeout (`slot_time_ms + tx_gap_ms + 10`, up to 2× while a
  frame is being received) is ≥ the slave budget; `min_slot_ms()` is checked in init.

### Reliability (stop-and-wait)

- **N7.** At most one unacknowledged frame with content per channel; its content is
  `channel.inflight[]`.
- **N8.** A retransmission has the same SEQ and is rebuilt from `inflight[]`
  (`pack_again()`); no new messages join a frame with the same SEQ.
- **N9.** SEQ advances only on commit (`chan_commit()`) or when content is abandoned
  (`chan_abandon()`: list emptied by TTL or state removal).
- **N10.** Frames without content do not take part in sequencing: `rx_content()` ignores
  length 0, and the master sets `resp_seq/resp_valid` only for a RESP with content.
  *Breaking this loses data: an empty RESP acknowledged the next one in advance (a bug
  found by the simulation in the predecessor `intercom`).*
- **N11.** ACKB in RESP acknowledges the POLL of the same slot; ACKB in POLL the last RESP
  with content.
- **N12.** A commit needs a matching SEQ **and** `inflight_count > 0`.
- **N13.** A duplicate is acknowledged again but not delivered; FIRST handles a restart.

### Event queues

- **N14.** Records are released only from the head, when `pending == 0`. `inflight` ⊆
  `pending` always (a bit is set only with a channel reference and cleared together).
- **N15.** TTL expiry removes the channel references first (`chan_drop_ref()` for each
  inflight bit), then clears the masks, then calls `on_drop` with the old pending mask.
- **N16.** Traversals use a head/tail snapshot taken inside the critical section. Only
  `queue_reclaim()` moves the head; the producer resets an empty queue to 0.
- **N17.** A record is visible only once `BATON_REC_VALID` is set, after the data copy;
  traversal stops at the first record without it.
- **N18.** Packing keeps FIFO within a queue: a record for the peer that does not fit stops
  the queue (never skip ahead).
- **N19.** Records are never split at the end of the array (SKIP marker or implicit wrap).

### States and unreachable nodes

- **N20.** Publish never fails for lack of space. A state removed from a rebuilt frame gets
  its dirty bit back.
- **N21.** Pass 0 serves peers with `fail_streak < BATON_DEAD_STREAK` round robin; pass 1
  the rest, without retry. A peer in the slot's NACK mask is not served again.
- **N22.** Every acknowledgement resets `fail_streak`. The master polls an unreachable
  slave only every `BATON_DEAD_POLL_DIV`-th cycle; the skip loop is bounded.

## Where things are in `src/baton_node.c`

| Area | Functions |
| --- | --- |
| Configuration | `baton_node_default_config`, `baton_node_init`, `config_valid`, `topology_valid`, `timing_valid`, `min_slot_ms` |
| Channels | `chan_commit`, `chan_abandon`, `chan_drop_ref`, `chan_remove_at` |
| Packing | `pack`, `pack_again`, `pack_events`, `pack_states`, `try_emit_state`, `emit` |
| Receive | `handle_frame`, `rx_content` (dedup), `deliver` |
| Slave | `slave_on_poll`, `slave_slot_step`, `slave_tx_data`, `slave_budget`, `slave_on_ack`, `slave_ack_timeout`, `slave_send_resp`, `slave_on_data` |
| Master | `master_service`, `master_send_poll`, `master_on_resp`, `master_poll_done` |
| Housekeeping | `queue_reclaim`, `expire_events`, `expire_record`, `housekeeping` |
| Link glue | `tx_request`, `on_tx_done`, `link_service`, `baton_node_process` |

## Common pitfalls

- A new source file not listed in `BATON_SOURCES` of `baton.cmake`.
- A new config field without a default in `baton_node_default_config()` and without
  validation in `baton_node_init()`.
- CTRL has 2 type bits, all used: extend with the reserved bit `CTRL[4]` or a payload
  prefix, and bump the major version.
- Changing `BATON_RESP_PREFIX` changes `BATON_MAX_MSG_LEN` and the compile-time limit of
  application messages.
- Variables shared with an ISR are `volatile`; multi-field updates go inside `lock()` /
  `unlock()`.
- MISRA 10.x vs `-Wconversion`: compute in the wide type first (`(uint16_t) x + 2U`, then
  cast the sum to the same type), never cast a composite expression to a wider type.

## Definition of done

```sh
cmake --preset "BATON[Debug]" && cmake --build "build/BATON[Debug]"
cmake --preset "BATON[UnitTests]" && cmake --build "build/BATON[UnitTests]" --target coverage
cmake --build "build/BATON[UnitTests]" --target baton_sim && tests/run_seeds.sh "build/BATON[UnitTests]" 100
cmake --preset "BATON[IntegrationTests]" && cmake --build "build/BATON[IntegrationTests]" --target integration_tests
cmake --build "build/BATON[Debug]" --target misra-analyze
cmake --preset "BATON[Examples]" && cmake --build "build/BATON[Examples]" && ctest --test-dir "build/BATON[Examples]" -L example
clang-format --dry-run --Werror baton.h src/*.c inc/*.h
python tools/docs/check_snippets.py && mkdocs build
```

Commit, tag and push only when the maintainer asks.
