#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Example system test suite with the Baton Inspector API.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Example system tests of the example bus (master 0x01, Output board 0x11, Panel 0x12).

Run against the simulation (default, no hardware)::

    python -m pytest tools/baton_inspector/examples -v

Run the same tests with the PC as the master of a real bus::

    BATON_MODE=master BATON_PORT=COM3 python -m pytest tools/baton_inspector/examples -v

Watch a run live: set ``BATON_LIVE_PORT=47555`` and start the GUI with
``python tools/baton_inspector/baton_inspector.py --attach 47555``.

Every test writes its session to ``test-results/<test name>.jsonl`` for the analysis
after the run (``batoninspector.load_session()``).
"""

import os
import re

import pytest

from batoninspector import Session

HERE = os.path.dirname(os.path.abspath(__file__))
BUS = os.path.join(HERE, "example_bus.json")
RESULTS = os.environ.get("BATON_RESULTS", os.path.join(os.getcwd(), "test-results"))
MODE = os.environ.get("BATON_MODE", "simulation")
SIMULATION = MODE == "simulation"


@pytest.fixture
def bus(request):
    """A session per test: simulation (fast, deterministic) or the real bus."""
    os.makedirs(RESULTS, exist_ok=True)
    name = re.sub(r"[^\w.-]+", "_", request.node.name)
    live = os.environ.get("BATON_LIVE_PORT")
    session = Session(BUS, mode=MODE, port=os.environ.get("BATON_PORT"),
                      speed=0 if SIMULATION and not live else 1.0,
                      record=os.path.join(RESULTS, f"{name}.jsonl"),
                      live_port=int(live) if live else None)
    with session:
        session.run_for(1500)          # let every node be polled a few times
        session.mark(f"start of {name}")
        yield session


def test_output_command_is_confirmed(bus):
    bus.send("Output board", "CMD_SET_OUTPUT", {"output": 2, "value": "on"}, priority="high")
    report = bus.wait_for("OUTPUT_REPORT", src="Output board", timeout_ms=1000)
    assert report.values == {"output": 2, "value": "on"}
    bus.assert_no_violations()


def test_every_slave_reports_its_uptime(bus):
    bus.run_for(2000)
    for slave in ("Output board", "Panel"):
        status = bus.latest_state("NODE_STATUS", slave)
        assert status is not None, f"no NODE_STATUS from {slave}"
        assert status["uptime"] >= 1


def test_slots_and_cycle_within_limits(bus):
    bus.run_for(3000)
    timing = bus.timing_summary()
    for slave, slot in timing["slots"].items():
        assert slot["count"] > 0, f"slave {slave} was never polled"
        assert slot["max_ms"] <= timing["limits"]["slot_max_ms"]
    assert timing["cycle"]["max_ms"] <= timing["limits"]["cycle_max_ms"]
    bus.assert_no_violations(min_severity="warning")


def test_no_report_without_a_command(bus):
    bus.expect_none("OUTPUT_REPORT", within_ms=2000)


@pytest.mark.skipif(not SIMULATION, reason="fault injection needs the simulation")
def test_unplugged_panel_is_detected_and_the_rest_keeps_working(bus):
    bus.slave("Panel").set_dead()
    start = bus.mark("Panel unplugged")
    bus.run_for(2000)
    assert bus.get_violations(rule="slave_unresponsive", since_ms=start)
    # the healthy slave is still served
    bus.send("Output board", "CMD_SET_OUTPUT", {"output": 0, "value": "off"})
    bus.wait_for("OUTPUT_REPORT", src="Output board", timeout_ms=2000)


@pytest.mark.skipif(not SIMULATION, reason="fault injection needs the simulation")
def test_late_slave_breaks_the_slot_limit(bus):
    # 255 ms: the RESP still starts before the master gives up (slot + gap + 10 = 262 ms),
    # but it ends after the 250 ms slot
    bus.slave("Panel").set_tx_delay(255)
    start = bus.mark("Panel answers late")
    bus.run_for(2000)
    overruns = bus.get_violations("error", "slot_overrun", since_ms=start)
    assert overruns and overruns[0].measured_ms > overruns[0].limit_ms


@pytest.mark.skipif(not SIMULATION, reason="fault injection needs the simulation")
def test_too_late_slave_is_a_missing_response(bus):
    bus.slave("Panel").set_tx_delay(300)
    start = bus.mark("Panel answers after the master gave up")
    bus.run_for(2000)
    assert bus.get_violations(rule="missing_resp", since_ms=start)
