---
name: intercom-core-change
description: Rules and invariants for modifying the intercom library core (src/ic_core.c, ic_queue.c, ic_internal.h, public API in include/Intercom.h) — protocol, frame format, queues, master/slave state machines, packing, deduplication, timing. Use before changing any library source or adding a feature to the protocol.
---

# Changing the intercom library core

First read `docs/protocol.md` and `docs/architecture.md`. They describe the behaviour you are changing.

## Procedure

1. Determine whether the change affects the **wire format**: frame, CTRL, POLL/RESP prefixes, TLV, ACK semantics. If so:
   - all nodes must run the same version, so bump the major version: the `vX.Y.Z` Git tag (read by `cmake/version.cmake`) together with `INTERCOM_DEFAULT_VERSION` there and the fallback `INTERCOM_VERSION_*` values in `include/Intercom.h`,
   - update `docs/protocol.md`.
2. Check the change against the **invariants** below. If it breaks any of them, stop and discuss it with the user.
3. Implement it in the project style: C99, no `malloc`, no busy-wait loops, explicit casts, `u` suffixes.
4. Add or extend a scenario in `tests/sim_test.c` that would catch a bug in this change.
5. Verify with the `intercom-testing` skill: a warning-free build and ≥ 100 seeds with ASan/UBSan.
6. Update the documentation:
   - `docs/api.md` when the API or callbacks change,
   - `docs/configuration.md` for new fields or macros,
   - `docs/architecture.md` for structures, FSMs or memory,
   - the "Decyzje projektowe" (design decisions) section in `CLAUDE.md` when the decision is non-obvious.

## Invariants (do not break)

### Determinism and resources

- **N1.** No function blocks. The execution time of `ic_process` is bounded by buffer sizes, not by bus state.
- **N2.** Memory is static only, inside `ic_t`, and sizes come from `IntercomConfig.h`. Add new limits as macros with `#error` checks in `ic_internal.h`.
- **N3.** User callbacks are called only from `ic_process`. ISR functions (`ic_rx_byte_isr`, `ic_tx_done_isr`) only write to a buffer or a flag.

### Bus access

- **N4.** Only the polled slave transmits, within its slot. Other slaves send only ACKs for DATA addressed to them, and only in `IC_L_IDLE`.
- **N5.** A slave sends DATA only if `slave_budget()` leaves time for an ACK and a maximum RESP. The RESP must finish within `slotTimeMs` of the POLL. Any new step in the slot must be included in the budget.
- **N6.** The master's RESP timeout (`slotTimeMs + txGapMs + 10`, extended up to 2× while a frame is being received) must be ≥ the slave's budget. The minimum slot is checked by `min_slot_ms()` in `ic_init`.

### Reliability (stop-and-wait)

- **N7.** There is at most one unacknowledged frame with content per channel (node → node). Its content is the `chan.inflight[]` list.
- **N8.** A retransmission has **the same SEQ** and is rebuilt from `inflight[]` (the rebuild branch of `pack()`). New messages must not be added to a frame with the same SEQ.
- **N9.** SEQ advances in only two cases:
  - on commit (`chan_commit`, after an acknowledgement),
  - when content is abandoned (`chan_abandon`, the `inflight` list emptied by TTL or by state removal).
- **N10.** Frames **without content** do not take part in sequencing: the recipient does not update `rxSeq` (`rx_content`, `n == 0`), and the master sets `respSeq/respValid` only for a RESP with content. *Breaking this causes data loss: an empty RESP acknowledged the next one in advance (a v1.0 bug before the fix).*
- **N11.** `ACKB` in RESP acknowledges the POLL **of the same slot**. `ACKB` in POLL acknowledges the last RESP with content.
- **N12.** A commit requires a matching SEQ **and** `nInflight > 0`.
- **N13.** A duplicate (same SEQ) is acknowledged again but not delivered. The FIRST flag handles a sender restart.

### Event queues

- **N14.** A record is released only from `head` and only when `pending == 0 && inflight == 0`. References (offsets) in `chan.inflight[]` must always be valid.
- **N15.** TTL expiry first removes the references from channels (`chan_drop_ref` for every `inflight` bit), only then zeroes `pending/inflight` and calls `onDrop` with the `pending` mask from before it was zeroed.
- **N16.** Queue traversal uses a snapshot of `head` and `tail` taken **inside a critical section** (`ic_q_first(q, head, tail)`). The producer (`ic_q_alloc`) resets an empty queue to 0. `q_reclaim` writes `head` only if it has not changed since the snapshot.
- **N17.** A record becomes visible to the consumer only once `IC_REC_VALID` is set, which happens after the data has been copied. Traversal stops at the first record without `VALID`.
- **N18.** Packing preserves FIFO within a queue: when a pending record for the recipient does not fit, it does `break`, not `continue`. Only records not addressed to the recipient or already in flight to it may be skipped.
- **N19.** Records are not split at the end of the buffer (SKIP marker, or an implicit wrap when there is no room for the header).

### States

- **N20.** `Publish` never fails for lack of space. The `dirty` bit is cleared during packing. If a state drops out of a rebuilt frame, its `dirty` bit must be restored.

### Unreachable nodes

- **N21.** An unreachable node must not take budget away from reachable ones:
  - pass 0 serves nodes with `failStreak < IC_DEAD_STREAK` in round-robin order,
  - pass 1 serves the rest, without retry.
- **N22.** Every successful acknowledgement resets `failStreak`. The master skips an unreachable node except every `IC_DEAD_POLL_DIV`-th cycle; the skip loop is bounded.

## Where things are in `ic_core.c`

| Area | Functions |
| --- | --- |
| Configuration | `ic_default_cfg`, `ic_init`, `min_slot_ms` |
| Channels | `chan_commit`, `chan_abandon`, `chan_drop_ref` |
| Packing | `pack`, `try_emit_state`, `emit` |
| Receive | `rx_drain` (echo), `parser_byte`, `handle_frame`, `rx_content` (dedup), `deliver` |
| Transmitter | `tx_request`, `tx_service`, `tx_finish` → `link_on_tx_done` |
| Slave | `slave_on_poll`, `slave_slot_step`, `slave_tx_data`, `slave_budget`, `slave_on_ack`, `slave_ack_timeout`, `slave_send_resp`, `slave_on_data` |
| Master | `master_service`, `master_send_poll`, `master_on_resp`, `master_poll_done` |
| Queues | `q_reclaim`, `expire_events`, `housekeeping` |
| Instance API | `ic_send`, `ic_publish`, `ic_get_stats`, `ic_process` |

## Common pitfalls

- Adding a source file to `src/` without listing it in `INTERCOM_SOURCES` in `intercom.cmake`.
- Adding a field to `IntercomCfg` without a default value in `ic_default_cfg` and without validation in `ic_init`.
- A new frame type in CTRL: there are only 2 type bits (all 4 types are taken). For extensions use the reserved bit `CTRL[4]` or a payload prefix, and bump the version.
- Changing `IC_RESP_PREFIX` changes `IC_MAX_MSG_LEN`, and with it the `MAX_LEN` limit for messages in user configurations (compile-time check).
- Variables shared with an ISR must be `volatile`, and multi-field operations must be inside `lock()/unlock()`.
