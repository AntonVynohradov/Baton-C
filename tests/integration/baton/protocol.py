#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Pure-Python Baton frame codec (twin of the C library codec).
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
#
# Copyright (c) 2026 Anton Vynohradov
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Baton frame codec.

Implements the wire format described in docs/protocol.md:

    SYNC | DST | SRC | CTRL | LEN | PAYLOAD[LEN] | CRC16 (big endian)

* SYNC is 0xA5. LEN counts the payload bytes only (0..MAX_PAYLOAD).
* CTRL = TYPE[7:6] | FIRST[5] | reserved[4] | SEQ[3:0].
* The CRC-16/CCITT-FALSE (polynomial 0x1021, initial value 0xFFFF, not
  reflected, no final XOR) covers DST..PAYLOAD.
* A payload carries messages as TLV records: id, length, data.

The module has no third-party dependencies so it can be tested anywhere.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable, List, Optional, Tuple

SYNC = 0xA5
HEADER_SIZE = 5
CRC_SIZE = 2
FRAME_OVERHEAD = HEADER_SIZE + CRC_SIZE
ADDR_BROADCAST = 0xFF
MAX_PAYLOAD = 64  # BATON_MAX_PAYLOAD of tests/config/baton_user_config.h

# Frame types (CTRL bits 7..6)
FT_POLL = 0
FT_RESP = 1
FT_DATA = 2
FT_ACK = 3

ACKB_VALID = 0x80
SEQ_MASK = 0x0F

FRAME_TYPE_NAMES = {FT_POLL: "POLL", FT_RESP: "RESP", FT_DATA: "DATA", FT_ACK: "ACK"}


def crc16(data: Iterable[int], crc: int = 0xFFFF) -> int:
    """CRC-16/CCITT-FALSE."""
    for byte in data:
        crc ^= (byte & 0xFF) << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def ctrl(frame_type: int, first: bool, seq: int) -> int:
    """Build a control byte."""
    return ((frame_type & 0x03) << 6) | (0x20 if first else 0x00) | (seq & SEQ_MASK)


def ctrl_fields(value: int) -> Tuple[int, bool, int]:
    """(type, first, seq) of a control byte."""
    return value >> 6, bool(value & 0x20), value & SEQ_MASK


@dataclass
class Frame:
    dst: int
    src: int
    ctrl: int
    payload: bytes = b""

    @property
    def frame_type(self) -> int:
        return self.ctrl >> 6

    @property
    def first(self) -> bool:
        return bool(self.ctrl & 0x20)

    @property
    def seq(self) -> int:
        return self.ctrl & SEQ_MASK


def build_frame(dst: int, src: int, control: int, payload: bytes = b"",
                max_payload: int = MAX_PAYLOAD) -> bytes:
    """Encode one frame: SYNC, header, payload and CRC (big endian)."""
    if len(payload) > max_payload:
        raise ValueError(f"payload of {len(payload)} bytes exceeds {max_payload}")
    body = bytes((dst & 0xFF, src & 0xFF, control & 0xFF, len(payload))) + bytes(payload)
    crc = crc16(body)
    return bytes((SYNC,)) + body + bytes((crc >> 8, crc & 0xFF))


def tlv(msg_id: int, data: bytes = b"") -> bytes:
    """One message record."""
    return bytes((msg_id & 0xFF, len(data))) + bytes(data)


def parse_tlv(payload: bytes) -> Tuple[List[Tuple[int, bytes]], bool]:
    """Split a payload into (id, data) records; the flag is False for a truncated record."""
    records: List[Tuple[int, bytes]] = []
    pos = 0
    while pos < len(payload):
        if len(payload) - pos < 2:
            return records, False
        msg_id, size = payload[pos], payload[pos + 1]
        if pos + 2 + size > len(payload):
            return records, False
        records.append((msg_id, bytes(payload[pos + 2:pos + 2 + size])))
        pos += 2 + size
    return records, True


@dataclass
class Parser:
    """Incremental parser with the behavior of baton_parser_feed().

    feed() returns 1 and stores the frame in .frame when a valid frame ends,
    -1 for an invalid LEN or a CRC mismatch, 0 otherwise. Bytes of a rejected
    frame are not re-scanned for SYNC.
    """

    max_payload: int = MAX_PAYLOAD
    frame: Optional[Frame] = None
    _state: int = 0
    _buf: bytearray = field(default_factory=bytearray)

    def reset(self) -> None:
        self._state = 0
        self._buf = bytearray()

    def feed(self, byte: int) -> int:
        result = 0
        if self._state == 0:
            if byte == SYNC:
                self._state = 1
                self._buf = bytearray()
        else:
            self._buf.append(byte)
            if len(self._buf) == 4 and self._buf[3] > self.max_payload:
                self.reset()
                result = -1
            elif len(self._buf) >= 4 and len(self._buf) == 4 + self._buf[3] + CRC_SIZE:
                body = bytes(self._buf[:-CRC_SIZE])
                received = (self._buf[-2] << 8) | self._buf[-1]
                self.reset()
                if crc16(body) == received:
                    self.frame = Frame(body[0], body[1], body[2], body[4:])
                    result = 1
                else:
                    result = -1
        return result

    def feed_all(self, data: Iterable[int]) -> List[Frame]:
        """Feed a stream; return every valid frame found."""
        frames: List[Frame] = []
        for byte in data:
            if self.feed(byte) == 1 and self.frame is not None:
                frames.append(self.frame)
        return frames


def describe(frame: Frame) -> str:
    """One-line description, e.g. 'POLL 01->11 first seq=0 [00]'."""
    name = FRAME_TYPE_NAMES[frame.frame_type]
    flags = " first" if frame.first else ""
    return f"{name} {frame.src:02X}->{frame.dst:02X}{flags} seq={frame.seq} [{frame.payload.hex(' ')}]"
