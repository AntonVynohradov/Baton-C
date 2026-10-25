#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Description: Baton Inspector tests - timing analyzer on synthetic frames.
#
# Author:      Anton Vynohradov
# License:     Apache License 2.0
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

import os

from batoninspector import BusConfig
from batoninspector.model import ErrorRecord, FrameRecord
from batoninspector.timing import TimingAnalyzer

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG = BusConfig.load(os.path.join(HERE, "..", "examples", "example_bus.json"))
TYPES = {"POLL": 0, "RESP": 1, "DATA": 2, "ACK": 3}


class Bus:
    def __init__(self):
        self.violations = []
        self.analyzer = TimingAnalyzer(CONFIG, CONFIG.rules(64), self.violations.append)
        self.index = 0

    def frame(self, kind, src, dst, start, end, seq=0, retransmission=False):
        f = FrameRecord(self.index, start, end, dst, src, (TYPES[kind] << 6) | seq, b"", b"",
                        retransmission=retransmission)
        self.index += 1
        self.analyzer.on_frame(f)
        return f

    def slot(self, slave, t, length=20.0):
        self.frame("POLL", 0x01, slave, t, t + 8)
        self.frame("RESP", slave, 0x01, t + length - 9, t + length)

    def rules(self):
        return [v.rule for v in self.violations]


def test_clean_cycles_have_no_violations():
    bus = Bus()
    for cycle in range(3):
        bus.slot(0x11, cycle * 100.0)
        bus.slot(0x12, cycle * 100.0 + 50)
    assert bus.rules() == []
    summary = bus.analyzer.summary()
    assert summary["slots"]["0x11"]["count"] == 3
    assert summary["slots"]["0x11"]["max_ms"] == 12.0   # POLL end 8 -> RESP end 20
    assert summary["cycle"] == {"count": 2, "min_ms": 100.0, "avg_ms": 100.0, "max_ms": 100.0}


def test_slot_overrun_with_tolerance():
    bus = Bus()
    bus.slot(0x11, 0, length=8 + 253)        # 253 ms: within 250 + 3
    bus.slot(0x12, 300, length=8 + 254)      # 254 ms: overrun
    assert bus.rules() == ["slot_overrun"]
    assert bus.violations[0].measured_ms == 254 and bus.violations[0].node == 0x12


def test_missing_resp_and_unresponsive_slave():
    bus = Bus()
    for i in range(3):
        bus.frame("POLL", 0x01, 0x12, i * 300.0, i * 300.0 + 8)
    bus.frame("POLL", 0x01, 0x11, 900, 908)
    assert bus.rules() == ["missing_resp", "missing_resp", "missing_resp", "slave_unresponsive"]


def test_data_ack_rules():
    bus = Bus()
    bus.frame("POLL", 0x01, 0x11, 0, 8)
    bus.frame("DATA", 0x11, 0x12, 12, 20, seq=1)
    bus.frame("ACK", 0x12, 0x11, 30, 37, seq=1)            # 17 ms: ok
    bus.frame("DATA", 0x11, 0x12, 40, 48, seq=2)
    bus.frame("ACK", 0x12, 0x11, 65, 72, seq=2)            # 24 ms > 20 + 3: late
    bus.frame("DATA", 0x11, 0x12, 80, 88, seq=3)
    bus.frame("DATA", 0x11, 0x12, 120, 128, seq=3, retransmission=True)
    bus.frame("ACK", 0x12, 0x11, 140, 147, seq=9)          # no such DATA
    bus.frame("RESP", 0x11, 0x01, 150, 160)                # the DATA seq 3 stays unacknowledged
    assert bus.rules() == ["late_ack", "retransmission", "missing_ack", "unexpected_ack",
                           "missing_ack"]


def test_bus_usage_anomalies():
    bus = Bus()
    bus.frame("POLL", 0x01, 0x11, 0, 8)
    bus.frame("DATA", 0x12, 0x11, 10, 18)                  # 0x12 does not hold the baton
    bus.frame("RESP", 0x12, 0x01, 19, 25)                  # nor may it answer
    bus.frame("POLL", 0x11, 0x12, 30, 38)                  # only the master polls
    bus.frame("RESP", 0x11, 0x12, 40, 48)                  # RESP goes to the master
    bus.frame("DATA", 0x11, 0x01, 50, 58)                  # DATA is between slaves
    bus.frame("POLL", 0x01, 0x77, 60, 68)                  # unknown address
    bus.frame("RESP", 0x11, 0x01, 66, 75)                  # 2 ms early: within the tolerance
    bus.frame("RESP", 0x12, 0x01, 70, 80)                  # starts 5 ms before the end: overlap
    # the RESP of 0x11 closes its slot: the DATA of 0x12 was never acknowledged
    assert bus.rules() == ["out_of_slot", "out_of_slot", "invalid_sender", "invalid_sender",
                           "invalid_sender", "unknown_node", "missing_ack", "overlap",
                           "unexpected_resp"]


def test_cycle_limits():
    data = dict(CONFIG.raw)
    data["timing"] = {"tolerance_ms": 3, "rules": {"cycle_max_ms": 150, "cycle_min_ms": 90}}
    config = BusConfig.from_dict(data, os.path.join(HERE, "..", "examples"))
    violations = []
    analyzer = TimingAnalyzer(config, config.rules(64), violations.append)
    for i, start in enumerate((0, 200, 260)):             # cycles of 200 ms and 60 ms
        analyzer.on_frame(FrameRecord(2 * i, start, start + 8, 0x11, 0x01, 0, b"", b""))
        analyzer.on_frame(FrameRecord(2 * i + 1, start + 10, start + 20, 0x01, 0x11, 0x40, b"",
                                      b""))
    assert [v.rule for v in violations] == ["cycle_too_long", "cycle_too_short"]


def test_receive_errors():
    bus = Bus()
    bus.analyzer.on_error(ErrorRecord(5, "crc", b"\xa5\x11"))
    bus.analyzer.on_error(ErrorRecord(9, "timeout", b"\xa5"))
    assert bus.rules() == ["crc_error", "frame_timeout"]
