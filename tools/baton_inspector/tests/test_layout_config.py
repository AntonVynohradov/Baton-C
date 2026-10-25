#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Description: Baton Inspector tests - field layouts and the bus definition.
#
# Author:      Anton Vynohradov
# License:     Apache License 2.0
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

import copy
import json
import os

import pytest

from batoninspector import BusConfig, ConfigError, Layout, LayoutError

HERE = os.path.dirname(os.path.abspath(__file__))
EXAMPLE = os.path.join(HERE, "..", "examples", "example_bus.json")


def layout(*fields):
    return Layout.from_json("MSG", {"fields": list(fields)})


def test_integer_types_and_byte_order():
    lay = layout({"name": "a", "type": "u8"}, {"name": "b", "type": "i16"},
                 {"name": "c", "type": "u32", "endian": "be"}, {"name": "d", "type": "i8"})
    data = lay.encode({"a": 255, "b": -2, "c": 0x01020304, "d": -128})
    assert data == bytes([0xFF, 0xFE, 0xFF, 1, 2, 3, 4, 0x80])
    assert lay.decode(data) == {"a": 255, "b": -2, "c": 0x01020304, "d": -128}


def test_scale_offset_unit_and_float():
    lay = layout({"name": "temp", "type": "i16", "scale": 0.01, "offset": -40, "unit": "degC"},
                 {"name": "f", "type": "f32"})
    data = lay.encode({"temp": 21.5, "f": 1.5})
    values = lay.decode(data)
    assert values == {"temp": 21.5, "f": 1.5}
    assert lay.text(values) == "temp=21.5 degC, f=1.5"


def test_enum_bits_bytes_ascii():
    lay = layout({"name": "mode", "type": "u8", "enum": {"0": "off", "1": "on"}},
                 {"name": "flags", "type": "u8",
                  "bits": [{"name": "ready", "bit": 0}, {"name": "level", "bit": 4, "width": 3}]},
                 {"name": "sn", "type": "bytes", "length": 3},
                 {"name": "label", "type": "ascii", "length": 4})
    data = lay.encode({"mode": "on", "flags": {"ready": 1, "level": 5}, "sn": "0a 0b 0c",
                       "label": "AB"})
    assert data == bytes([1, 0x51, 0x0A, 0x0B, 0x0C]) + b"AB\x00\x00"
    assert lay.decode(data) == {"mode": "on", "flags": {"ready": 1, "level": 5},
                                "sn": "0a 0b 0c", "label": "AB"}
    assert lay.decode(bytes([7, 0, 0, 0, 0]) + b"xyzw")["mode"] == 7  # not in the enum


def test_short_and_long_data():
    lay = layout({"name": "a", "type": "u16"})
    assert lay.decode(b"\x01")["_error"].startswith("data ends")
    assert lay.decode(b"\x01\x00\xAA")["_rest"] == "aa"


@pytest.mark.parametrize("spec, value, message", [
    ({"name": "a", "type": "u8"}, 256, "out of range"),
    ({"name": "a", "type": "i8"}, -129, "out of range"),
    ({"name": "a", "type": "u8", "enum": {"0": "off"}}, "maybe", "is not one of"),
    ({"name": "a", "type": "ascii", "length": 2}, "abc", "longer"),
    ({"name": "a", "type": "bytes", "length": 2}, "01", "needs 2 bytes"),
])
def test_encode_errors(spec, value, message):
    with pytest.raises(LayoutError, match=message):
        layout(spec).encode({"a": value})


@pytest.mark.parametrize("spec, message", [
    ({"name": "a", "type": "u64"}, "unknown type"),
    ({"name": "a", "type": "bytes"}, "positive 'length'"),
    ({"name": "a", "type": "u8", "endian": "middle"}, "endian"),
    ({"name": "a", "type": "u8", "bits": [{"name": "x", "bit": 7, "width": 2}]}, "outside"),
    ({"name": "a", "type": "u8", "scale": 0}, "scale"),
    ({"type": "u8"}, "needs 'name'"),
])
def test_layout_definition_errors(spec, message):
    with pytest.raises(LayoutError, match=message):
        layout(spec)


def test_missing_and_unknown_fields():
    lay = layout({"name": "a", "type": "u8"})
    with pytest.raises(LayoutError, match="missing"):
        lay.encode({})
    with pytest.raises(LayoutError, match="unknown"):
        lay.encode({"a": 1, "b": 2})


def example():
    with open(EXAMPLE, encoding="utf-8") as f:
        return json.load(f)


def test_example_loads_and_derives_limits():
    config = BusConfig.load(EXAMPLE)
    assert config.master == 0x01 and config.slaves == [0x11, 0x12]
    assert config.node_name(0x11) == "Output board"
    assert os.path.isfile(config.user_config)
    rules = config.rules(64)
    assert rules.slot_max_ms == 250 and rules.ack_max_ms == 20
    # 2 x (74 ms maximum frame + 2 + 250 + 2 + 10)
    assert rules.cycle_max_ms == 2 * (71 * 10000 / 9600 + 264) // 1 + 1
    assert all(rules.derived.values())


def test_rule_override():
    data = example()
    data["timing"]["rules"]["slot_max_ms"] = 100
    rules = BusConfig.from_dict(data, os.path.dirname(EXAMPLE)).rules(64)
    assert rules.slot_max_ms == 100 and rules.derived["slot_max_ms"] is False


@pytest.mark.parametrize("change, message", [
    (lambda d: d.pop("native"), "user_config"),
    (lambda d: d["bus"].update(slaves=[]), "at least one slave"),
    (lambda d: d["bus"].update(slaves=["0x11", "0x11"]), "unique"),
    (lambda d: d["bus"].update(master="0xFF"), "outside"),
    (lambda d: d["timing"]["rules"].update(foo=1), "unknown rules"),
    (lambda d: d["messages"].update(X={"fields": [{"name": "a", "type": "u9"}]}), "unknown type"),
    (lambda d: d["simulation"]["slaves"].update({"0x33": {}}), "not a slave"),
    (lambda d: d["simulation"].update(noise=2), "noise"),
])
def test_config_errors(change, message):
    data = copy.deepcopy(example())
    change(data)
    with pytest.raises(ConfigError, match=message):
        BusConfig.from_dict(data, os.path.dirname(EXAMPLE))
