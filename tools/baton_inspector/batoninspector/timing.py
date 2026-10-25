#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - measures slots, response times and cycles
#              and reports timing and bus-usage violations.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Timing analysis.

Measurements (all from frame end times, see decoder.py for the time stamps):

=============  ===========================================================  ==========
Quantity       Definition                                                    Limit
=============  ===========================================================  ==========
slot           end of the POLL to slave S → end of the RESP of S             slot_max_ms
ACK time       end of a DATA frame → end of its ACK                          ack_max_ms
cycle          start of the first POLL of a polling round → start of the      cycle_max_ms,
               first POLL of the next round                                  cycle_min_ms
=============  ===========================================================  ==========

Rules (``Violation.rule``):

* error: ``slot_overrun``, ``out_of_slot`` (a slave transmits DATA or RESP while
  another one holds the baton), ``overlap`` (two frames at the same time),
  ``invalid_sender`` (POLL not from the master, RESP not to the master),
  ``cycle_too_long``
* warning: ``missing_resp``, ``slave_unresponsive`` (``unanswered_polls`` POLLs in a
  row without RESP), ``late_ack``, ``missing_ack``, ``unexpected_resp``,
  ``unexpected_ack``, ``unknown_node``, ``crc_error``, ``frame_timeout``,
  ``cycle_too_short``
* info: ``retransmission``

Every limit is compared with ``limit + tolerance_ms``.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Tuple

from .busconfig import BusConfig, TimingRules
from .model import (SEVERITY_ERROR, SEVERITY_INFO, SEVERITY_WARNING, ErrorRecord, FrameRecord,
                    Violation)


@dataclass
class Series:
    """Measured values of one quantity."""

    values: List[Tuple[float, float]] = field(default_factory=list)   # (t_ms, value_ms)

    def add(self, t_ms: float, value: float) -> None:
        self.values.append((t_ms, value))

    def summary(self) -> Dict[str, float]:
        if not self.values:
            return {"count": 0}
        data = [v for _, v in self.values]
        return {"count": len(data), "min_ms": round(min(data), 3),
                "avg_ms": round(sum(data) / len(data), 3), "max_ms": round(max(data), 3)}


class TimingAnalyzer:
    """Feed frames and errors in time order; violations go to ``on_violation``."""

    def __init__(self, config: BusConfig, rules: TimingRules,
                 on_violation: Callable[[Violation], None]) -> None:
        self.config = config
        self.rules = rules
        self.on_violation = on_violation
        self.slots: Dict[int, Series] = {s: Series() for s in config.slaves}
        self.acks = Series()
        self.cycles = Series()
        self._poll: Optional[FrameRecord] = None
        self._holder: Optional[int] = None
        self._unanswered: Dict[int, int] = {s: 0 for s in config.slaves}
        self._pending_data: Dict[Tuple[int, int], FrameRecord] = {}
        self._last_frame: Optional[FrameRecord] = None
        self._round_start: Optional[float] = None
        self._last_poll_index = -1

    # ---- helpers -----------------------------------------------------------

    def _report(self, t: float, rule: str, severity: str, text: str, node: Optional[int] = None,
                measured: Optional[float] = None, limit: Optional[float] = None,
                frames: Optional[List[int]] = None) -> None:
        self.on_violation(Violation(t, rule, severity, text, node, measured, limit, frames or []))

    def _name(self, address: int) -> str:
        return self.config.node_name(address)

    # ---- input ---------------------------------------------------------------

    def on_error(self, error: ErrorRecord) -> None:
        if error.kind == "crc":
            self._report(error.t_ms, "crc_error", SEVERITY_WARNING,
                         f"frame rejected (CRC or LEN), {len(error.raw)} bytes")
        else:
            self._report(error.t_ms, "frame_timeout", SEVERITY_WARNING,
                         f"frame cut off after {len(error.raw)} bytes")

    def on_frame(self, f: FrameRecord) -> None:
        tol = self.rules.tolerance_ms
        nodes = self.config.nodes
        if self._last_frame is not None and f.t_start_ms < self._last_frame.t_end_ms - tol:
            self._report(f.t_start_ms, "overlap", SEVERITY_ERROR,
                         f"{f.frame_type} from {self._name(f.src)} starts "
                         f"{self._last_frame.t_end_ms - f.t_start_ms:.1f} ms before the previous "
                         "frame ended", f.src, frames=[self._last_frame.index, f.index])
        self._last_frame = f
        if f.src not in nodes or f.dst not in nodes:
            self._report(f.t_end_ms, "unknown_node", SEVERITY_WARNING,
                         f"{f.frame_type} 0x{f.src:02X} -> 0x{f.dst:02X}: address not on the bus",
                         frames=[f.index])
            return
        if f.retransmission:
            self._report(f.t_end_ms, "retransmission", SEVERITY_INFO,
                         f"{f.frame_type} {self._name(f.src)} -> {self._name(f.dst)} repeated "
                         f"(seq {f.seq})", f.src, frames=[f.index])
        handler = {"POLL": self._poll_frame, "RESP": self._resp_frame,
                   "DATA": self._data_frame, "ACK": self._ack_frame}[f.frame_type]
        handler(f)

    # ---- frame types -------------------------------------------------------------

    def _close_slot(self, t: float) -> None:
        for (a, b), data in list(self._pending_data.items()):
            self._report(t, "missing_ack", SEVERITY_WARNING,
                         f"no ACK from {self._name(b)} to DATA of {self._name(a)} (seq {data.seq})",
                         b, frames=[data.index])
        self._pending_data.clear()

    def _poll_frame(self, f: FrameRecord) -> None:
        if f.src != self.config.master or f.dst == self.config.master:
            self._report(f.t_end_ms, "invalid_sender", SEVERITY_ERROR,
                         f"POLL {self._name(f.src)} -> {self._name(f.dst)}: only the master polls "
                         "slaves", f.src, frames=[f.index])
            return
        if self._poll is not None:
            slave = self._poll.dst
            self._close_slot(f.t_start_ms)
            self._report(f.t_start_ms, "missing_resp", SEVERITY_WARNING,
                         f"no RESP from {self._name(slave)} to the POLL", slave,
                         frames=[self._poll.index])
            self._unanswered[slave] = self._unanswered.get(slave, 0) + 1
            if self._unanswered[slave] == self.rules.unanswered_polls:
                self._report(f.t_start_ms, "slave_unresponsive", SEVERITY_WARNING,
                             f"{self._name(slave)} did not answer "
                             f"{self.rules.unanswered_polls} POLLs in a row", slave)
        # polling rounds: a POLL to a slave earlier in the list starts a new round
        index = self.config.slaves.index(f.dst) if f.dst in self.config.slaves else -1
        if index <= self._last_poll_index or self._round_start is None:
            if self._round_start is not None:
                cycle = f.t_start_ms - self._round_start
                self.cycles.add(f.t_start_ms, cycle)
                limit = self.rules.cycle_max_ms
                if cycle > limit + self.rules.tolerance_ms:
                    self._report(f.t_start_ms, "cycle_too_long", SEVERITY_ERROR,
                                 f"polling cycle took {cycle:.1f} ms", None, cycle, limit,
                                 [f.index])
                low = self.rules.cycle_min_ms
                if low > 0 and cycle < low - self.rules.tolerance_ms:
                    self._report(f.t_start_ms, "cycle_too_short", SEVERITY_WARNING,
                                 f"polling cycle took {cycle:.1f} ms, cycle_period_ms is "
                                 f"{low:.0f}", None, cycle, low, [f.index])
            self._round_start = f.t_start_ms
        self._last_poll_index = index
        self._poll = f
        self._holder = f.dst

    def _resp_frame(self, f: FrameRecord) -> None:
        if f.dst != self.config.master or f.src == self.config.master:
            self._report(f.t_end_ms, "invalid_sender", SEVERITY_ERROR,
                         f"RESP {self._name(f.src)} -> {self._name(f.dst)}: a RESP goes to the "
                         "master", f.src, frames=[f.index])
            return
        if self._poll is None or self._poll.dst != f.src:
            rule = "out_of_slot" if self._holder not in (None, f.src) else "unexpected_resp"
            severity = SEVERITY_ERROR if rule == "out_of_slot" else SEVERITY_WARNING
            self._report(f.t_end_ms, rule, severity,
                         f"RESP from {self._name(f.src)} without a POLL to it", f.src,
                         frames=[f.index])
            return
        slot = f.t_end_ms - self._poll.t_end_ms
        self.slots.setdefault(f.src, Series()).add(f.t_end_ms, slot)
        if slot > self.rules.slot_max_ms + self.rules.tolerance_ms:
            self._report(f.t_end_ms, "slot_overrun", SEVERITY_ERROR,
                         f"slot of {self._name(f.src)} took {slot:.1f} ms", f.src, slot,
                         self.rules.slot_max_ms, [self._poll.index, f.index])
        self._close_slot(f.t_end_ms)
        self._unanswered[f.src] = 0
        self._poll = None
        self._holder = None

    def _data_frame(self, f: FrameRecord) -> None:
        if f.src == self.config.master or f.dst == self.config.master:
            self._report(f.t_end_ms, "invalid_sender", SEVERITY_ERROR,
                         f"DATA {self._name(f.src)} -> {self._name(f.dst)}: DATA is between "
                         "slaves", f.src, frames=[f.index])
            return
        if self._holder != f.src:
            self._report(f.t_end_ms, "out_of_slot", SEVERITY_ERROR,
                         f"DATA from {self._name(f.src)} outside its slot", f.src,
                         frames=[f.index])
        key = (f.src, f.dst)
        previous = self._pending_data.get(key)
        if previous is not None:
            self._report(f.t_start_ms, "missing_ack", SEVERITY_WARNING,
                         f"no ACK from {self._name(f.dst)} to DATA of {self._name(f.src)} "
                         f"(seq {previous.seq})", f.dst, frames=[previous.index])
        self._pending_data[key] = f

    def _ack_frame(self, f: FrameRecord) -> None:
        data = self._pending_data.get((f.dst, f.src))
        if data is None or data.seq != f.seq:
            self._report(f.t_end_ms, "unexpected_ack", SEVERITY_WARNING,
                         f"ACK {self._name(f.src)} -> {self._name(f.dst)} (seq {f.seq}) without "
                         "a matching DATA", f.src, frames=[f.index])
            return
        del self._pending_data[(f.dst, f.src)]
        ack = f.t_end_ms - data.t_end_ms
        self.acks.add(f.t_end_ms, ack)
        if ack > self.rules.ack_max_ms + self.rules.tolerance_ms:
            self._report(f.t_end_ms, "late_ack", SEVERITY_WARNING,
                         f"ACK of {self._name(f.src)} came {ack:.1f} ms after the DATA", f.src,
                         ack, self.rules.ack_max_ms, [data.index, f.index])

    # ---- results -----------------------------------------------------------------

    def summary(self) -> Dict[str, object]:
        return {"slots": {f"0x{s:02X}": series.summary() for s, series in self.slots.items()},
                "ack": self.acks.summary(), "cycle": self.cycles.summary(),
                "limits": {"slot_max_ms": self.rules.slot_max_ms,
                           "ack_max_ms": self.rules.ack_max_ms,
                           "cycle_max_ms": self.rules.cycle_max_ms,
                           "cycle_min_ms": self.rules.cycle_min_ms,
                           "tolerance_ms": self.rules.tolerance_ms,
                           "unanswered_polls": self.rules.unanswered_polls}}
