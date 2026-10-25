#!/usr/bin/env python3
#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector launcher.
#
#   python tools/baton_inspector/baton_inspector.py [--config bus.json]
#          [--mode simulation|master|monitor] [--port COM3] [--start] [--attach PORT]
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from batoninspector.gui.app import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
