#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - transports: a PC serial port and a simulated
#              single-wire bus with byte timing and fault injection.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Transports.

``SimBus`` models one shared wire: every transmitted byte takes 10 bit times,
reaches every powered node (the sender included, as on a single-wire UART), can be
corrupted by noise, and is corrupted when two nodes transmit at the same time.
A node can be unpowered (dead) or answer late (``tx_delay_ms`` delays the start of
each of its transmissions).

``SerialTransport`` wraps pyserial; it accepts port names (``COM3``,
``/dev/ttyUSB0``) and pyserial URLs (``socket://host:port``).
"""

from __future__ import annotations

import random
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


@dataclass
class _Transmission:
    address: int
    data: bytes
    start_ms: float
    sent: int = 0


@dataclass
class SimBus:
    """The simulated wire. Times are session milliseconds."""

    baudrate: int
    noise: float = 0.0
    seed: int = 1
    dead: set = field(default_factory=set)
    tx_delay_ms: Dict[int, float] = field(default_factory=dict)
    collisions: int = 0
    corrupted: int = 0

    def __post_init__(self) -> None:
        self.byte_ms = 10000.0 / self.baudrate
        self._active: List[_Transmission] = []
        self._rng = random.Random(self.seed)

    def transmit(self, address: int, data: bytes, now_ms: float) -> bool:
        """Start a transmission of ``address`` (a dead node transmits nothing)."""
        if address in self.dead:
            return True
        start = now_ms + self.tx_delay_ms.get(address, 0.0)
        self._active.append(_Transmission(address, bytes(data), start))
        return True

    def busy(self, address: int) -> bool:
        return any(t.address == address for t in self._active)

    def step(self, now_ms: float) -> List[Tuple[int, float, int]]:
        """Bytes whose last bit ended by ``now_ms``: (byte, end time, sender), in time order."""
        out: List[Tuple[int, float, int]] = []
        while True:
            due = [(t.start_ms + (t.sent + 1) * self.byte_ms, t) for t in self._active
                   if t.sent < len(t.data)]
            due = [d for d in due if d[0] <= now_ms + 1e-9]
            if not due:
                break
            end, tx = min(due, key=lambda d: d[0])
            byte = tx.data[tx.sent]
            byte_start = end - self.byte_ms
            # another transmitter driving the wire during this byte corrupts it
            for other in self._active:
                if other is tx or other.sent >= len(other.data):
                    continue
                other_start = other.start_ms + other.sent * self.byte_ms
                if other_start < end - 1e-9 and other_start + self.byte_ms > byte_start + 1e-9:
                    byte ^= other.data[other.sent] | 0x01
                    self.collisions += 1
                    break
            if self.noise > 0.0 and self._rng.random() < self.noise:
                byte ^= 1 << self._rng.randrange(8)
                self.corrupted += 1
            tx.sent += 1
            out.append((byte, end, tx.address))
        self._active = [t for t in self._active if t.sent < len(t.data)]
        return out


class SerialTransport:
    """A PC serial port (pyserial), non-blocking reads."""

    def __init__(self, port: str, baudrate: int) -> None:
        try:
            import serial  # noqa: PLC0415 - optional dependency
        except ImportError as exc:  # pragma: no cover - depends on the environment
            raise RuntimeError("pyserial is required for the serial modes: "
                               "python -m pip install pyserial") from exc
        self._serial = serial.serial_for_url(port, baudrate=baudrate, bytesize=8, parity="N",
                                             stopbits=1, timeout=0, write_timeout=1.0)
        self.port = port
        self.baudrate = baudrate
        try:
            self._serial.reset_input_buffer()
        except Exception:  # noqa: BLE001 - URL transports may not support it
            pass

    def read(self) -> bytes:
        waiting = self._serial.in_waiting
        return self._serial.read(waiting) if waiting else b""

    def write(self, data: bytes) -> bool:
        try:
            return self._serial.write(data) == len(data)
        except Exception:  # noqa: BLE001 - reported as a failed transmission
            return False

    def close(self) -> None:
        self._serial.close()


def list_serial_ports() -> List[Tuple[str, str]]:
    """(device, description) of the serial ports of this PC."""
    try:
        from serial.tools import list_ports  # noqa: PLC0415
    except ImportError:  # pragma: no cover
        return []
    return [(p.device, p.description) for p in sorted(list_ports.comports())]


def default_serial_port() -> Optional[str]:
    ports = list_serial_ports()
    return ports[0][0] if ports else None
