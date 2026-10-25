#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - session recording in JSON Lines and loading
#              of recorded sessions for analysis.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""JSON Lines session log.

One JSON object per line, ``type`` first:

* ``session`` — once at the start: format version, wall-clock start time
  (ISO 8601, UTC), mode, bus name, bus parameters, effective timing limits,
  library version, the message table;
* ``frame`` — every valid frame: ``t_ms``/``t_end_ms`` (session time),
  ``wall`` (ISO time of the frame end), frame type, addresses, sequence, prefix
  (ACKB, NACK), raw bytes, the decoded messages with field values;
* ``error`` — rejected bytes (CRC/LEN or a frame cut off);
* ``violation`` — a timing or bus-usage rule that was not met;
* ``mark`` — a text marker from the test (``Session.mark()``);
* ``stats`` — node counters, written when the recording stops;
* ``end`` — once at the end, with the timing summary.

``load_session()`` reads a file back into plain dictionaries.
"""

from __future__ import annotations

import datetime as _dt
import json
import threading
from typing import Any, Dict, Iterator, List, Optional, TextIO

FORMAT_VERSION = 1


def iso(timestamp: float) -> str:
    return _dt.datetime.fromtimestamp(timestamp, _dt.timezone.utc).isoformat(timespec="milliseconds")


class Recorder:
    """Writes records of a session; thread-safe."""

    def __init__(self, path: str, wall_start: float) -> None:
        self.path = path
        self._wall_start = wall_start
        self._file: Optional[TextIO] = open(path, "w", encoding="utf-8", newline="\n")
        self._lock = threading.Lock()
        self.lines = 0

    def write(self, kind: str, body: Dict[str, Any], t_ms: Optional[float] = None) -> None:
        record: Dict[str, Any] = {"type": kind}
        if t_ms is not None:
            record["wall"] = iso(self._wall_start + t_ms / 1000.0)
        record.update(body)
        line = json.dumps(record, ensure_ascii=False, default=str)
        with self._lock:
            if self._file is not None:
                self._file.write(line + "\n")
                self.lines += 1

    def flush(self) -> None:
        with self._lock:
            if self._file is not None:
                self._file.flush()

    def close(self) -> None:
        with self._lock:
            if self._file is not None:
                self._file.close()
                self._file = None


def iter_session(path: str) -> Iterator[Dict[str, Any]]:
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                yield json.loads(line)


def load_session(path: str) -> Dict[str, List[Dict[str, Any]]]:
    """Records of a log grouped by type: {'session': [...], 'frame': [...], ...}."""
    out: Dict[str, List[Dict[str, Any]]] = {}
    for record in iter_session(path):
        out.setdefault(record["type"], []).append(record)
    return out


def messages_of(path: str) -> List[Dict[str, Any]]:
    """Every decoded message of a log, flattened, with the time of its frame."""
    out = []
    for record in iter_session(path):
        if record["type"] == "frame":
            for message in record["messages"]:
                item = dict(message)
                item["t_ms"] = record["t_end_ms"]
                item["wall"] = record.get("wall")
                item["frame"] = record["frame"]
                out.append(item)
    return out
