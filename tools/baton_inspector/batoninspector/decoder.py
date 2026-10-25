#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - turns the timestamped byte stream of the bus
#              into frame, message and error records.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Stream decoder.

Bytes are framed by the library's parser (``baton_parser_feed``); this module
only adds time stamps, the payload prefixes of POLL and RESP, the TLV split and
the field decoding of the bus definition.

Time stamps: every byte arrives with the time its last bit ended (exact in the
simulation; in serial mode the time the PC read the chunk containing it). A frame
ends at the time of its last byte and starts one wire-time of the frame earlier.
"""

from __future__ import annotations

from typing import Callable, Dict, List, Optional, Union

from .busconfig import BusConfig, frame_ms
from .layout import Layout, decode_text
from .model import ErrorRecord, FrameRecord, MessageRecord
from .native import Bridge, Parser

Record = Union[FrameRecord, ErrorRecord]


class StreamDecoder:
    """Bytes in, records out (through ``on_record``)."""

    def __init__(self, config: BusConfig, bridge: Bridge, frame_gap_ms: float,
                 on_record: Callable[[Record], None]) -> None:
        self.config = config
        self.bridge = bridge
        self.parser = Parser(bridge)
        self.frame_gap_ms = frame_gap_ms
        self.on_record = on_record
        self.baudrate = int(config.bus["baudrate"])
        self.mask_bytes = (bridge.config.max_nodes + 7) // 8
        self._raw = bytearray()
        self._last_ms: Optional[float] = None
        self._index = 0
        self._own_pending = 0      # bytes of the next frame that the own node transmitted
        self._last_content: Dict[tuple, bytes] = {}

    def mark_own(self, nbytes: int) -> None:
        """The next ``nbytes`` bytes are the own node's transmission."""
        self._own_pending += nbytes

    def feed(self, data: bytes, times_ms: List[float]) -> None:
        for byte, t in zip(data, times_ms):
            if (not self.parser.idle) and self._last_ms is not None \
                    and t - self._last_ms > self.frame_gap_ms:
                self.on_record(ErrorRecord(self._last_ms, "timeout", bytes(self._raw)))
                self.parser.reset()
                self._raw.clear()
            self._last_ms = t
            was_idle = self.parser.idle
            result = self.parser.feed(byte)
            if was_idle and not self.parser.idle:
                self._raw = bytearray([byte])
            elif not was_idle:
                self._raw.append(byte)
            own = self._own_pending > 0
            if own:
                self._own_pending -= 1
            if result == 1:
                self._emit_frame(t, own)
                self._raw.clear()
            elif result < 0:
                self.on_record(ErrorRecord(t, "crc", bytes(self._raw)))
                self._raw.clear()

    # ---- frames ----------------------------------------------------------

    def _emit_frame(self, t_end: float, own: bool) -> None:
        dst, src, ctrl, payload = self.parser.frame()
        raw = bytes(self._raw)
        frame = FrameRecord(self._index, t_end - frame_ms(self.baudrate, len(raw)), t_end, dst,
                            src, ctrl, payload, raw, own=own)
        self._index += 1
        ftype = frame.frame_type
        content = payload
        if ftype == "POLL" and payload:
            frame.prefix = {"ackb": (payload[0] & 0x0F) if payload[0] & 0x80 else None}
            content = payload[1:]
        elif ftype == "RESP" and len(payload) >= 1 + self.mask_bytes:
            nack = int.from_bytes(payload[1:1 + self.mask_bytes], "little")
            frame.prefix = {"ackb": (payload[0] & 0x0F) if payload[0] & 0x80 else None,
                            "nack": nack}
            content = payload[1 + self.mask_bytes:]
        # a frame with content that repeats the last content frame of the channel
        key = (src, dst, ftype == "ACK")
        if content and ftype != "ACK":
            previous = self._last_content.get(key)
            if previous is not None and previous == bytes([ctrl & 0x0F]) + content:
                frame.retransmission = True
            self._last_content[key] = bytes([ctrl & 0x0F]) + content
        frame.messages = self._messages(frame, content)
        self.on_record(frame)

    def _messages(self, frame: FrameRecord, content: bytes) -> List[MessageRecord]:
        out = []
        pos = 0
        while pos + 2 <= len(content):
            msg_id, length = content[pos], content[pos + 1]
            data = content[pos + 2:pos + 2 + length]
            if len(data) < length:
                break
            info = self.bridge.by_id.get(msg_id)
            name = info.name if info else f"0x{msg_id:02X}"
            kind = ("state" if info.is_state else "event") if info else "unknown"
            layout: Optional[Layout] = self.config.layouts.get(name)
            values = layout.decode(data) if layout and layout.fields else {}
            out.append(MessageRecord(frame.t_end_ms, frame.index, frame.src, frame.dst, name,
                                     msg_id, kind, data, values, decode_text(layout, data),
                                     frame.retransmission))
            pos += 2 + length
        return out
