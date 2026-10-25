#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - takes the documentation screenshots by driving
#              the real GUI in simulation mode.
#
#   python tools/baton_inspector/screenshots.py [output folder]
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

from PySide6.QtCore import QTimer  # noqa: E402
from PySide6.QtWidgets import QApplication  # noqa: E402

from batoninspector.gui.app import MainWindow, parse_args  # noqa: E402
from batoninspector.gui.theme import DARK, LIGHT, ui_font  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "docs", "assets", "images",
                                                           "inspector")
CONFIG = os.path.join(HERE, "examples", "example_bus.json")


def main() -> int:
    os.makedirs(OUT, exist_ok=True)
    app = QApplication(sys.argv[:1])
    app.setStyle("Fusion")
    app.setFont(ui_font(10))
    window = MainWindow(parse_args(["--config", CONFIG, "--mode", "simulation", "--theme",
                                    "dark"]))
    window.settings.clear()
    window.resize(1600, 960)
    window.show()
    steps = []

    def shot(name, widget=None):
        (widget or window).grab().save(os.path.join(OUT, name))
        print("wrote", name)

    def step(delay_ms):
        def wrap(function):
            steps.append((delay_ms, function))
            return function
        return wrap

    @step(300)
    def start():
        window.speed.setCurrentIndex(window.speed.findData(2.0))
        window.start_stop()

    @step(2500)
    def traffic():
        s = window.session
        s.send("0x11", "CMD_SET_OUTPUT", {"output": 1, "value": "on"}, priority="high")
        s.slave("0x12").send("0x11", "CMD_SET_OUTPUT", {"output": 3, "value": "off"})

    @step(1500)
    def fault():
        window.session.mark("slave 0x12 answers 300 ms late")
        window.session.slave("0x12").set_tx_delay(300)

    @step(2500)
    def recover():
        window.session.slave("0x12").set_tx_delay(0)
        window.session.set_noise(0.01)

    @step(2500)
    def capture_main():
        window.session.set_noise(0.0)
        frames = [f for f in window.store.frames if f["messages"] and f["frame"] == "DATA"]
        window.sequence.set_zoom(3.0)
        if frames:
            window._select_frame_and_show(frames[-1]["index"])
        window.bottom.setCurrentIndex(0)
        shot("inspector-main.png")

    @step(400)
    def capture_violations():
        window.bottom.setCurrentIndex(1)
        violation = next((v for v in window.store.violations if v["rule"] == "slot_overrun"), None)
        if violation and violation.get("frames"):
            window._select_frame_and_show(violation["frames"][-1])
        window.side.setCurrentWidget(window.timing_panel)
        shot("inspector-violations.png")

    @step(400)
    def capture_send():
        window.side.setCurrentWidget(window.send_panel)
        index = window.send_panel.message.findData("CMD_SET_OUTPUT")
        window.send_panel.message.setCurrentIndex(index)
        window.bottom.setCurrentIndex(2)
        window.editor.list.setCurrentRow(1)
        shot("inspector-layouts.png")

    @step(400)
    def capture_light():
        window.apply_theme(LIGHT)
        window.side.setCurrentWidget(window.sim_panel)
        window.bottom.setCurrentIndex(0)
        shot("inspector-light.png")
        window.apply_theme(DARK)

    @step(200)
    def finish():
        window._stop()
        app.quit()

    total = 0
    for delay, function in steps:
        total += delay
        QTimer.singleShot(total, function)
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
