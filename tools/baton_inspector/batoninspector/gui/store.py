#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector GUI - the record store shared by all views:
#              fed by a local session or by the live feed of a test session.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Record store.

Records are the JSON dictionaries of the session log (``frame``, ``error``,
``violation``, ``mark``), whatever their source. They are queued from the session
thread and moved into the store by a Qt timer, so the views only ever run in the
GUI thread.
"""

from __future__ import annotations

import threading
from collections import deque
from typing import Any, Callable, Deque, Dict, List, Optional

from PySide6.QtCore import QObject, QTimer, Signal

MAX_FRAMES = 100_000


class BusInfo:
    """What the views need to know about the bus."""

    def __init__(self, master: int, slaves: List[int], names: Dict[int, str],
                 limits: Dict[str, Any], messages: List[Dict[str, Any]], title: str) -> None:
        self.master = master
        self.slaves = slaves
        self.names = names
        self.limits = limits
        self.messages = messages
        self.title = title

    @property
    def nodes(self) -> List[int]:
        return [self.master] + self.slaves

    def name(self, address: int) -> str:
        return self.names.get(address, f"0x{address:02X}")

    def label(self, address: int) -> str:
        name = self.names.get(address)
        return f"{name} (0x{address:02X})" if name else f"0x{address:02X}"


class RecordStore(QObject):
    """Frames, errors, violations and marks, in arrival order."""

    reset = Signal()
    added = Signal(int, int)        # first new frame index, number of new frames
    other_added = Signal()          # violations, errors or marks changed
    timing_updated = Signal(dict)
    stats_updated = Signal(dict)

    def __init__(self) -> None:
        super().__init__()
        self.info: Optional[BusInfo] = None
        self.frames: List[Dict[str, Any]] = []
        self.errors: List[Dict[str, Any]] = []
        self.violations: List[Dict[str, Any]] = []
        self.marks: List[Dict[str, Any]] = []
        self.violated_frames: Dict[int, str] = {}
        self.now_ms = 0.0
        self._queue: Deque[Dict[str, Any]] = deque()
        self._lock = threading.Lock()
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._drain)
        self._timer.start(50)

    def clear(self, info: Optional[BusInfo] = None) -> None:
        with self._lock:
            self._queue.clear()
        if info is not None:
            self.info = info
        self.frames.clear()
        self.errors.clear()
        self.violations.clear()
        self.marks.clear()
        self.violated_frames.clear()
        self.now_ms = 0.0
        self.reset.emit()

    def push(self, record: Dict[str, Any]) -> None:
        """Thread-safe: queue a record (any thread)."""
        with self._lock:
            self._queue.append(record)

    def _drain(self) -> None:
        with self._lock:
            records = list(self._queue)
            self._queue.clear()
        if not records:
            return
        first = len(self.frames)
        other = False
        for record in records:
            kind = record.get("type")
            if kind == "frame":
                self.frames.append(record)
                self.now_ms = max(self.now_ms, record["t_end_ms"])
            elif kind == "violation":
                self.violations.append(record)
                for index in record.get("frames", []):
                    previous = self.violated_frames.get(index)
                    if previous != "error":
                        self.violated_frames[index] = record["severity"]
                other = True
            elif kind == "error":
                self.errors.append(record)
                other = True
            elif kind == "mark":
                self.marks.append(record)
                other = True
            elif kind == "time":
                self.now_ms = max(self.now_ms, record["t_ms"])
            elif kind == "timing":
                self.timing_updated.emit(record)
            elif kind == "stats":
                self.stats_updated.emit(record.get("nodes", {}))
        if len(self.frames) > MAX_FRAMES:
            drop = len(self.frames) - MAX_FRAMES
            del self.frames[:drop]
            self.reset.emit()
            return
        if len(self.frames) > first:
            self.added.emit(first, len(self.frames) - first)
        if other:
            self.other_added.emit()

    def frame_by_index(self, index: int) -> Optional[Dict[str, Any]]:
        if not self.frames:
            return None
        offset = index - self.frames[0]["index"]
        if 0 <= offset < len(self.frames) and self.frames[offset]["index"] == index:
            return self.frames[offset]
        for frame in self.frames:
            if frame["index"] == index:
                return frame
        return None


def session_listener(store: RecordStore) -> Callable[[str, Any], None]:
    """A Session listener that feeds the store."""

    def listener(kind: str, record: Any) -> None:
        if kind == "message":
            return
        body = record.to_json() if hasattr(record, "to_json") else dict(record)
        body["type"] = kind
        store.push(body)

    return listener
