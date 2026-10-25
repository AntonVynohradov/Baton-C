#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - Python API (headless, no Qt needed).
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Baton Inspector: diagnostics and test automation for Baton-C buses.

    from batoninspector import Session

    with Session("bus.json", mode="simulation", speed=0, record="run.jsonl") as s:
        s.send("0x11", "CMD_SET_OUTPUT", {"output": 0, "value": "on"})
        report = s.wait_for("OUTPUT_REPORT", src="0x11", timeout_ms=1000)
        s.assert_no_violations()
"""

from .busconfig import BusConfig, ConfigError, TimingRules
from .layout import Field, Layout, LayoutError
from .model import ErrorRecord, FrameRecord, MessageRecord, Violation
from .native import Bridge, NativeError
from .recorder import load_session, messages_of
from .session import MODES, Session, SessionError, SimSlave, WaitTimeout
from .transport import list_serial_ports

__all__ = ["BusConfig", "ConfigError", "TimingRules", "Field", "Layout", "LayoutError",
           "ErrorRecord", "FrameRecord", "MessageRecord", "Violation", "Bridge", "NativeError",
           "load_session", "messages_of", "MODES", "Session", "SessionError", "SimSlave",
           "WaitTimeout", "list_serial_ports"]

__version__ = "1.0.0"
