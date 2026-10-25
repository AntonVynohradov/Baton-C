#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - records of a session: frames, messages,
#              receive errors and timing violations.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Session records. Times are session milliseconds (float, 0 = session start)."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

FRAME_TYPES = ("POLL", "RESP", "DATA", "ACK")

SEVERITY_ERROR = "error"
SEVERITY_WARNING = "warning"
SEVERITY_INFO = "info"
SEVERITY_ORDER = {SEVERITY_INFO: 0, SEVERITY_WARNING: 1, SEVERITY_ERROR: 2}


@dataclass
class MessageRecord:
    """One TLV message inside a frame."""

    t_ms: float
    frame_index: int
    src: int
    dst: int
    name: str
    msg_id: int
    kind: str                     # "event", "state" or "unknown"
    data: bytes
    values: Dict[str, Any]
    text: str
    retransmission: bool = False  # the frame repeats a frame not yet acknowledged

    def to_json(self) -> Dict[str, Any]:
        return {"name": self.name, "id": self.msg_id, "kind": self.kind, "src": self.src,
                "dst": self.dst, "data": self.data.hex(" "), "values": self.values,
                "text": self.text, "retransmission": self.retransmission}


@dataclass
class FrameRecord:
    """One valid frame seen on the bus."""

    index: int
    t_start_ms: float
    t_end_ms: float
    dst: int
    src: int
    ctrl: int
    payload: bytes
    raw: bytes
    messages: List[MessageRecord] = field(default_factory=list)
    prefix: Dict[str, Any] = field(default_factory=dict)   # ACKB / NACK of POLL and RESP
    retransmission: bool = False
    own: bool = False             # transmitted by the session's own node

    @property
    def frame_type(self) -> str:
        return FRAME_TYPES[self.ctrl >> 6]

    @property
    def first(self) -> bool:
        return bool(self.ctrl & 0x20)

    @property
    def seq(self) -> int:
        return self.ctrl & 0x0F

    @property
    def duration_ms(self) -> float:
        return self.t_end_ms - self.t_start_ms

    def summary(self) -> str:
        parts = [self.frame_type, f"seq={self.seq}"]
        if self.first:
            parts.append("FIRST")
        if "ackb" in self.prefix:
            ackb = self.prefix["ackb"]
            parts.append(f"ACKB={ackb if ackb is not None else '-'}")
        if self.prefix.get("nack"):
            parts.append(f"NACK=0x{self.prefix['nack']:X}")
        if self.messages:
            parts.append(f"{len(self.messages)} msg")
        if self.retransmission:
            parts.append("retry")
        return " ".join(parts)

    def to_json(self) -> Dict[str, Any]:
        return {"index": self.index, "t_start_ms": round(self.t_start_ms, 3),
                "t_end_ms": round(self.t_end_ms, 3), "frame": self.frame_type,
                "src": self.src, "dst": self.dst, "first": self.first, "seq": self.seq,
                "length": len(self.payload), "prefix": self.prefix, "raw": self.raw.hex(" "),
                "retransmission": self.retransmission, "own": self.own,
                "messages": [m.to_json() for m in self.messages]}


@dataclass
class ErrorRecord:
    """Bytes that did not form a valid frame."""

    t_ms: float
    kind: str                     # "crc" (CRC or LEN rejected) or "timeout" (frame cut off)
    raw: bytes

    def to_json(self) -> Dict[str, Any]:
        return {"t_ms": round(self.t_ms, 3), "kind": self.kind, "raw": self.raw.hex(" ")}


@dataclass
class Violation:
    """A timing or bus-usage rule that was not met."""

    t_ms: float
    rule: str
    severity: str
    text: str
    node: Optional[int] = None
    measured_ms: Optional[float] = None
    limit_ms: Optional[float] = None
    frames: List[int] = field(default_factory=list)

    def to_json(self) -> Dict[str, Any]:
        out: Dict[str, Any] = {"t_ms": round(self.t_ms, 3), "rule": self.rule,
                               "severity": self.severity, "text": self.text,
                               "frames": self.frames}
        if self.node is not None:
            out["node"] = self.node
        if self.measured_ms is not None:
            out["measured_ms"] = round(self.measured_ms, 3)
        if self.limit_ms is not None:
            out["limit_ms"] = round(self.limit_ms, 3)
        return out
