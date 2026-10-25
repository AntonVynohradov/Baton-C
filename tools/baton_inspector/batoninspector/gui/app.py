#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector GUI - main window and command line.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Main window.

Modes: Simulation, Master and Monitor run a session in this process; Attach shows
the live feed of a session started by a test (``Session.serve_live()``).
"""

from __future__ import annotations

import argparse
import os
import sys
import threading
from typing import Any, Dict, Optional

from PySide6.QtCore import QSettings, Qt, QTimer, Signal
from PySide6.QtGui import QKeySequence, QPixmap
from PySide6.QtWidgets import (QApplication, QCheckBox, QComboBox, QDialog, QFileDialog,
                               QHBoxLayout, QLabel, QMainWindow, QMessageBox, QPushButton,
                               QSpinBox, QSplitter, QStatusBar, QTabWidget, QToolBar,
                               QVBoxLayout, QWidget)

from .. import __version__
from ..busconfig import BusConfig, ConfigError
from ..live import LiveClient
from ..native import NativeError
from ..session import Session, SessionError
from ..transport import list_serial_ports
from .panels import (FrameDetail, LayoutEditor, MessagesPanel, SendPanel, SimulationPanel,
                     StatsPanel, TimingPanel, ViolationsPanel)
from .sequence import SequenceView
from .store import BusInfo, RecordStore, session_listener
from .theme import DARK, LIGHT, Theme, style_sheet, ui_font

TOOL_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REPO_ROOT = os.path.abspath(os.path.join(TOOL_DIR, "..", ".."))
LOGO_ON_DARK = os.path.join(REPO_ROOT, "docs", "assets", "images", "brand",
                            "systemfromscratch-logo-on-dark.png")
MODES = (("Simulation", "simulation"), ("Master", "master"), ("Monitor", "monitor"),
         ("Attach to test session", "attach"))


def _label(text: str, role: str = "") -> QLabel:
    label = QLabel(text)
    if role:
        label.setProperty("role", role)
    return label


class MainWindow(QMainWindow):
    started = Signal(object)          # Session or exception, from the start thread
    attach_closed = Signal()

    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__()
        self.settings = QSettings("SystemFromScratch", "Baton Inspector")
        self.theme: Theme = LIGHT if (args.theme or self.settings.value("theme", "dark")) == "light" \
            else DARK
        self.session: Optional[Session] = None
        self.client: Optional[LiveClient] = None
        self.config_path: Optional[str] = args.config or self.settings.value("config", "") or None
        self.store = RecordStore()
        self.setWindowTitle("Baton Inspector")
        self.resize(1500, 920)

        self._build_toolbar(args)
        self._build_body()
        self._build_status()
        self.started.connect(self._session_started)
        self.attach_closed.connect(self._attach_closed)
        self.apply_theme(self.theme)
        self._update_mode_widgets()
        self._update_config_label()
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._update_status)
        self._timer.start(250)
        if args.attach:
            self.mode.setCurrentIndex(3)
            self.attach_port.setValue(args.attach)
            QTimer.singleShot(100, self.start_stop)
        elif args.start:
            QTimer.singleShot(100, self.start_stop)

    # ======================================================================
    # layout
    # ======================================================================

    def _build_toolbar(self, args: argparse.Namespace) -> None:
        bar = QToolBar("Session")
        bar.setMovable(False)
        self.addToolBar(bar)
        brand = QWidget()
        brand_layout = QVBoxLayout(brand)
        brand_layout.setContentsMargins(4, 0, 12, 0)
        brand_layout.setSpacing(0)
        brand_layout.addWidget(_label("BATON INSPECTOR", "eyebrow"))
        brand_layout.addWidget(_label("Bus diagnostics", "muted"))
        bar.addWidget(brand)

        self.mode = QComboBox()
        for title, key in MODES:
            self.mode.addItem(title, key)
        mode = args.mode or self.settings.value("mode", "simulation")
        self.mode.setCurrentIndex(max(0, [k for _, k in MODES].index(mode)
                                      if mode in [k for _, k in MODES] else 0))
        self.mode.currentIndexChanged.connect(self._update_mode_widgets)
        bar.addWidget(_label("Mode"))
        bar.addWidget(self.mode)

        self.config_button = QPushButton("Bus definition…")
        self.config_button.setToolTip("Open the bus definition (JSON)")
        self.config_button.clicked.connect(self._choose_config)
        self.config_label = _label("", "muted")
        self._config_actions = {self.config_button: bar.addWidget(self.config_button),
                                self.config_label: bar.addWidget(self.config_label)}

        self.port = QComboBox()
        self.port.setEditable(True)
        self.port.setMinimumWidth(150)
        self.port.setToolTip("Serial port or pyserial URL (e.g. socket://host:port)")
        self.refresh_ports = QPushButton("⟳")
        self.refresh_ports.setToolTip("Refresh the list of serial ports")
        self.refresh_ports.clicked.connect(self._fill_ports)
        self.echo = QCheckBox("Echo")
        self.echo.setToolTip("The adapter receives its own bytes (single-wire). "
                             "Off: RS-485 without echo")
        self.speed = QComboBox()
        for title, value in (("0.25×", 0.25), ("0.5×", 0.5), ("1× real time", 1.0), ("2×", 2.0),
                             ("5×", 5.0)):
            self.speed.addItem(title, value)
        self.speed.setCurrentIndex(2)
        self.attach_port = QSpinBox()
        self.attach_port.setRange(1, 65535)
        self.attach_port.setValue(int(self.settings.value("attach_port", 47555)))
        self.attach_port.setToolTip("Port of Session.serve_live() on this PC")
        self.port_label = _label("Port")
        self.speed_label = _label("Speed")
        self.attach_label = _label("Live port")
        self._actions = {}
        for widget in (self.config_button, self.config_label):
            self._actions[widget] = self._config_actions[widget]
        for widget in (self.port_label, self.port, self.refresh_ports, self.echo, self.speed_label,
                       self.speed, self.attach_label, self.attach_port):
            self._actions[widget] = bar.addWidget(widget)
        self._fill_ports()
        if args.port:
            self.port.setEditText(args.port)

        self.start_button = QPushButton("Start")
        self.start_button.setProperty("role", "primary")
        self.start_button.setShortcut(QKeySequence("F5"))
        self.start_button.setToolTip("Start or stop the session (F5)")
        self.start_button.clicked.connect(self.start_stop)
        bar.addWidget(self.start_button)

        spacer = QWidget()
        spacer.setStyleSheet("background: transparent;")
        spacer.setSizePolicy(spacer.sizePolicy().horizontalPolicy().Expanding,
                             spacer.sizePolicy().verticalPolicy().Preferred)
        bar.addWidget(spacer)

        self.record_button = QPushButton("● Record")
        self.record_button.setCheckable(True)
        self.record_button.setToolTip("Write the parsed session to a JSON Lines file")
        self.record_button.toggled.connect(self._record)
        self.mark_button = QPushButton("◆ Mark")
        self.mark_button.setToolTip("Insert a marker into the session (Ctrl+M)")
        self.mark_button.setShortcut(QKeySequence("Ctrl+M"))
        self.mark_button.clicked.connect(self._mark)
        self.follow_button = QPushButton("⤓ Follow")
        self.follow_button.setToolTip("Scroll the diagram with the live traffic (End)")
        self.follow_button.setShortcut(QKeySequence("End"))
        self.follow_button.clicked.connect(lambda: self.sequence.jump_to_end())
        zoom_out = QPushButton("−")
        zoom_out.setToolTip("Zoom out (Ctrl+wheel)")
        zoom_out.clicked.connect(lambda: self.sequence.set_zoom(self.sequence.px_per_ms / 1.5))
        zoom_in = QPushButton("+")
        zoom_in.setToolTip("Zoom in (Ctrl+wheel)")
        zoom_in.clicked.connect(lambda: self.sequence.set_zoom(self.sequence.px_per_ms * 1.5))
        clear = QPushButton("Clear")
        clear.setToolTip("Clear the views (the session keeps running)")
        clear.clicked.connect(lambda: self.store.clear())
        self.theme_button = QPushButton("☀")
        self.theme_button.setToolTip("Switch between dark and light theme")
        self.theme_button.clicked.connect(self._toggle_theme)
        about = QPushButton("About")
        about.clicked.connect(self._about)
        for widget in (self.record_button, self.mark_button, self.follow_button, zoom_out,
                       zoom_in, clear, self.theme_button, about):
            bar.addWidget(widget)

    def _build_body(self) -> None:
        self.sequence = SequenceView(self.store, self.theme)
        self.detail = FrameDetail(self.store)
        self.send_panel = SendPanel()
        self.sim_panel = SimulationPanel()
        self.timing_panel = TimingPanel(self.theme)
        self.stats_panel = StatsPanel()
        self.messages_panel = MessagesPanel(self.store, self.theme)
        self.violations_panel = ViolationsPanel(self.store, self.theme)
        self.editor = LayoutEditor()

        self.side = QTabWidget()
        self.side.addTab(self.detail, "Frame")
        self.side.addTab(self.send_panel, "Send")
        self.side.addTab(self.sim_panel, "Simulation")
        self.side.addTab(self.timing_panel, "Timing")
        self.side.addTab(self.stats_panel, "Statistics")
        self.bottom = QTabWidget()
        self.bottom.addTab(self.messages_panel, "Messages")
        self.bottom.addTab(self.violations_panel, "Violations")
        self.bottom.addTab(self.editor, "Message layouts && rules")

        top = QSplitter(Qt.Orientation.Horizontal)
        top.addWidget(self.sequence)
        top.addWidget(self.side)
        top.setStretchFactor(0, 3)
        top.setSizes([1050, 430])
        main = QSplitter(Qt.Orientation.Vertical)
        main.addWidget(top)
        main.addWidget(self.bottom)
        main.setSizes([600, 280])
        container = QWidget()
        layout = QVBoxLayout(container)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addWidget(main)
        self.setCentralWidget(container)

        self.sequence.frame_selected.connect(self._select_frame)
        self.messages_panel.frame_selected.connect(self._select_frame_and_show)
        self.violations_panel.frame_selected.connect(self._select_frame_and_show)
        self.violations_panel.time_selected.connect(self.sequence.scroll_to_time)
        self.store.timing_updated.connect(
            lambda record: self.timing_panel.show_summary(record, self._name))
        self.store.stats_updated.connect(lambda nodes: self.stats_panel.show_stats(nodes,
                                                                                    self._name))
        self.store.other_added.connect(self._update_violation_tab)
        self.editor.applied.connect(self._apply_definition)
        self.editor.saved.connect(self._definition_saved)

    def _build_status(self) -> None:
        status = QStatusBar()
        self.setStatusBar(status)
        self.state_label = _label("Stopped", "badge")
        self.time_label = _label("")
        self.count_label = _label("")
        self.record_label = _label("")
        status.addWidget(self.state_label)
        status.addWidget(self.time_label)
        status.addWidget(self.count_label)
        status.addPermanentWidget(self.record_label)

    # ======================================================================
    # helpers
    # ======================================================================

    def _name(self, address: int) -> str:
        return self.store.info.name(address) if self.store.info else f"0x{address:02X}"

    def apply_theme(self, theme: Theme) -> None:
        self.theme = theme
        app = QApplication.instance()
        app.setStyleSheet(style_sheet(theme))
        self.sequence.set_theme(theme)
        self.messages_panel.model.theme = theme
        self.violations_panel.model.theme = theme
        self.timing_panel.theme = theme
        self.theme_button.setText("☀" if theme.name == "dark" else "☾")
        self.settings.setValue("theme", theme.name)

    def _toggle_theme(self) -> None:
        self.apply_theme(LIGHT if self.theme.name == "dark" else DARK)

    def _mode(self) -> str:
        return self.mode.currentData()

    def _running(self) -> bool:
        return self.session is not None or self.client is not None

    def _update_mode_widgets(self) -> None:
        mode = self._mode()
        serial = mode in ("master", "monitor")
        visible = {self.port_label: serial, self.port: serial, self.refresh_ports: serial,
                   self.echo: mode == "master", self.speed_label: mode == "simulation",
                   self.speed: mode == "simulation", self.attach_label: mode == "attach",
                   self.attach_port: mode == "attach", self.config_button: mode != "attach",
                   self.config_label: mode != "attach"}
        for widget, show in visible.items():
            self._actions[widget].setVisible(show)
        self.settings.setValue("mode", mode)

    def _fill_ports(self) -> None:
        current = self.port.currentText()
        self.port.clear()
        for device, description in list_serial_ports():
            self.port.addItem(device)
            self.port.setItemData(self.port.count() - 1, description, Qt.ItemDataRole.ToolTipRole)
        saved = self.settings.value("port", "")
        self.port.setEditText(current or saved or (self.port.itemText(0) if self.port.count()
                                                   else ""))

    def _choose_config(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Open bus definition",
                                              os.path.dirname(self.config_path or TOOL_DIR),
                                              "Bus definition (*.json)")
        if path:
            self.config_path = path
            self.settings.setValue("config", path)
            self._update_config_label()

    def _update_config_label(self) -> None:
        if not self.config_path:
            self.config_label.setText("no bus definition")
            return
        self.config_label.setText(os.path.basename(self.config_path))
        self.config_label.setToolTip(self.config_path)
        try:
            config = BusConfig.load(self.config_path)
            self.echo.setChecked(bool(config.bus["rx_echo"]))
            speed = config.simulation.speed
            index = self.speed.findData(speed)
            if index >= 0:
                self.speed.setCurrentIndex(index)
        except (ConfigError, OSError) as exc:
            self.config_label.setText(f"{os.path.basename(self.config_path)} (invalid)")
            self.config_label.setToolTip(str(exc))

    def _select_frame(self, index: int) -> None:
        self.detail.show_frame(index)
        self.side.setCurrentWidget(self.detail)

    def _select_frame_and_show(self, index: int) -> None:
        self.sequence.select_frame(index)
        self._select_frame(index)

    def _update_violation_tab(self) -> None:
        errors = sum(1 for v in self.store.violations if v["severity"] == "error")
        warnings = sum(1 for v in self.store.violations if v["severity"] == "warning")
        self.bottom.setTabText(1, f"Violations ({errors} E · {warnings} W)"
                               if errors or warnings else "Violations")

    def _update_status(self) -> None:
        if self.session is not None:
            self.store.now_ms = max(self.store.now_ms, self.session.now_ms)
            if self.session.error is not None:
                error = self.session.error
                self._stop()
                QMessageBox.critical(self, "Session stopped", str(error))
                return
        self.time_label.setText(f"t = {self.store.now_ms / 1000:.3f} s")
        self.count_label.setText(f"{len(self.store.frames)} frames · "
                                 f"{len(self.store.errors)} receive errors · "
                                 f"{len(self.store.violations)} violations")
        if self._running():
            self.sequence.viewport().update()

    # ======================================================================
    # sessions
    # ======================================================================

    def start_stop(self) -> None:
        if self._running():
            self._stop()
            return
        mode = self._mode()
        if mode == "attach":
            self._attach()
            return
        if not self.config_path:
            self._choose_config()
            if not self.config_path:
                return
        port = self.port.currentText().strip() or None
        if mode != "simulation" and not port:
            QMessageBox.warning(self, "Serial port", "Choose a serial port first.")
            return
        self.settings.setValue("port", port or "")
        self.start_button.setEnabled(False)
        self.state_label.setText("Starting… (building the native bridge on first use)")
        kwargs: Dict[str, Any] = {"mode": mode, "port": port}
        if mode == "master":
            kwargs["rx_echo"] = self.echo.isChecked()
        if mode == "simulation":
            kwargs["speed"] = self.speed.currentData()
        path = self.config_path

        def work() -> None:
            try:
                session = Session(path, **kwargs)
                session.add_listener(session_listener(self.store))
                session.start()
                self.started.emit(session)
            except (ConfigError, NativeError, SessionError, OSError, ValueError,
                    RuntimeError) as exc:
                self.started.emit(exc)

        self.store.clear()
        threading.Thread(target=work, daemon=True).start()

    def _session_started(self, result: object) -> None:
        self.start_button.setEnabled(True)
        if isinstance(result, BaseException):
            self.state_label.setText("Stopped")
            QMessageBox.critical(self, "Cannot start the session", str(result))
            return
        session: Session = result
        self.session = session
        config = session.config
        info = BusInfo(config.master, config.slaves, config.node_names,
                       session.analyzer.summary()["limits"],
                       [{"name": m.name, "id": m.msg_id, "max_length": m.max_length,
                         "kind": "state" if m.is_state else "event"} for m in session.bridge.messages],
                       config.name)
        self.store.info = info
        self.store.reset.emit()
        self.send_panel.set_session(session)
        self.sim_panel.set_session(session)
        self.editor.load(config.raw, info.messages, config.path, True)
        self._running_ui(f"{session.mode.capitalize()} · {config.name}")
        if self.record_button.isChecked():
            self._record(True)

    def _attach(self) -> None:
        port = self.attach_port.value()
        self.settings.setValue("attach_port", port)
        self.store.clear()

        def on_record(record: Dict[str, Any]) -> None:
            if record.get("type") == "hello":
                self._hello = record
                QTimer.singleShot(0, self._apply_hello)
            else:
                self.store.push(record)

        try:
            self.client = LiveClient(port, on_record, on_closed=self.attach_closed.emit)
        except OSError as exc:
            QMessageBox.critical(self, "Attach", f"No session on 127.0.0.1:{port}: {exc}")
            self.client = None
            return
        self._running_ui(f"Attached to 127.0.0.1:{port}")

    def _apply_hello(self) -> None:
        hello = self._hello
        names = {int(k): v for k, v in hello.get("node_names", {}).items()}
        info = BusInfo(hello["master"], hello["slaves"], names, hello.get("limits", {}),
                       hello.get("messages", []), hello.get("name", "Baton bus"))
        self.store.info = info
        self.store.reset.emit()
        self.editor.load(hello.get("config", {}), info.messages, None, False)
        self.state_label.setText(f"Attached · {info.title} · {hello.get('mode', '')}")

    def _attach_closed(self) -> None:
        if self.client is not None:
            self.client = None
            self._stopped_ui("Test session ended")

    def _stop(self) -> None:
        if self.client is not None:
            client, self.client = self.client, None
            client.close()
        if self.session is not None:
            session, self.session = self.session, None
            session.stop()
        self.send_panel.set_session(None)
        self.sim_panel.set_session(None)
        self.record_label.setText("")
        self._stopped_ui("Stopped")

    def _running_ui(self, text: str) -> None:
        self.state_label.setText(text)
        self.start_button.setText("Stop")
        self.start_button.setProperty("role", "danger")
        self.start_button.style().polish(self.start_button)
        for widget in (self.mode, self.config_button, self.port, self.echo, self.speed,
                       self.attach_port, self.refresh_ports):
            widget.setEnabled(False)

    def _stopped_ui(self, text: str) -> None:
        self.state_label.setText(text)
        self.start_button.setText("Start")
        self.start_button.setProperty("role", "primary")
        self.start_button.style().polish(self.start_button)
        for widget in (self.mode, self.config_button, self.port, self.echo, self.speed,
                       self.attach_port, self.refresh_ports):
            widget.setEnabled(True)

    def _record(self, on: bool) -> None:
        if self.session is None:
            if on and self.client is not None:
                QMessageBox.information(self, "Record", "An attached session is recorded by the "
                                        "test itself (Session(record=...)).")
                self.record_button.setChecked(False)
            return
        if on:
            path, _ = QFileDialog.getSaveFileName(self, "Record session", "session.jsonl",
                                                  "JSON Lines (*.jsonl)")
            if not path:
                self.record_button.setChecked(False)
                return
            self.session.record(path)
            self.record_label.setText(f"● recording {os.path.basename(path)}")
        else:
            path = self.session.stop_recording()
            self.record_label.setText(f"saved {os.path.basename(path)}" if path else "")

    def _mark(self) -> None:
        if self.session is not None:
            self.session.mark(f"mark at {self.session.now_ms / 1000:.1f} s")

    def _apply_definition(self, raw: Dict[str, Any]) -> None:
        if self.session is None:
            return
        base = os.path.dirname(self.session.config.path or self.config_path or TOOL_DIR)
        try:
            config = BusConfig.from_dict(raw, base, self.session.config.path)
            notes = self.session.apply_config(config)
        except (ConfigError, SessionError) as exc:
            QMessageBox.warning(self, "Bus definition", str(exc))
            return
        self.editor.status.setText("Applied: new frames are decoded with the new layouts."
                                   + (" " + "; ".join(notes) if notes else ""))

    def _definition_saved(self, path: str) -> None:
        if self.session is None:
            self.config_path = path
            self.settings.setValue("config", path)
            self._update_config_label()

    def _about(self) -> None:
        dialog = QDialog(self)
        dialog.setWindowTitle("About Baton Inspector")
        layout = QVBoxLayout(dialog)
        if os.path.isfile(LOGO_ON_DARK):
            logo = QLabel()
            logo.setPixmap(QPixmap(LOGO_ON_DARK).scaledToHeight(
                90, Qt.TransformationMode.SmoothTransformation))
            logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
            logo.setStyleSheet("background: #050b16; border-radius: 10px; padding: 14px;")
            layout.addWidget(logo)
        layout.addWidget(_label(f"Baton Inspector {__version__}", "title"))
        text = QLabel("Diagnostics and test automation for Baton-C buses: master, passive "
                      "monitor and simulation, live sequence diagram with timing, field "
                      "decoding, timing rules and JSON Lines recording.<br><br>"
                      "© 2026 Anton Vynohradov · SystemFromScratch® · Apache License 2.0<br>"
                      "SystemFromScratch® is a registered trademark.<br>"
                      "<a href='https://systemfromscratch.com'>systemfromscratch.com</a>")
        text.setWordWrap(True)
        text.setOpenExternalLinks(True)
        layout.addWidget(text)
        close = QPushButton("Close")
        close.clicked.connect(dialog.accept)
        row = QHBoxLayout()
        row.addStretch(1)
        row.addWidget(close)
        layout.addLayout(row)
        dialog.resize(460, 340)
        dialog.exec()

    def closeEvent(self, event) -> None:  # noqa: N802
        self._stop()
        super().closeEvent(event)


def parse_args(argv=None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Baton Inspector - Baton-C bus diagnostics")
    parser.add_argument("--config", help="bus definition (JSON)")
    parser.add_argument("--mode", choices=["simulation", "master", "monitor"])
    parser.add_argument("--port", help="serial port or pyserial URL (master, monitor)")
    parser.add_argument("--start", action="store_true", help="start the session at once")
    parser.add_argument("--attach", type=int, metavar="PORT",
                        help="attach to a test session published with Session.serve_live(PORT)")
    parser.add_argument("--theme", choices=["dark", "light"])
    return parser.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(argv)
    if args.config:
        args.config = os.path.abspath(args.config)
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough)
    app = QApplication(sys.argv[:1])
    app.setApplicationName("Baton Inspector")
    app.setOrganizationName("SystemFromScratch")
    app.setStyle("Fusion")
    app.setFont(ui_font(10))
    window = MainWindow(args)
    window.show()
    return app.exec()

