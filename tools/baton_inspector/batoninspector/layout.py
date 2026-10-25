#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - field layouts of message data: decode bytes
#              into named values and encode values into bytes.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Field layouts.

A layout is the ordered list of fields of one message's data, from the bus
JSON (``messages.<NAME>.fields``)::

    {"name": "speed", "type": "u16", "endian": "le", "scale": 0.1, "offset": 0, "unit": "rpm"}
    {"name": "mode",  "type": "u8",  "enum": {"0": "off", "1": "on"}}
    {"name": "flags", "type": "u8",  "bits": [{"name": "ready", "bit": 0}, {"name": "level", "bit": 4, "width": 3}]}
    {"name": "serial","type": "bytes", "length": 6}
    {"name": "label", "type": "ascii", "length": 8}

Types: u8, i8, u16, i16, u32, i32, f32, bytes, ascii. Byte order "le" (default) or "be".
A decoded value is ``raw * scale + offset`` (scale 1, offset 0 by default). Bytes after
the last field are reported as ``_rest`` (hex); a field that does not fit the data
is reported in ``_error``.
"""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

INT_TYPES = {"u8": ("B", 1), "i8": ("b", 1), "u16": ("H", 2), "i16": ("h", 2),
             "u32": ("I", 4), "i32": ("i", 4), "f32": ("f", 4)}
ALL_TYPES = tuple(INT_TYPES) + ("bytes", "ascii")


class LayoutError(ValueError):
    """Invalid layout definition or value."""


@dataclass
class BitField:
    name: str
    bit: int
    width: int = 1


@dataclass
class Field:
    name: str
    type: str
    endian: str = "le"
    scale: float = 1.0
    offset: float = 0.0
    unit: str = ""
    length: int = 0
    enum: Dict[int, str] = field(default_factory=dict)
    bits: List[BitField] = field(default_factory=list)

    @property
    def size(self) -> int:
        return INT_TYPES[self.type][1] if self.type in INT_TYPES else self.length

    @staticmethod
    def from_json(spec: Dict[str, Any], where: str) -> "Field":
        try:
            name = str(spec["name"])
            ftype = str(spec["type"])
        except KeyError as exc:
            raise LayoutError(f"{where}: field needs 'name' and 'type'") from exc
        if ftype not in ALL_TYPES:
            raise LayoutError(f"{where}.{name}: unknown type '{ftype}' (use {', '.join(ALL_TYPES)})")
        endian = str(spec.get("endian", "le"))
        if endian not in ("le", "be"):
            raise LayoutError(f"{where}.{name}: endian must be 'le' or 'be'")
        length = int(spec.get("length", 0))
        if ftype in ("bytes", "ascii") and length <= 0:
            raise LayoutError(f"{where}.{name}: '{ftype}' needs a positive 'length'")
        enum = {int(k, 0) if isinstance(k, str) else int(k): str(v)
                for k, v in dict(spec.get("enum", {})).items()}
        bits = []
        for b in spec.get("bits", []):
            bf = BitField(str(b["name"]), int(b["bit"]), int(b.get("width", 1)))
            if ftype not in INT_TYPES or ftype == "f32":
                raise LayoutError(f"{where}.{name}: bits need an integer type")
            if bf.bit < 0 or bf.width < 1 or bf.bit + bf.width > 8 * INT_TYPES[ftype][1]:
                raise LayoutError(f"{where}.{name}.{bf.name}: bit range outside the field")
            bits.append(bf)
        scale = float(spec.get("scale", 1.0))
        if scale == 0.0:
            raise LayoutError(f"{where}.{name}: scale must not be 0")
        return Field(name, ftype, endian, scale, float(spec.get("offset", 0.0)),
                     str(spec.get("unit", "")), length, enum, bits)

    def to_json(self) -> Dict[str, Any]:
        out: Dict[str, Any] = {"name": self.name, "type": self.type}
        if self.endian != "le":
            out["endian"] = self.endian
        if self.length:
            out["length"] = self.length
        if self.scale != 1.0:
            out["scale"] = self.scale
        if self.offset != 0.0:
            out["offset"] = self.offset
        if self.unit:
            out["unit"] = self.unit
        if self.enum:
            out["enum"] = {str(k): v for k, v in self.enum.items()}
        if self.bits:
            out["bits"] = [{"name": b.name, "bit": b.bit, "width": b.width} for b in self.bits]
        return out

    # ---- decoding --------------------------------------------------------

    def decode(self, data: bytes) -> Any:
        if self.type == "bytes":
            return data.hex(" ")
        if self.type == "ascii":
            return data.split(b"\x00", 1)[0].decode("ascii", errors="replace")
        code, _ = INT_TYPES[self.type]
        raw = struct.unpack(("<" if self.endian == "le" else ">") + code, data)[0]
        if self.bits:
            return {b.name: (raw >> b.bit) & ((1 << b.width) - 1) for b in self.bits}
        if self.enum and raw in self.enum:
            return self.enum[raw]
        if self.type == "f32" or self.scale != 1.0 or self.offset != 0.0:
            value = raw * self.scale + self.offset
            return round(value, 6) if isinstance(value, float) else value
        return raw

    def text(self, value: Any) -> str:
        if isinstance(value, dict):
            return "{" + ", ".join(f"{k}={v}" for k, v in value.items()) + "}"
        if isinstance(value, float):
            value = f"{value:g}"
        return f"{value} {self.unit}".rstrip() if self.unit else str(value)

    # ---- encoding --------------------------------------------------------

    def encode(self, value: Any) -> bytes:
        if self.type == "bytes":
            data = bytes.fromhex(value.replace(" ", "")) if isinstance(value, str) else bytes(value)
            if len(data) != self.length:
                raise LayoutError(f"{self.name}: needs {self.length} bytes, got {len(data)}")
            return data
        if self.type == "ascii":
            data = str(value).encode("ascii")
            if len(data) > self.length:
                raise LayoutError(f"{self.name}: text longer than {self.length} characters")
            return data.ljust(self.length, b"\x00")
        code, size = INT_TYPES[self.type]
        if self.bits:
            if not isinstance(value, dict):
                raise LayoutError(f"{self.name}: bit fields need a dict of bit values")
            raw = 0
            names = {b.name: b for b in self.bits}
            for key, bit_value in value.items():
                if key not in names:
                    raise LayoutError(f"{self.name}: unknown bit field '{key}'")
                b = names[key]
                raw |= (int(bit_value) & ((1 << b.width) - 1)) << b.bit
        elif isinstance(value, str) and self.enum:
            reverse = {v: k for k, v in self.enum.items()}
            if value not in reverse:
                raise LayoutError(f"{self.name}: '{value}' is not one of {sorted(reverse)}")
            raw = reverse[value]
        else:
            raw = (float(value) - self.offset) / self.scale
            if self.type != "f32":
                raw = int(round(raw))
        if self.type != "f32":
            bits = 8 * size
            low, high = (-(1 << (bits - 1)), (1 << (bits - 1)) - 1) if code.islower() else \
                (0, (1 << bits) - 1)
            if not low <= raw <= high:
                raise LayoutError(f"{self.name}: raw value {raw} out of range {low}..{high}")
        elif math.isnan(raw):
            raw = float("nan")
        return struct.pack(("<" if self.endian == "le" else ">") + code, raw)


@dataclass
class Layout:
    """The fields of one message."""

    name: str
    fields: List[Field] = field(default_factory=list)

    @property
    def size(self) -> int:
        return sum(f.size for f in self.fields)

    def decode(self, data: bytes) -> Dict[str, Any]:
        values: Dict[str, Any] = {}
        pos = 0
        for f in self.fields:
            if pos + f.size > len(data):
                values["_error"] = f"data ends before field '{f.name}' ({len(data)} bytes)"
                break
            values[f.name] = f.decode(data[pos:pos + f.size])
            pos += f.size
        if pos < len(data) and "_error" not in values:
            values["_rest"] = data[pos:].hex(" ")
        return values

    def text(self, values: Dict[str, Any]) -> str:
        names = {f.name: f for f in self.fields}
        parts = []
        for key, value in values.items():
            if key in names:
                parts.append(f"{key}={names[key].text(value)}")
            else:
                parts.append(f"{key}={value}")
        return ", ".join(parts)

    def encode(self, values: Dict[str, Any]) -> bytes:
        unknown = set(values) - {f.name for f in self.fields}
        if unknown:
            raise LayoutError(f"{self.name}: unknown fields {sorted(unknown)}")
        out = b""
        for f in self.fields:
            if f.name not in values:
                raise LayoutError(f"{self.name}: missing field '{f.name}'")
            out += f.encode(values[f.name])
        return out

    @staticmethod
    def from_json(name: str, spec: Dict[str, Any]) -> "Layout":
        fields = [Field.from_json(f, name) for f in spec.get("fields", [])]
        names = [f.name for f in fields]
        if len(names) != len(set(names)):
            raise LayoutError(f"{name}: duplicate field names")
        return Layout(name, fields)

    def to_json(self) -> Dict[str, Any]:
        return {"fields": [f.to_json() for f in self.fields]}


def raw_layout(name: str) -> Layout:
    """Layout of a message without a definition: data shown as hex."""
    return Layout(name, [])


def decode_text(layout: Optional[Layout], data: bytes) -> str:
    if layout is None or not layout.fields:
        return data.hex(" ") if data else "(empty)"
    return layout.text(layout.decode(data))
