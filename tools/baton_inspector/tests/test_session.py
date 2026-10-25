#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Description: Baton Inspector tests - sessions on the native bridge:
#              simulation, serial loopback, recording, live feed.
#
# Author:      Anton Vynohradov
# License:     Apache License 2.0
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

import os
import shutil
import threading
import time

import pytest

from batoninspector import Session, SessionError, WaitTimeout, load_session, messages_of
from batoninspector.live import LiveClient

HERE = os.path.dirname(os.path.abspath(__file__))
EXAMPLE = os.path.join(HERE, "..", "examples", "example_bus.json")

pytestmark = pytest.mark.skipif(shutil.which("cmake") is None, reason="CMake is needed to build "
                                "the native bridge")


@pytest.fixture
def sim():
    with Session(EXAMPLE, mode="simulation", speed=0) as session:
        yield session


def test_send_and_wait_for_the_auto_reply(sim):
    sim.run_for(500)
    sim.send("0x11", "CMD_SET_OUTPUT", {"output": 1, "value": "on"})
    reply = sim.wait_for("OUTPUT_REPORT", src="Output board", where={"value": "on"},
                         timeout_ms=1000)
    assert reply.values == {"output": 1, "value": "on"}
    assert reply.dst == 0x01
    # the master node received it exactly once
    sim.run_for(500)
    assert [m.name for m in sim.received].count("OUTPUT_REPORT") == 1
    sim.assert_no_violations()


def test_wait_timeout_and_expect_none(sim):
    sim.mark("nothing sent")
    with pytest.raises(WaitTimeout):
        sim.wait_for("OUTPUT_REPORT", timeout_ms=300)
    sim.expect_none("OUTPUT_REPORT", within_ms=300)
    sim.send("0x11", "CMD_SET_OUTPUT", {"output": 0, "value": "off"})
    with pytest.raises(AssertionError, match="unexpected OUTPUT_REPORT"):
        sim.expect_none("OUTPUT_REPORT", within_ms=1000)


def test_states_and_scripted_slaves(sim):
    sim.run_for(2100)
    assert sim.latest_state("NODE_STATUS", "0x11")["uptime"] >= 1
    panel = sim.slave("Panel")
    panel.send("0x11", "CMD_SET_OUTPUT", {"output": 3, "value": "on"})
    sim.wait_for("CMD_SET_OUTPUT", src="0x12", dst="0x11", timeout_ms=1000)
    report = sim.wait_for("OUTPUT_REPORT", src="0x11", dst="0x12", timeout_ms=1000)
    assert report.values["output"] == 3
    sim.run_for(300)
    assert any(m.name == "OUTPUT_REPORT" for m in panel.received)
    sim.publish("NODE_STATUS", {"uptime": 42})
    sim.wait_for("NODE_STATUS", src="0x01", where={"uptime": 42}, timeout_ms=1000)
    assert panel.stats()["rx_frames"] > 0


def test_api_errors(sim):
    with pytest.raises(SessionError, match="unknown message"):
        sim.send("0x11", "NOPE")
    with pytest.raises(SessionError, match="is a state"):
        sim.send("0x11", "NODE_STATUS", {"uptime": 1})
    with pytest.raises(SessionError, match="unknown or own address"):
        sim.send("0x01", "CMD_SET_OUTPUT", {"output": 0, "value": 0})
    with pytest.raises(Exception, match="missing field"):
        sim.send("0x11", "CMD_SET_OUTPUT", {"output": 0})
    sim.send("0x11", "CMD_SET_OUTPUT", data=b"\x01\x01")


def test_faults_produce_violations(sim):
    sim.run_for(500)
    sim.slave("0x12").set_dead()
    sim.run_for(2500)
    rules = {v.rule for v in sim.get_violations(since_ms=500)}
    assert {"missing_resp", "slave_unresponsive"} <= rules
    with pytest.raises(AssertionError, match="violation"):
        sim.assert_no_violations(since_ms=500)
    sim.assert_no_violations(since_ms=500, ignore=["missing_resp", "slave_unresponsive",
                                                   "missing_ack", "retransmission"])
    start = sim.mark("late slave")
    sim.slave("0x12").set_dead(False)
    sim.slave("0x11").set_tx_delay(280)
    sim.run_for(2000)
    assert sim.get_violations("error", "slot_overrun", since_ms=start)
    sim.slave("0x11").set_tx_delay(0)
    start = sim.mark("noise")
    sim.set_noise(0.02)
    sim.run_for(3000)
    assert sim.get_violations(rule="crc_error", since_ms=start)


def test_recording(tmp_path):
    path = str(tmp_path / "run.jsonl")
    with Session(EXAMPLE, mode="simulation", speed=0, record=path) as s:
        s.send("0x11", "CMD_SET_OUTPUT", {"output": 2, "value": "on"})
        s.wait_for("OUTPUT_REPORT", timeout_ms=1000)
        s.mark("done")
    log = load_session(path)
    assert log["session"][0]["mode"] == "simulation"
    assert log["end"][0]["violations"] == 0
    assert log["mark"][0]["text"] == "done"
    assert "0x01" in log["stats"][0]["nodes"]
    frame = log["frame"][0]
    assert {"t_start_ms", "t_end_ms", "wall", "frame", "src", "dst", "raw"} <= set(frame)
    reports = [m for m in messages_of(path) if m["name"] == "OUTPUT_REPORT"]
    assert reports[0]["values"] == {"output": 2, "value": "on"}


def test_realtime_simulation_and_live_feed():
    received = []
    hello = threading.Event()

    def on_record(record):
        received.append(record)
        if record["type"] == "hello":
            hello.set()

    with Session(EXAMPLE, mode="simulation", speed=1.0) as s:
        port = s.serve_live(0)
        client = LiveClient(port, on_record)
        assert hello.wait(3)
        time.sleep(0.5)
        assert s.now_ms >= 400
        client.close()
    kinds = {r["type"] for r in received}
    assert {"hello", "frame"} <= kinds
    assert received[0]["messages"]


def test_master_on_a_serial_loopback():
    # pyserial loop:// returns every written byte: the master hears its own POLLs, no slave
    # answers
    with Session(EXAMPLE, mode="master", port="loop://", rx_echo=True) as s:
        time.sleep(1.0)
        frames = [f for f in s.frames if f.frame_type == "POLL"]
        assert frames and all(f.own for f in frames)
        assert s.get_violations(rule="missing_resp")
        assert s.stats()["0x01"]["resp_timeouts"] >= 1


def test_monitor_needs_a_port_and_has_no_node():
    with pytest.raises(ValueError):
        Session(EXAMPLE, mode="monitor")
    with Session(EXAMPLE, mode="monitor", port="loop://") as s:
        with pytest.raises(SessionError, match="monitor"):
            s.send("0x11", "CMD_SET_OUTPUT", {"output": 0, "value": 0})
