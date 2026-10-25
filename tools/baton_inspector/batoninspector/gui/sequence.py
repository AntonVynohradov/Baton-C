#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector GUI - live UML sequence diagram of the bus.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Sequence diagram.

One lifeline per node (master first, then the slaves in polling order). Time runs
downward at a fixed scale (pixels per millisecond, Ctrl + wheel to zoom), so the
picture shows real timing: each frame is an arrow from its sender's lifeline at
its start time to the receiver's lifeline at its end time, labeled with its type,
sequence number, duration and the gap since the previous frame. The slot of the
polled slave is drawn as an activation bar. Frames involved in a violation are
drawn in the color of its severity; violations, receive errors and test marks are
shown on the time axis.
"""

from __future__ import annotations

import bisect
import math
from typing import Any, Dict, List, Optional, Tuple

from PySide6.QtCore import QPointF, QRectF, Qt, Signal
from PySide6.QtGui import QColor, QFontMetrics, QPainter, QPainterPath, QPen
from PySide6.QtWidgets import QAbstractScrollArea, QToolTip

from .store import RecordStore
from .theme import FRAME_COLORS, SEVERITY_COLORS, Theme, mono_font, ui_font

HEADER = 62
AXIS = 104
MIN_COLUMN = 190
MARGIN_MS = 40.0


class SequenceView(QAbstractScrollArea):
    """Live sequence diagram over a RecordStore."""

    frame_selected = Signal(int)

    def __init__(self, store: RecordStore, theme: Theme) -> None:
        super().__init__()
        self.store = store
        self.theme = theme
        self.px_per_ms = 3.0
        self._label_slots: Dict[int, float] = {}
        self.follow = True
        self.selected: Optional[int] = None
        self._spans: List[Tuple[int, float, float]] = []      # slot activation bars
        self._open_poll: Optional[Dict[str, Any]] = None
        self._starts: List[float] = []
        self._font = ui_font(9)
        self._small = ui_font(8)
        self._mono = mono_font(8)
        self._bold = ui_font(9)
        self._bold.setBold(True)
        self.setMouseTracking(True)
        self.viewport().setMouseTracking(True)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        self.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOn)
        self.verticalScrollBar().valueChanged.connect(self._scrolled)
        store.reset.connect(self._rebuild)
        store.added.connect(self._added)
        store.other_added.connect(self.viewport().update)

    # ---- data -----------------------------------------------------------------

    def set_theme(self, theme: Theme) -> None:
        self.theme = theme
        self.viewport().update()

    def _rebuild(self) -> None:
        self._spans.clear()
        self._open_poll = None
        self._starts = []
        self.selected = None
        self._added(0, len(self.store.frames))

    def _added(self, first: int, count: int) -> None:
        frames = self.store.frames
        for frame in frames[first:first + count]:
            self._starts.append(frame["t_start_ms"])
            if frame["frame"] == "POLL":
                self._open_poll = frame
            elif frame["frame"] == "RESP" and self._open_poll is not None \
                    and self._open_poll["dst"] == frame["src"]:
                self._spans.append((frame["src"], self._open_poll["t_end_ms"], frame["t_end_ms"]))
                self._open_poll = None
        self._update_range()
        if self.follow:
            self.verticalScrollBar().setValue(self.verticalScrollBar().maximum())
        self.viewport().update()

    def _origin(self) -> float:
        return self.store.frames[0]["t_start_ms"] - MARGIN_MS if self.store.frames else 0.0

    def _end(self) -> float:
        end = self.store.frames[-1]["t_end_ms"] if self.store.frames else 0.0
        return max(end, self.store.now_ms) + MARGIN_MS

    def _update_range(self) -> None:
        height = int((self._end() - self._origin()) * self.px_per_ms) + HEADER
        bar = self.verticalScrollBar()
        bar.setPageStep(max(1, self.viewport().height()))
        bar.setSingleStep(40)
        bar.setRange(0, max(0, height - self.viewport().height()))
        nodes = len(self.store.info.nodes) if self.store.info else 1
        width = AXIS + nodes * MIN_COLUMN
        hbar = self.horizontalScrollBar()
        hbar.setPageStep(max(1, self.viewport().width()))
        hbar.setRange(0, max(0, width - self.viewport().width()))

    def resizeEvent(self, event) -> None:  # noqa: N802 - Qt name
        super().resizeEvent(event)
        self._update_range()

    def _scrolled(self, value: int) -> None:
        self.follow = value >= self.verticalScrollBar().maximum() - 2
        self.viewport().update()

    # ---- geometry ---------------------------------------------------------------

    def _columns(self) -> Dict[int, float]:
        if self.store.info is None:
            return {}
        nodes = self.store.info.nodes
        width = max(self.viewport().width(), AXIS + len(nodes) * MIN_COLUMN)
        step = (width - AXIS) / len(nodes)
        x0 = AXIS + step / 2 - self.horizontalScrollBar().value()
        return {address: x0 + i * step for i, address in enumerate(nodes)}

    def _y(self, t_ms: float) -> float:
        return HEADER + (t_ms - self._origin()) * self.px_per_ms - self.verticalScrollBar().value()

    def _t(self, y: float) -> float:
        return (y - HEADER + self.verticalScrollBar().value()) / self.px_per_ms + self._origin()

    def _visible(self) -> Tuple[int, int]:
        top = self._t(HEADER) - 200.0
        bottom = self._t(self.viewport().height()) + 1.0
        first = bisect.bisect_left(self._starts, top)
        last = bisect.bisect_right(self._starts, bottom)
        return max(0, first - 1), min(len(self.store.frames), last)

    def set_zoom(self, px_per_ms: float, anchor_y: Optional[float] = None) -> None:
        anchor_y = self.viewport().height() / 2 if anchor_y is None else anchor_y
        t_anchor = self._t(anchor_y)
        self.px_per_ms = min(40.0, max(0.05, px_per_ms))
        self._update_range()
        value = HEADER + (t_anchor - self._origin()) * self.px_per_ms - anchor_y
        if not self.follow:
            self.verticalScrollBar().setValue(int(value))
        self.viewport().update()

    def scroll_to_time(self, t_ms: float) -> None:
        self.follow = False
        value = (t_ms - self._origin()) * self.px_per_ms - self.viewport().height() / 3
        self.verticalScrollBar().setValue(int(max(0, value)))

    def select_frame(self, index: int) -> None:
        frame = self.store.frame_by_index(index)
        if frame is not None:
            self.selected = index
            self.scroll_to_time(frame["t_start_ms"])
            self.viewport().update()

    def jump_to_end(self) -> None:
        self.follow = True
        self.verticalScrollBar().setValue(self.verticalScrollBar().maximum())

    # ---- input --------------------------------------------------------------------

    def wheelEvent(self, event) -> None:  # noqa: N802
        if event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            factor = 1.25 if event.angleDelta().y() > 0 else 0.8
            self.set_zoom(self.px_per_ms * factor, event.position().y())
            event.accept()
        else:
            super().wheelEvent(event)

    def _hit(self, pos: QPointF) -> Optional[Dict[str, Any]]:
        columns = self._columns()
        first, last = self._visible()
        best, best_distance = None, 7.0
        for frame in self.store.frames[first:last]:
            if frame["src"] not in columns or frame["dst"] not in columns:
                continue
            a = QPointF(columns[frame["src"]], self._y(frame["t_start_ms"]))
            b = QPointF(columns[frame["dst"]], self._y(frame["t_end_ms"]))
            distance = _segment_distance(pos, a, b)
            if distance < best_distance:
                best, best_distance = frame, distance
        return best

    def mousePressEvent(self, event) -> None:  # noqa: N802
        frame = self._hit(event.position())
        if frame is not None:
            self.selected = frame["index"]
            self.frame_selected.emit(frame["index"])
            self.viewport().update()

    def mouseMoveEvent(self, event) -> None:  # noqa: N802
        pos = event.position()
        frame = self._hit(pos)
        if frame is not None:
            QToolTip.showText(event.globalPosition().toPoint(), self._frame_tooltip(frame), self)
            return
        if pos.x() < AXIS:
            texts = [f"{v['severity'].upper()}: {v['text']}" for v in self.store.violations
                     if abs(self._y(v["t_ms"]) - pos.y()) < 6]
            texts += [f"{e['kind'].upper()} error: {e['raw']}" for e in self.store.errors
                      if abs(self._y(e["t_ms"]) - pos.y()) < 6]
            if texts:
                QToolTip.showText(event.globalPosition().toPoint(), "\n".join(texts[:8]), self)
                return
        QToolTip.hideText()

    def _frame_tooltip(self, frame: Dict[str, Any]) -> str:
        info = self.store.info
        lines = [f"<b>{frame['frame']}</b> {info.label(frame['src'])} → {info.label(frame['dst'])}",
                 f"start {frame['t_start_ms']:.2f} ms · end {frame['t_end_ms']:.2f} ms · "
                 f"{frame['t_end_ms'] - frame['t_start_ms']:.2f} ms",
                 f"seq {frame['seq']}{' · FIRST' if frame['first'] else ''}"
                 f"{' · retransmission' if frame['retransmission'] else ''}"]
        for message in frame["messages"]:
            lines.append(f"• {message['name']}: {message['text']}")
        lines.append(f"<span style='font-family:monospace'>{frame['raw']}</span>")
        return "<br>".join(lines)

    # ---- painting ----------------------------------------------------------------------

    def paintEvent(self, event) -> None:  # noqa: N802
        t = self.theme
        painter = QPainter(self.viewport())
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        rect = self.viewport().rect()
        painter.fillRect(rect, t.color("surface"))
        if self.store.info is None:
            painter.setPen(t.color("muted"))
            painter.setFont(ui_font(11))
            painter.drawText(rect, Qt.AlignmentFlag.AlignCenter,
                             "Start a session or attach to a test session")
            return
        columns = self._columns()
        top_t, bottom_t = self._t(HEADER), self._t(rect.height())
        self._paint_grid(painter, rect, top_t, bottom_t)
        # lifelines
        painter.setPen(QPen(t.color("lifeline"), 1.2, Qt.PenStyle.DashLine))
        for x in columns.values():
            painter.drawLine(QPointF(x, HEADER), QPointF(x, rect.height()))
        self._paint_spans(painter, columns, top_t, bottom_t)
        self._paint_marks(painter, rect, top_t, bottom_t)
        first, last = self._visible()
        previous_end = None
        self._label_slots = {}
        if first > 0:
            previous_end = self.store.frames[first - 1]["t_end_ms"]
        for frame in self.store.frames[first:last]:
            self._paint_frame(painter, frame, columns, previous_end)
            previous_end = frame["t_end_ms"]
        self._paint_axis_events(painter, top_t, bottom_t)
        self._paint_header(painter, rect, columns)

    def _paint_grid(self, painter: QPainter, rect, top_t: float, bottom_t: float) -> None:
        t = self.theme
        painter.fillRect(QRectF(0, HEADER, AXIS - 8, rect.height()), t.color("bg_alt"))
        # a tick every 10/50/100/500/1000 ms so that ticks are at least 60 px apart
        step = next(s for s in (1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 60000)
                    if s * self.px_per_ms >= 60)
        painter.setFont(self._mono)
        start = math.floor(top_t / step) * step
        tick = start
        grid = QColor(t.border)
        while tick <= bottom_t:
            y = self._y(tick)
            if y >= HEADER:
                painter.setPen(QPen(grid, 1))
                painter.drawLine(QPointF(AXIS - 8, y), QPointF(rect.width(), y))
                painter.setPen(t.color("muted"))
                painter.drawText(QRectF(4, y - 8, AXIS - 18, 16),
                                 Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter,
                                 _format_time(tick))
            tick += step

    def _paint_spans(self, painter, columns, top_t, bottom_t) -> None:
        color = QColor(self.theme.accent)
        color.setAlpha(60)
        painter.setPen(QPen(QColor(self.theme.accent), 1))
        painter.setBrush(color)
        for slave, t0, t1 in self._spans:
            if t1 < top_t or t0 > bottom_t or slave not in columns:
                continue
            x = columns[slave]
            painter.drawRect(QRectF(x - 5, self._y(t0), 10, max(2.0, (t1 - t0) * self.px_per_ms)))
        painter.setBrush(Qt.BrushStyle.NoBrush)

    def _paint_marks(self, painter, rect, top_t, bottom_t) -> None:
        painter.setFont(self._bold)
        for mark in self.store.marks:
            if not top_t <= mark["t_ms"] <= bottom_t:
                continue
            y = self._y(mark["t_ms"])
            painter.setPen(QPen(self.theme.color("accent"), 1.5, Qt.PenStyle.DashDotLine))
            painter.drawLine(QPointF(AXIS - 8, y), QPointF(rect.width(), y))
            painter.drawText(QPointF(AXIS + 4, y - 4), f"◆ {mark.get('text') or 'mark'}")

    def _paint_axis_events(self, painter, top_t, bottom_t) -> None:
        for v in self.store.violations:
            if top_t <= v["t_ms"] <= bottom_t:
                color = self.theme.color(SEVERITY_COLORS[v["severity"]])
                y = self._y(v["t_ms"])
                path = QPainterPath()
                path.moveTo(AXIS - 22, y - 6)
                path.lineTo(AXIS - 10, y)
                path.lineTo(AXIS - 22, y + 6)
                path.closeSubpath()
                painter.fillPath(path, color)
        painter.setPen(QPen(self.theme.color("error"), 2))
        for e in self.store.errors:
            if top_t <= e["t_ms"] <= bottom_t:
                y = self._y(e["t_ms"])
                painter.drawLine(QPointF(8, y - 4), QPointF(16, y + 4))
                painter.drawLine(QPointF(8, y + 4), QPointF(16, y - 4))

    def _paint_frame(self, painter: QPainter, frame, columns, previous_end) -> None:
        if frame["src"] not in columns or frame["dst"] not in columns:
            return
        t = self.theme
        severity = self.store.violated_frames.get(frame["index"])
        color = t.color(SEVERITY_COLORS[severity]) if severity in ("error", "warning") else \
            t.color(FRAME_COLORS[frame["frame"]])
        x1, x2 = columns[frame["src"]], columns[frame["dst"]]
        y1, y2 = self._y(frame["t_start_ms"]), self._y(frame["t_end_ms"])
        selected = frame["index"] == self.selected
        width = 3.2 if selected else (2.2 if frame["own"] or severity else 1.6)
        pen = QPen(color, width)
        if frame["retransmission"]:
            pen.setStyle(Qt.PenStyle.DashLine)
        painter.setPen(pen)
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2))
        # arrow head
        angle = math.atan2(y2 - y1, x2 - x1)
        head = QPainterPath()
        head.moveTo(x2, y2)
        head.lineTo(x2 - 11 * math.cos(angle - 0.4), y2 - 11 * math.sin(angle - 0.4))
        head.lineTo(x2 - 11 * math.cos(angle + 0.4), y2 - 11 * math.sin(angle + 0.4))
        head.closeSubpath()
        painter.fillPath(head, color)
        # start and end dots
        painter.setBrush(color)
        painter.drawEllipse(QPointF(x1, y1), 2.5, 2.5)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        # label above the arrow, near its start
        duration = frame["t_end_ms"] - frame["t_start_ms"]
        label = f"{frame['frame']} seq {frame['seq']}"
        if frame["first"]:
            label += " F"
        label += f" · {duration:.1f} ms"
        if previous_end is not None:
            label += f" · gap {frame['t_start_ms'] - previous_end:.1f}"
        names = ", ".join(m["name"] for m in frame["messages"][:3])
        if len(frame["messages"]) > 3:
            names += f" +{len(frame['messages']) - 3}"
        left = min(x1, x2)
        span = abs(x2 - x1)
        # the label of a frame that starts too close to the previous one of the same column
        # pair is left out (zoom in to see it; the tooltip always shows everything)
        key = int(left)
        label_y = min(y1, y2) - 4
        if label_y - self._label_slots.get(key, -1e9) < 14 and not selected:
            return
        self._label_slots[key] = label_y
        painter.setFont(self._bold if frame["own"] else self._font)
        painter.setPen(color)
        metrics = QFontMetrics(painter.font())
        room = int(span - 16)
        text = metrics.elidedText(label, Qt.TextElideMode.ElideRight, room)
        text_x = left + 8
        painter.drawText(QPointF(text_x, label_y), text)
        used = metrics.horizontalAdvance(text) + 10
        if names and room - used > 40:
            # the message names follow the frame label on the same line, in a quieter color
            painter.setFont(self._small)
            painter.setPen(t.color("muted") if not selected else color)
            small = QFontMetrics(self._small).elidedText(names, Qt.TextElideMode.ElideRight,
                                                         room - used)
            painter.drawText(QPointF(text_x + used, label_y), small)

    def _paint_header(self, painter: QPainter, rect, columns) -> None:
        t = self.theme
        painter.fillRect(QRectF(0, 0, rect.width(), HEADER), t.color("bg_alt"))
        painter.setPen(QPen(t.color("border"), 1))
        painter.drawLine(QPointF(0, HEADER), QPointF(rect.width(), HEADER))
        info = self.store.info
        for address, x in columns.items():
            master = address == info.master
            box = QRectF(x - 80, 10, 160, 42)
            painter.setPen(QPen(t.color("accent") if master else t.color("border_strong"), 1.4))
            painter.setBrush(t.color("surface2"))
            painter.drawRoundedRect(box, 8, 8)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.setFont(self._bold)
            painter.setPen(t.color("text"))
            name = QFontMetrics(self._bold).elidedText(info.name(address),
                                                       Qt.TextElideMode.ElideRight, 150)
            painter.drawText(QRectF(box.x(), box.y() + 4, box.width(), 18),
                             Qt.AlignmentFlag.AlignCenter, name)
            painter.setFont(self._mono)
            painter.setPen(t.color("muted"))
            role = "master" if master else f"slave {info.slaves.index(address) + 1}"
            painter.drawText(QRectF(box.x(), box.y() + 22, box.width(), 16),
                             Qt.AlignmentFlag.AlignCenter, f"0x{address:02X} · {role}")
        painter.setFont(self._small)
        painter.setPen(t.color("muted"))
        painter.drawText(QRectF(6, 12, AXIS - 14, 40), Qt.AlignmentFlag.AlignRight,
                         f"time (ms)\n{self.px_per_ms:.3g} px/ms")


def _format_time(t_ms: float) -> str:
    if abs(t_ms) >= 100000:
        return f"{t_ms / 1000:.1f} s"
    return f"{t_ms:.0f}"


def _segment_distance(p: QPointF, a: QPointF, b: QPointF) -> float:
    dx, dy = b.x() - a.x(), b.y() - a.y()
    length = dx * dx + dy * dy
    if length == 0:
        return math.hypot(p.x() - a.x(), p.y() - a.y())
    k = max(0.0, min(1.0, ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / length))
    return math.hypot(p.x() - (a.x() + k * dx), p.y() - (a.y() + k * dy))

