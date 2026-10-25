#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - the bus definition file (JSON): topology,
#              timing, timing rules, message layouts, simulation.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""The bus definition file.

Example (see ``examples/example_bus.json``)::

    {
      "name": "Example bus",
      "native": {"user_config": "../../../examples/config/baton_user_config.h"},
      "bus": {
        "baudrate": 9600, "master": "0x01", "slaves": ["0x11", "0x12"],
        "node_names": {"0x01": "Master", "0x11": "Output board"},
        "slot_time_ms": 250, "ack_timeout_ms": 20, "max_retries": 1, "tx_gap_ms": 2,
        "inter_byte_timeout_ms": 5, "cycle_period_ms": 0, "event_ttl_ms": 5000,
        "rx_echo": true
      },
      "timing": {"tolerance_ms": 3, "rules": {"slot_max_ms": null, "ack_max_ms": null,
                 "cycle_max_ms": null, "cycle_min_ms": null, "unanswered_polls": 3}},
      "messages": {"CMD_SET_OUTPUT": {"fields": [...]}},
      "simulation": {...}
    }

Relative paths are relative to the JSON file. Every timing rule left at ``null`` is
derived from the bus parameters with the formulas of docs/protocol.md; a number
overrides it.
"""

from __future__ import annotations

import copy
import json
import math
import os
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

from .layout import Layout, LayoutError

DEFAULT_BUS = {"baudrate": 9600, "slot_time_ms": 250, "ack_timeout_ms": 20, "max_retries": 1,
               "tx_gap_ms": 2, "inter_byte_timeout_ms": 5, "cycle_period_ms": 0,
               "event_ttl_ms": 5000, "rx_echo": True}
DEFAULT_RULES = {"slot_max_ms": None, "ack_max_ms": None, "cycle_max_ms": None,
                 "cycle_min_ms": None, "unanswered_polls": 3}
TIMING_KEYS = ("baudrate", "slot_time_ms", "ack_timeout_ms", "max_retries", "tx_gap_ms",
               "inter_byte_timeout_ms", "cycle_period_ms", "event_ttl_ms")


class ConfigError(ValueError):
    """Invalid bus definition."""


def parse_address(value: Any, where: str) -> int:
    try:
        address = int(value, 0) if isinstance(value, str) else int(value)
    except (TypeError, ValueError) as exc:
        raise ConfigError(f"{where}: '{value}' is not an address") from exc
    if not 0 <= address <= 0xFE:
        raise ConfigError(f"{where}: address 0x{address:X} outside 0x00..0xFE")
    return address


def frame_ms(baudrate: int, nbytes: int) -> float:
    """Wire time of nbytes at 8N1 in milliseconds (exact, not rounded)."""
    return nbytes * 10000.0 / baudrate


@dataclass
class TimingRules:
    """Limits used by the timing analyzer, in milliseconds."""

    tolerance_ms: float
    slot_max_ms: float
    ack_max_ms: float
    cycle_max_ms: float
    cycle_min_ms: float
    unanswered_polls: int
    derived: Dict[str, bool] = field(default_factory=dict)


@dataclass
class SimSlave:
    address: int
    auto_reply: List[Dict[str, Any]] = field(default_factory=list)
    publish: List[Dict[str, Any]] = field(default_factory=list)


@dataclass
class SimConfig:
    noise: float = 0.0
    dead: List[int] = field(default_factory=list)
    tx_delay_ms: Dict[int, float] = field(default_factory=dict)
    slaves: Dict[int, SimSlave] = field(default_factory=dict)
    speed: float = 1.0


@dataclass
class BusConfig:
    """A loaded and validated bus definition."""

    path: Optional[str]
    name: str
    user_config: str
    master: int
    slaves: List[int]
    node_names: Dict[int, str]
    bus: Dict[str, Any]
    tolerance_ms: float
    rules_override: Dict[str, Any]
    layouts: Dict[str, Layout]
    simulation: SimConfig
    raw: Dict[str, Any]

    # ---- loading -----------------------------------------------------------

    @staticmethod
    def load(path: str) -> "BusConfig":
        with open(path, encoding="utf-8") as f:
            try:
                data = json.load(f)
            except json.JSONDecodeError as exc:
                raise ConfigError(f"{path}: invalid JSON: {exc}") from exc
        return BusConfig.from_dict(data, os.path.dirname(os.path.abspath(path)), path)

    @staticmethod
    def from_dict(data: Dict[str, Any], base_dir: str = ".", path: Optional[str] = None) -> "BusConfig":
        raw = copy.deepcopy(data)
        native = data.get("native", {})
        if "user_config" not in native:
            raise ConfigError("native.user_config: path of the system's baton_user_config.h is required")
        user_config = native["user_config"]
        if not os.path.isabs(user_config):
            user_config = os.path.normpath(os.path.join(base_dir, user_config))

        bus_in = data.get("bus")
        if not isinstance(bus_in, dict):
            raise ConfigError("'bus' section is required")
        bus = dict(DEFAULT_BUS)
        bus.update({k: v for k, v in bus_in.items() if k not in ("master", "slaves", "node_names")})
        for key in TIMING_KEYS:
            try:
                bus[key] = int(bus[key])
            except (TypeError, ValueError) as exc:
                raise ConfigError(f"bus.{key}: integer expected") from exc
        bus["rx_echo"] = bool(bus["rx_echo"])
        if bus["baudrate"] <= 0:
            raise ConfigError("bus.baudrate must be positive")
        master = parse_address(bus_in.get("master"), "bus.master")
        slaves = [parse_address(s, "bus.slaves") for s in bus_in.get("slaves", [])]
        if not slaves:
            raise ConfigError("bus.slaves: at least one slave is required")
        if len(set(slaves + [master])) != len(slaves) + 1:
            raise ConfigError("bus: addresses must be unique")
        names = {parse_address(k, "bus.node_names"): str(v)
                 for k, v in dict(bus_in.get("node_names", {})).items()}

        timing = data.get("timing", {})
        rules = dict(DEFAULT_RULES)
        rules.update(timing.get("rules", {}))
        unknown = set(rules) - set(DEFAULT_RULES)
        if unknown:
            raise ConfigError(f"timing.rules: unknown rules {sorted(unknown)}")

        layouts = {}
        for name, spec in dict(data.get("messages", {})).items():
            try:
                layouts[name] = Layout.from_json(name, spec)
            except LayoutError as exc:
                raise ConfigError(f"messages: {exc}") from exc

        sim_in = data.get("simulation", {})
        sim = SimConfig(noise=float(sim_in.get("noise", 0.0)),
                        dead=[parse_address(a, "simulation.dead") for a in sim_in.get("dead", [])],
                        tx_delay_ms={parse_address(k, "simulation.tx_delay_ms"): float(v)
                                     for k, v in dict(sim_in.get("tx_delay_ms", {})).items()},
                        speed=float(sim_in.get("speed", 1.0)))
        for key, spec in dict(sim_in.get("slaves", {})).items():
            address = parse_address(key, "simulation.slaves")
            if address not in slaves:
                raise ConfigError(f"simulation.slaves: 0x{address:02X} is not a slave of the bus")
            sim.slaves[address] = SimSlave(address, list(spec.get("auto_reply", [])),
                                           list(spec.get("publish", [])))
        if not 0.0 <= sim.noise < 1.0:
            raise ConfigError("simulation.noise must be in 0..1")

        return BusConfig(path, str(data.get("name", "Baton bus")), user_config, master, slaves,
                         names, bus, float(timing.get("tolerance_ms", 3.0)), rules, layouts, sim, raw)

    def save(self, path: str) -> None:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            json.dump(self.raw, f, indent=2)
            f.write("\n")

    # ---- helpers -----------------------------------------------------------

    @property
    def nodes(self) -> List[int]:
        """Node addresses in node-index order (master first)."""
        return [self.master] + self.slaves

    def node_name(self, address: int) -> str:
        return self.node_names.get(address, f"0x{address:02X}")

    def node_index(self, address: int) -> Optional[int]:
        return self.nodes.index(address) if address in self.nodes else None

    def timing_params(self) -> Dict[str, int]:
        return {k: int(self.bus[k]) for k in TIMING_KEYS}

    def rules(self, max_payload: int) -> TimingRules:
        """Effective limits: overrides from the JSON, the rest derived (docs/protocol.md)."""
        b = self.bus
        frame_max = frame_ms(b["baudrate"], 7 + max_payload)
        derived_values = {
            # RESP must end within slot_time_ms after the POLL
            "slot_max_ms": float(b["slot_time_ms"]),
            "ack_max_ms": float(b["ack_timeout_ms"]),
            # longest slot including the POLL: POLL frame + gap + slot + master margin
            "cycle_max_ms": len(self.slaves) * (frame_max + b["tx_gap_ms"] + b["slot_time_ms"]
                                                + b["tx_gap_ms"] + 10.0),
            "cycle_min_ms": float(b["cycle_period_ms"]),
        }
        values: Dict[str, float] = {}
        derived: Dict[str, bool] = {}
        for key, default in derived_values.items():
            override = self.rules_override.get(key)
            derived[key] = override is None
            values[key] = float(default if override is None else override)
        return TimingRules(self.tolerance_ms, values["slot_max_ms"], values["ack_max_ms"],
                           math.ceil(values["cycle_max_ms"]), values["cycle_min_ms"],
                           int(self.rules_override.get("unanswered_polls") or 3), derived)

    def check_against(self, messages: Dict[str, Any], max_nodes: int) -> List[str]:
        """Problems of this definition against the message table of the native bridge."""
        problems = []
        if len(self.slaves) + 1 > max_nodes:
            problems.append(f"{len(self.slaves) + 1} nodes, but BATON_MAX_NODES is {max_nodes}")
        for name, layout in self.layouts.items():
            info = messages.get(name)
            if info is None:
                problems.append(f"messages.{name}: not in baton_user_config.h")
                continue
            spec_id = self.raw.get("messages", {}).get(name, {}).get("id")
            if spec_id is not None and int(str(spec_id), 0) != info.msg_id:
                problems.append(f"messages.{name}: id {spec_id} differs from 0x{info.msg_id:02X} "
                                "in baton_user_config.h")
            if layout.size > info.max_length:
                problems.append(f"messages.{name}: fields take {layout.size} bytes, MAX_LEN is "
                                f"{info.max_length}")
        for address, slave in self.simulation.slaves.items():
            for rule in slave.auto_reply:
                for key in ("on", "send"):
                    if rule.get(key) not in messages:
                        problems.append(f"simulation.slaves.0x{address:02X}.auto_reply: "
                                        f"unknown message '{rule.get(key)}'")
            for rule in slave.publish:
                if rule.get("state") not in messages:
                    problems.append(f"simulation.slaves.0x{address:02X}.publish: unknown state "
                                    f"'{rule.get('state')}'")
        return problems
