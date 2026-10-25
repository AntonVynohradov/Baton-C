#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector GUI - panels: messages, violations, frame
#              details, timing, statistics, send, simulation, layout editor.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Panels of the main window."""

from __future__ import annotations

import copy
import json
from typing import Any, Callable, Dict, List, Optional

from PySide6.QtCore import (QAbstractTableModel, QModelIndex, QSortFilterProxyModel, Qt, Signal)
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QComboBox, QDoubleSpinBox,
                               QFormLayout, QFrame, QGridLayout, QGroupBox, QHBoxLayout,
                               QHeaderView, QLabel, QLineEdit, QListWidget, QListWidgetItem,
                               QPlainTextEdit, QPushButton, QSpinBox, QSplitter,
                               QTableView, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)

from ..layout import Layout, LayoutError
from .store import RecordStore
from .theme import FRAME_COLORS, SEVERITY_COLORS, Theme, mono_font

ALIGN_RIGHT = int(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)


def _label(text: str, role: str = "") -> QLabel:
    label = QLabel(text)
    if role:
        label.setProperty("role", role)
    return label


def _table(view: QTableView) -> QTableView:
    view.setAlternatingRowColors(True)
    view.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
    view.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
    view.verticalHeader().setVisible(False)
    view.verticalHeader().setDefaultSectionSize(24)
    view.setShowGrid(False)
    view.setWordWrap(False)
    view.horizontalHeader().setStretchLastSection(True)
    return view


# ==============================================================================
# messages
# ==============================================================================

class MessageModel(QAbstractTableModel):
    HEADERS = ("Time [ms]", "From", "To", "Frame", "Message", "Kind", "Values", "Raw data")

    def __init__(self, store: RecordStore, theme: Theme) -> None:
        super().__init__()
        self.store = store
        self.theme = theme
        self.rows: List[tuple] = []
        store.reset.connect(self._reset)
        store.added.connect(self._added)

    def _reset(self) -> None:
        self.beginResetModel()
        self.rows = []
        self.endResetModel()
        self._added(0, len(self.store.frames))

    def _added(self, first: int, count: int) -> None:
        new = []
        for frame in self.store.frames[first:first + count]:
            for message in frame["messages"]:
                new.append((frame, message))
        if new:
            self.beginInsertRows(QModelIndex(), len(self.rows), len(self.rows) + len(new) - 1)
            self.rows.extend(new)
            self.endInsertRows()

    def rowCount(self, parent=QModelIndex()) -> int:  # noqa: N802
        return 0 if parent.isValid() else len(self.rows)

    def columnCount(self, parent=QModelIndex()) -> int:  # noqa: N802
        return len(self.HEADERS)

    def headerData(self, section, orientation, role=Qt.ItemDataRole.DisplayRole):  # noqa: N802
        if orientation == Qt.Orientation.Horizontal and role == Qt.ItemDataRole.DisplayRole:
            return self.HEADERS[section]
        return None

    def data(self, index, role=Qt.ItemDataRole.DisplayRole):
        frame, message = self.rows[index.row()]
        column = index.column()
        info = self.store.info
        if role == Qt.ItemDataRole.DisplayRole:
            return [f"{frame['t_end_ms']:.2f}", info.name(message["src"]) if info else message["src"],
                    info.name(message["dst"]) if info else message["dst"],
                    frame["frame"] + (" (retry)" if message["retransmission"] else ""),
                    message["name"], message["kind"], message["text"], message["data"]][column]
        if role == Qt.ItemDataRole.ForegroundRole:
            if column == 3:
                return self.theme.color(FRAME_COLORS[frame["frame"]])
            if message["retransmission"] or message["kind"] == "unknown":
                return self.theme.color("muted")
        if role == Qt.ItemDataRole.TextAlignmentRole and column == 0:
            return ALIGN_RIGHT
        if role == Qt.ItemDataRole.UserRole:
            return frame["index"]
        return None


class MessagesPanel(QWidget):
    frame_selected = Signal(int)

    def __init__(self, store: RecordStore, theme: Theme) -> None:
        super().__init__()
        self.model = MessageModel(store, theme)
        self.proxy = QSortFilterProxyModel()
        self.proxy.setSourceModel(self.model)
        self.proxy.setFilterCaseSensitivity(Qt.CaseSensitivity.CaseInsensitive)
        self.proxy.setFilterKeyColumn(-1)
        self.filter = QLineEdit()
        self.filter.setPlaceholderText("Filter: message name, node, field value…")
        self.filter.setClearButtonEnabled(True)
        self.filter.textChanged.connect(self.proxy.setFilterFixedString)
        self.hide_retries = QCheckBox("Hide retransmissions")
        self.hide_retries.toggled.connect(self._apply_retry_filter)
        self.follow = QCheckBox("Follow")
        self.follow.setChecked(True)
        self.view = _table(QTableView())
        self.view.setModel(self.proxy)
        self.view.setFont(mono_font(9))
        header = self.view.horizontalHeader()
        for column, width in enumerate((90, 110, 110, 90, 150, 60, 320)):
            header.resizeSection(column, width)
        self.view.clicked.connect(lambda index: self.frame_selected.emit(
            self.proxy.data(index, Qt.ItemDataRole.UserRole)))
        self.model.rowsInserted.connect(self._scroll)
        top = QHBoxLayout()
        top.addWidget(self.filter, 1)
        top.addWidget(self.hide_retries)
        top.addWidget(self.follow)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addLayout(top)
        layout.addWidget(self.view)

    def _apply_retry_filter(self, hide: bool) -> None:
        for row, (_, message) in enumerate(self.model.rows):
            source = self.model.index(row, 0)
            proxy_row = self.proxy.mapFromSource(source).row()
            if proxy_row >= 0:
                self.view.setRowHidden(proxy_row, hide and message["retransmission"])

    def _scroll(self) -> None:
        if self.hide_retries.isChecked():
            self._apply_retry_filter(True)
        if self.follow.isChecked():
            self.view.scrollToBottom()


# ==============================================================================
# violations
# ==============================================================================

class ViolationModel(QAbstractTableModel):
    HEADERS = ("Time [ms]", "Severity", "Rule", "Node", "Measured", "Limit", "Description")

    def __init__(self, store: RecordStore, theme: Theme) -> None:
        super().__init__()
        self.store = store
        self.theme = theme
        self.count = 0
        store.reset.connect(self._reset)
        store.other_added.connect(self._changed)

    def _reset(self) -> None:
        self.beginResetModel()
        self.count = 0
        self.endResetModel()

    def _changed(self) -> None:
        total = len(self.store.violations)
        if total > self.count:
            self.beginInsertRows(QModelIndex(), self.count, total - 1)
            self.count = total
            self.endInsertRows()

    def rowCount(self, parent=QModelIndex()) -> int:  # noqa: N802
        return 0 if parent.isValid() else self.count

    def columnCount(self, parent=QModelIndex()) -> int:  # noqa: N802
        return len(self.HEADERS)

    def headerData(self, section, orientation, role=Qt.ItemDataRole.DisplayRole):  # noqa: N802
        if orientation == Qt.Orientation.Horizontal and role == Qt.ItemDataRole.DisplayRole:
            return self.HEADERS[section]
        return None

    def data(self, index, role=Qt.ItemDataRole.DisplayRole):
        v = self.store.violations[index.row()]
        column = index.column()
        if role == Qt.ItemDataRole.DisplayRole:
            info = self.store.info
            node = v.get("node")
            return [f"{v['t_ms']:.2f}", v["severity"], v["rule"],
                    (info.name(node) if info else node) if node is not None else "",
                    f"{v['measured_ms']:.1f} ms" if "measured_ms" in v else "",
                    f"{v['limit_ms']:.1f} ms" if "limit_ms" in v else "", v["text"]][column]
        if role == Qt.ItemDataRole.ForegroundRole and column in (1, 2):
            return self.theme.color(SEVERITY_COLORS[v["severity"]])
        if role == Qt.ItemDataRole.TextAlignmentRole and column in (0, 4, 5):
            return ALIGN_RIGHT
        if role == Qt.ItemDataRole.UserRole:
            return v
        return None


class ViolationsPanel(QWidget):
    time_selected = Signal(float)
    frame_selected = Signal(int)

    def __init__(self, store: RecordStore, theme: Theme) -> None:
        super().__init__()
        self.store = store
        self.model = ViolationModel(store, theme)
        self.proxy = QSortFilterProxyModel()
        self.proxy.setSourceModel(self.model)
        self.proxy.setFilterKeyColumn(1)
        self.severity = QComboBox()
        self.severity.addItems(["All severities", "error", "warning", "info"])
        self.severity.currentTextChanged.connect(
            lambda text: self.proxy.setFilterFixedString("" if text.startswith("All") else text))
        self.counts = _label("", "muted")
        self.view = _table(QTableView())
        self.view.setModel(self.proxy)
        header = self.view.horizontalHeader()
        for column, width in enumerate((90, 70, 140, 110, 80, 80)):
            header.resizeSection(column, width)
        self.view.clicked.connect(self._clicked)
        store.other_added.connect(self._update_counts)
        store.reset.connect(self._update_counts)
        top = QHBoxLayout()
        top.addWidget(self.severity)
        top.addStretch(1)
        top.addWidget(self.counts)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addLayout(top)
        layout.addWidget(self.view)

    def _update_counts(self) -> None:
        counts = {"error": 0, "warning": 0, "info": 0}
        for v in self.store.violations:
            counts[v["severity"]] += 1
        self.counts.setText(f"{counts['error']} errors · {counts['warning']} warnings · "
                            f"{counts['info']} info")

    def _clicked(self, index) -> None:
        v = self.proxy.data(index, Qt.ItemDataRole.UserRole)
        if v.get("frames"):
            self.frame_selected.emit(v["frames"][-1])
        else:
            self.time_selected.emit(v["t_ms"])


# ==============================================================================
# frame details
# ==============================================================================

class FrameDetail(QPlainTextEdit):
    def __init__(self, store: RecordStore) -> None:
        super().__init__()
        self.store = store
        self.setReadOnly(True)
        self.setFont(mono_font(9))
        self.setPlaceholderText("Click a frame in the diagram or a row in a table")

    def show_frame(self, index: int) -> None:
        frame = self.store.frame_by_index(index)
        info = self.store.info
        if frame is None or info is None:
            return
        lines = [f"Frame #{frame['index']}  {frame['frame']}  {info.label(frame['src'])} -> "
                 f"{info.label(frame['dst'])}",
                 f"start {frame['t_start_ms']:.3f} ms   end {frame['t_end_ms']:.3f} ms   "
                 f"duration {frame['t_end_ms'] - frame['t_start_ms']:.3f} ms",
                 f"seq {frame['seq']}   FIRST {'yes' if frame['first'] else 'no'}   "
                 f"payload {frame['length']} bytes"
                 f"{'   RETRANSMISSION' if frame['retransmission'] else ''}"
                 f"{'   sent by this session' if frame['own'] else ''}"]
        prefix = frame.get("prefix") or {}
        if "ackb" in prefix:
            ackb = prefix["ackb"]
            lines.append(f"ACKB {'not valid' if ackb is None else f'valid, seq {ackb}'}"
                         + (f"   NACK mask 0x{prefix['nack']:X}" if prefix.get("nack") else ""))
        lines += ["", "Raw bytes:"]
        raw = frame["raw"].split()
        for i in range(0, len(raw), 16):
            lines.append(f"  {i:04X}  " + " ".join(raw[i:i + 16]))
        if frame["messages"]:
            lines += ["", "Messages:"]
            for m in frame["messages"]:
                lines.append(f"  {m['name']} (0x{m['id']:02X}, {m['kind']}, "
                             f"{len(m['data'].split())} bytes)"
                             + ("  [retransmission: not delivered again]"
                                if m["retransmission"] else ""))
                if m["values"]:
                    for key, value in m["values"].items():
                        lines.append(f"      {key:<16} {value}")
                else:
                    lines.append(f"      data {m['data'] or '(empty)'}")
        violations = [v for v in self.store.violations if frame["index"] in v.get("frames", [])]
        if violations:
            lines += ["", "Violations:"]
            lines += [f"  {v['severity'].upper()} {v['rule']}: {v['text']}" for v in violations]
        self.setPlainText("\n".join(lines))


# ==============================================================================
# timing and statistics
# ==============================================================================

class TimingPanel(QWidget):
    def __init__(self, theme: Theme) -> None:
        super().__init__()
        self.theme = theme
        self.table = QTableWidget(0, 6)
        self.table.setHorizontalHeaderLabels(["Quantity", "Count", "Min [ms]", "Avg [ms]",
                                              "Max [ms]", "Limit [ms]"])
        _table(self.table)
        header = self.table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        for column in range(1, 6):
            header.setSectionResizeMode(column, QHeaderView.ResizeMode.ResizeToContents)
        self.limits = _label("", "muted")
        self.limits.setWordWrap(True)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addWidget(self.table)
        layout.addWidget(self.limits)

    def show_summary(self, summary: Dict[str, Any], names: Callable[[int], str]) -> None:
        limits = summary.get("limits", {})
        rows = []
        tol = limits.get("tolerance_ms", 0)
        for key, series in summary.get("slots", {}).items():
            rows.append((f"Slot of {names(int(key, 16))}", series, limits.get("slot_max_ms")))
        rows.append(("ACK after DATA", summary.get("ack", {}), limits.get("ack_max_ms")))
        rows.append(("Polling cycle", summary.get("cycle", {}), limits.get("cycle_max_ms")))
        self.table.setRowCount(len(rows))
        for r, (name, series, limit) in enumerate(rows):
            values = [name, str(series.get("count", 0)), _ms(series.get("min_ms")),
                      _ms(series.get("avg_ms")), _ms(series.get("max_ms")), _ms(limit)]
            for c, value in enumerate(values):
                item = QTableWidgetItem(value)
                if c:
                    item.setTextAlignment(ALIGN_RIGHT)
                if c == 4 and limit is not None and series.get("max_ms") is not None \
                        and series["max_ms"] > limit + tol:
                    item.setForeground(self.theme.color("error"))
                self.table.setItem(r, c, item)
        self.limits.setText(f"Limits are compared with limit + tolerance ({tol} ms). "
                            f"Minimum cycle: {limits.get('cycle_min_ms', 0)} ms · "
                            f"unanswered POLLs before 'unresponsive': "
                            f"{limits.get('unanswered_polls', '-')}")


def _ms(value: Optional[float]) -> str:
    return "" if value is None else f"{value:.2f}"


class StatsPanel(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.table = QTableWidget(0, 0)
        _table(self.table)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addWidget(_label("baton_stats of the nodes run by this session", "muted"))
        layout.addWidget(self.table)

    def show_stats(self, nodes: Dict[str, Dict[str, int]], names: Callable[[int], str]) -> None:
        keys = list(nodes)
        counters = list(next(iter(nodes.values())).keys()) if nodes else []
        self.table.setColumnCount(len(keys) + 1)
        self.table.setHorizontalHeaderLabels(["Counter"] + [names(int(k, 16)) for k in keys])
        self.table.setRowCount(len(counters))
        for r, counter in enumerate(counters):
            self.table.setItem(r, 0, QTableWidgetItem(counter))
            for c, key in enumerate(keys):
                item = QTableWidgetItem(str(nodes[key][counter]))
                item.setTextAlignment(ALIGN_RIGHT)
                self.table.setItem(r, c + 1, item)
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)


# ==============================================================================
# send
# ==============================================================================

class SendPanel(QWidget):
    """Send events and publish states from the master (or a simulated slave)."""

    def __init__(self) -> None:
        super().__init__()
        self.session = None
        self.sender = QComboBox()
        self.target = QComboBox()
        self.message = QComboBox()
        self.priority = QComboBox()
        self.priority.addItems(["normal", "high"])
        self.raw = QCheckBox("Raw data (hex)")
        self.raw_edit = QLineEdit()
        self.raw_edit.setPlaceholderText("e.g. 01 00")
        self.raw_edit.setFont(mono_font(9))
        self.fields_box = QGroupBox("Fields")
        self.fields_form = QFormLayout(self.fields_box)
        self.editors: Dict[str, QWidget] = {}
        self.button = QPushButton("Send")
        self.button.setProperty("role", "primary")
        self.result = _label("", "muted")
        self.result.setWordWrap(True)
        self.message.currentIndexChanged.connect(self._message_changed)
        self.raw.toggled.connect(self._raw_toggled)
        self.button.clicked.connect(self._send)
        form = QFormLayout()
        form.addRow("From", self.sender)
        form.addRow("Message", self.message)
        form.addRow("To", self.target)
        form.addRow("Priority", self.priority)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addLayout(form)
        layout.addWidget(self.fields_box)
        layout.addWidget(self.raw)
        layout.addWidget(self.raw_edit)
        layout.addWidget(self.button)
        layout.addWidget(self.result)
        layout.addStretch(1)
        self.set_session(None)

    def set_session(self, session) -> None:
        self.session = session
        self.setEnabled(session is not None and session.master_node is not None)
        self.sender.clear()
        self.target.clear()
        self.message.clear()
        if not self.isEnabled():
            self.result.setText("Sending needs a session in master or simulation mode.")
            return
        config = session.config
        self.sender.addItem(f"{config.node_name(config.master)} (master)", config.master)
        if session.mode == "simulation":
            for slave in config.slaves:
                self.sender.addItem(f"{config.node_name(slave)} (simulated)", slave)
        for address in config.nodes:
            self.target.addItem(f"{config.node_name(address)} (0x{address:02X})", address)
        self.target.addItem("Broadcast (every other node)", 0xFF)
        if config.slaves:
            self.target.setCurrentIndex(1)
        for info in session.bridge.messages:
            kind = "state" if info.is_state else "event"
            self.message.addItem(f"{info.name}  ·  {kind}", info.name)
        self.result.setText("")

    def _info(self):
        name = self.message.currentData()
        return self.session.bridge.by_name.get(name) if self.session and name else None

    def _message_changed(self) -> None:
        while self.fields_form.rowCount():
            self.fields_form.removeRow(0)
        self.editors.clear()
        info = self._info()
        if info is None:
            return
        self.target.setEnabled(not info.is_state)
        self.priority.setEnabled(not info.is_state)
        self.button.setText("Publish state" if info.is_state else "Send event")
        layout = self.session.config.layouts.get(info.name)
        if layout is None or not layout.fields:
            self.raw.setChecked(True)
            self.raw.setEnabled(False)
            self.fields_box.setTitle("Fields (no layout defined: use raw data)")
            return
        self.raw.setEnabled(True)
        self.raw.setChecked(False)
        self.fields_box.setTitle(f"Fields of {info.name} ({layout.size} bytes)")
        for field in layout.fields:
            if field.enum and not field.bits:
                editor = QComboBox(self.fields_box)
                editor.setEditable(True)
                editor.addItems(list(field.enum.values()))
            else:
                editor = QLineEdit(self.fields_box)
                if field.bits:
                    editor.setPlaceholderText(", ".join(f"{b.name}=0" for b in field.bits))
                elif field.type in ("bytes",):
                    editor.setPlaceholderText(f"{field.length} bytes in hex")
                else:
                    editor.setPlaceholderText(field.type + (f" [{field.unit}]" if field.unit else ""))
            self.editors[field.name] = editor
            label = QLabel(field.name + (f" [{field.unit}]" if field.unit else ""), self.fields_box)
            self.fields_form.addRow(label, editor)
            label.show()
            editor.show()
        self._raw_toggled(False)

    def _raw_toggled(self, raw: bool) -> None:
        self.raw_edit.setEnabled(raw)
        self.fields_box.setEnabled(not raw)

    def _values(self, layout: Layout) -> Dict[str, Any]:
        values: Dict[str, Any] = {}
        for field in layout.fields:
            editor = self.editors[field.name]
            text = editor.currentText() if isinstance(editor, QComboBox) else editor.text()
            text = text.strip()
            if field.bits:
                values[field.name] = {k.strip(): int(v, 0) for k, v in
                                      (part.split("=") for part in text.split(",") if part.strip())}
            elif field.type in ("bytes", "ascii") or (field.enum and not _is_number(text)):
                values[field.name] = text
            else:
                values[field.name] = float(text) if field.type == "f32" or "." in text \
                    else int(text, 0)
        return values

    def _send(self) -> None:
        info = self._info()
        if info is None:
            return
        try:
            data = None
            fields = None
            if self.raw.isChecked():
                data = bytes.fromhex(self.raw_edit.text().replace(" ", ""))
            else:
                fields = self._values(self.session.config.layouts[info.name])
            sender = self.sender.currentData()
            if sender == self.session.config.master:
                if info.is_state:
                    self.session.publish(info.name, fields, data)
                else:
                    self.session.send(self.target.currentData(), info.name, fields, data,
                                      self.priority.currentText())
            else:
                slave = self.session.slave(sender)
                if info.is_state:
                    slave.publish(info.name, fields, data)
                else:
                    slave.send(self.target.currentData(), info.name, fields, data,
                               self.priority.currentText())
            self.result.setText(f"{info.name} queued at {self.session.now_ms:.0f} ms")
        except (ValueError, LayoutError, RuntimeError) as exc:
            self.result.setText(f"Not sent: {exc}")


def _is_number(text: str) -> bool:
    for convert in (float, lambda value: int(value, 0)):
        try:
            convert(text)
            return True
        except ValueError:
            pass
    return False


# ==============================================================================
# simulation
# ==============================================================================

class SimulationPanel(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.session = None
        self.grid = QGridLayout()
        self.noise = QDoubleSpinBox()
        self.noise.setRange(0.0, 20.0)
        self.noise.setDecimals(2)
        self.noise.setSuffix(" % of bytes")
        self.noise.valueChanged.connect(self._noise)
        self.note = _label("Simulated slaves run the Baton-C library; the bus moves one byte per "
                           "bit-time of the configured baud rate.", "muted")
        self.note.setWordWrap(True)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addWidget(self.note)
        noise = QFormLayout()
        noise.addRow("Line noise", self.noise)
        layout.addLayout(noise)
        layout.addLayout(self.grid)
        layout.addStretch(1)
        self.set_session(None)

    def set_session(self, session) -> None:
        self.session = session
        while self.grid.count():
            item = self.grid.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        enabled = session is not None and session.mode == "simulation"
        self.setEnabled(enabled)
        if not enabled:
            return
        for column, title in enumerate(("Slave", "Powered", "Answer delay")):
            self.grid.addWidget(_label(title, "muted"), 0, column)
        for row, address in enumerate(session.config.slaves, start=1):
            self.grid.addWidget(QLabel(f"{session.config.node_name(address)} (0x{address:02X})"),
                                row, 0)
            alive = QCheckBox()
            alive.setChecked(address not in session.config.simulation.dead)
            alive.toggled.connect(lambda on, a=address: self.session.slave(a).set_dead(not on))
            self.grid.addWidget(alive, row, 1)
            delay = QSpinBox()
            delay.setRange(0, 5000)
            delay.setSuffix(" ms")
            delay.setValue(int(session.config.simulation.tx_delay_ms.get(address, 0)))
            delay.valueChanged.connect(lambda v, a=address: self.session.slave(a).set_tx_delay(v))
            self.grid.addWidget(delay, row, 2)
        self.noise.blockSignals(True)
        self.noise.setValue(session.config.simulation.noise * 100.0)
        self.noise.blockSignals(False)

    def _noise(self, percent: float) -> None:
        if self.session is not None:
            self.session.set_noise(percent / 100.0)


# ==============================================================================
# bus definition editor
# ==============================================================================

FIELD_COLUMNS = ("Name", "Type", "Endian", "Scale", "Offset", "Unit", "Length", "Enum",
                 "Bits")


class LayoutEditor(QWidget):
    """Edit the message field layouts and the timing rules of the bus definition."""

    applied = Signal(dict)       # the new raw bus definition
    saved = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self.raw: Optional[Dict[str, Any]] = None
        self.messages: List[Dict[str, Any]] = []
        self.path: Optional[str] = None
        self.list = QListWidget()
        self.list.currentRowChanged.connect(self._load_message)
        self.table = QTableWidget(0, len(FIELD_COLUMNS))
        self.table.setHorizontalHeaderLabels(FIELD_COLUMNS)
        _table(self.table)
        self.table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.table.cellChanged.connect(self._fields_changed)
        self.size_label = _label("", "muted")
        add = QPushButton("Add field")
        remove = QPushButton("Remove")
        up = QPushButton("Up")
        down = QPushButton("Down")
        add.clicked.connect(self._add)
        remove.clicked.connect(self._remove)
        up.clicked.connect(lambda: self._move(-1))
        down.clicked.connect(lambda: self._move(1))
        self.tolerance = QDoubleSpinBox()
        self.tolerance.setRange(0, 1000)
        self.tolerance.setSuffix(" ms")
        self.rule_edits: Dict[str, QLineEdit] = {}
        rules = QFormLayout()
        rules.addRow("Tolerance", self.tolerance)
        for key, title in (("slot_max_ms", "Slot max [ms]"), ("ack_max_ms", "ACK max [ms]"),
                           ("cycle_max_ms", "Cycle max [ms]"), ("cycle_min_ms", "Cycle min [ms]"),
                           ("unanswered_polls", "Unanswered POLLs")):
            edit = QLineEdit()
            edit.setPlaceholderText("derived from the bus parameters")
            self.rule_edits[key] = edit
            rules.addRow(title, edit)
        rules_box = QGroupBox("Timing rules (empty = derived)")
        rules_box.setLayout(rules)
        self.apply_button = QPushButton("Apply to session")
        self.apply_button.setProperty("role", "primary")
        self.save_button = QPushButton("Save bus definition…")
        self.status = _label("", "muted")
        self.status.setWordWrap(True)
        self.apply_button.clicked.connect(self._apply)
        self.save_button.clicked.connect(self._save)

        buttons = QHBoxLayout()
        for b in (add, remove, up, down):
            buttons.addWidget(b)
        buttons.addStretch(1)
        buttons.addWidget(self.size_label)
        right = QVBoxLayout()
        right.addWidget(_label("FIELDS", "eyebrow"))
        right.addWidget(self.table, 1)
        right.addLayout(buttons)
        right.addWidget(_label("Types: u8 i8 u16 i16 u32 i32 f32 bytes ascii · Enum: 0=off, 1=on "
                               "· Bits: ready:0, level:4:3 (name:bit[:width])", "muted"))
        right_widget = QWidget()
        right_widget.setLayout(right)
        left = QVBoxLayout()
        left.addWidget(_label("MESSAGES", "eyebrow"))
        left.addWidget(self.list, 1)
        left.addWidget(rules_box)
        left_widget = QWidget()
        left_widget.setLayout(left)
        split = QSplitter()
        split.addWidget(left_widget)
        split.addWidget(right_widget)
        split.setSizes([300, 700])
        bottom = QHBoxLayout()
        bottom.addWidget(self.status, 1)
        bottom.addWidget(self.save_button)
        bottom.addWidget(self.apply_button)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addWidget(split, 1)
        layout.addLayout(bottom)
        self._loading = False

    def load(self, raw: Dict[str, Any], messages: List[Dict[str, Any]], path: Optional[str],
             can_apply: bool) -> None:
        self.raw = copy.deepcopy(raw)
        self.raw.setdefault("messages", {})
        self.messages = messages
        self.path = path
        self.apply_button.setEnabled(can_apply)
        self.list.clear()
        for m in messages:
            item = QListWidgetItem(f"{m['name']}  ·  0x{m['id']:02X}  ·  {m['kind']}  ·  "
                                   f"max {m['max_length']} B")
            item.setData(Qt.ItemDataRole.UserRole, m)
            self.list.addItem(item)
        timing = self.raw.get("timing", {})
        self.tolerance.setValue(float(timing.get("tolerance_ms", 3)))
        for key, edit in self.rule_edits.items():
            value = timing.get("rules", {}).get(key)
            edit.setText("" if value is None else str(value))
        if messages:
            self.list.setCurrentRow(0)
        self.status.setText(f"Bus definition: {path or '(attached session)'}")

    def _current(self) -> Optional[Dict[str, Any]]:
        item = self.list.currentItem()
        return item.data(Qt.ItemDataRole.UserRole) if item else None

    def _load_message(self) -> None:
        message = self._current()
        if message is None or self.raw is None:
            return
        self._loading = True
        fields = self.raw["messages"].get(message["name"], {}).get("fields", [])
        self.table.setRowCount(len(fields))
        for row, f in enumerate(fields):
            enum = ", ".join(f"{k}={v}" for k, v in f.get("enum", {}).items())
            bits = ", ".join(f"{b['name']}:{b['bit']}" + (f":{b['width']}" if b.get("width", 1) != 1
                                                          else "") for b in f.get("bits", []))
            values = [f["name"], f["type"], f.get("endian", "le"), str(f.get("scale", 1)),
                      str(f.get("offset", 0)), f.get("unit", ""), str(f.get("length", "")), enum,
                      bits]
            for column, value in enumerate(values):
                self.table.setItem(row, column, QTableWidgetItem(value))
        self._loading = False
        self._fields_changed()

    def _row_spec(self, row: int) -> Dict[str, Any]:
        def text(column: int) -> str:
            item = self.table.item(row, column)
            return item.text().strip() if item else ""

        spec: Dict[str, Any] = {"name": text(0), "type": text(1) or "u8"}
        if text(2) and text(2) != "le":
            spec["endian"] = text(2)
        if text(3) and float(text(3)) != 1.0:
            spec["scale"] = float(text(3))
        if text(4) and float(text(4)) != 0.0:
            spec["offset"] = float(text(4))
        if text(5):
            spec["unit"] = text(5)
        if text(6):
            spec["length"] = int(text(6))
        if text(7):
            spec["enum"] = {k.strip(): v.strip() for k, v in
                            (part.split("=", 1) for part in text(7).split(",") if part.strip())}
        if text(8):
            bits = []
            for part in text(8).split(","):
                pieces = [p.strip() for p in part.split(":")]
                bit = {"name": pieces[0], "bit": int(pieces[1])}
                if len(pieces) > 2:
                    bit["width"] = int(pieces[2])
                bits.append(bit)
            spec["bits"] = bits
        return spec

    def _fields_changed(self) -> None:
        message = self._current()
        if self._loading or message is None or self.raw is None:
            return
        try:
            fields = [self._row_spec(r) for r in range(self.table.rowCount())]
            layout = Layout.from_json(message["name"], {"fields": fields})
            if layout.size > message["max_length"]:
                raise LayoutError(f"fields take {layout.size} bytes, MAX_LEN is "
                                  f"{message['max_length']}")
            if fields:
                self.raw["messages"].setdefault(message["name"], {})["fields"] = fields
            else:
                self.raw["messages"].pop(message["name"], None)
            self.size_label.setText(f"{layout.size} of {message['max_length']} bytes")
            self.status.setText("Layout valid.")
        except (LayoutError, ValueError, IndexError) as exc:
            self.status.setText(f"Invalid: {exc}")

    def _add(self) -> None:
        row = self.table.rowCount()
        self._loading = True
        self.table.insertRow(row)
        for column, value in enumerate([f"field{row + 1}", "u8", "le", "1", "0", "", "", "", ""]):
            self.table.setItem(row, column, QTableWidgetItem(value))
        self._loading = False
        self._fields_changed()

    def _remove(self) -> None:
        row = self.table.currentRow()
        if row >= 0:
            self.table.removeRow(row)
            self._fields_changed()

    def _move(self, delta: int) -> None:
        row = self.table.currentRow()
        target = row + delta
        if row < 0 or not 0 <= target < self.table.rowCount():
            return
        self._loading = True
        for column in range(self.table.columnCount()):
            a, b = self.table.takeItem(row, column), self.table.takeItem(target, column)
            self.table.setItem(row, column, b)
            self.table.setItem(target, column, a)
        self._loading = False
        self.table.selectRow(target)
        self._fields_changed()

    def _collect_rules(self) -> None:
        timing = self.raw.setdefault("timing", {})
        timing["tolerance_ms"] = self.tolerance.value()
        rules = timing.setdefault("rules", {})
        for key, edit in self.rule_edits.items():
            text = edit.text().strip()
            rules[key] = None if not text else (int(text) if key == "unanswered_polls"
                                                 else float(text))

    def _apply(self) -> None:
        try:
            self._collect_rules()
        except ValueError as exc:
            self.status.setText(f"Invalid timing rule: {exc}")
            return
        self.applied.emit(copy.deepcopy(self.raw))

    def _save(self) -> None:
        from PySide6.QtWidgets import QFileDialog  # noqa: PLC0415
        try:
            self._collect_rules()
        except ValueError as exc:
            self.status.setText(f"Invalid timing rule: {exc}")
            return
        path, _ = QFileDialog.getSaveFileName(self, "Save bus definition", self.path or "",
                                              "Bus definition (*.json)")
        if not path:
            return
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            json.dump(self.raw, f, indent=2)
            f.write("\n")
        self.path = path
        self.status.setText(f"Saved {path}")
        self.saved.emit(path)


def card(title: str, value: str) -> QFrame:
    frame = QFrame()
    frame.setProperty("role", "card")
    layout = QVBoxLayout(frame)
    layout.setContentsMargins(12, 8, 12, 8)
    layout.addWidget(_label(title.upper(), "eyebrow"))
    value_label = _label(value, "title")
    value_label.setObjectName("value")
    layout.addWidget(value_label)
    return frame


__all__ = ["MessagesPanel", "ViolationsPanel", "FrameDetail", "TimingPanel", "StatsPanel",
           "SendPanel", "SimulationPanel", "LayoutEditor", "card"]
