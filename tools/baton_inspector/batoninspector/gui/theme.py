#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector GUI - SystemFromScratch dark and light themes.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Colors and the Qt style sheet of both themes (company palette)."""

from __future__ import annotations

from dataclasses import dataclass

from PySide6.QtGui import QColor, QFont, QFontDatabase


@dataclass(frozen=True)
class Theme:
    name: str
    bg: str
    bg_alt: str
    surface: str
    surface2: str
    border: str
    border_strong: str
    text: str
    muted: str
    accent: str
    accent_strong: str
    poll: str
    resp: str
    data: str
    ack: str
    error: str
    warning: str
    info: str
    lifeline: str

    def color(self, name: str) -> QColor:
        return QColor(getattr(self, name))


DARK = Theme("dark", bg="#050b16", bg_alt="#071122", surface="#0a1628", surface2="#0d1c33",
             border="#15294a", border_strong="#1e3a66", text="#e6edf7", muted="#9aabc4",
             accent="#2f8cff", accent_strong="#1a6fe0", poll="#f59e2b", resp="#3fcf8e",
             data="#5aa9ff", ack="#2cc7d6", error="#ff5d5d", warning="#f5c542", info="#8fa3c0",
             lifeline="#1e3a66")

LIGHT = Theme("light", bg="#f5f7fb", bg_alt="#eaf0f8", surface="#ffffff", surface2="#f2f5fa",
              border="#d5dde8", border_strong="#b7c4d6", text="#0b2545", muted="#5a6472",
              accent="#1a6fe0", accent_strong="#1e5aa8", poll="#e07a00", resp="#2e7d32",
              data="#1e5aa8", ack="#0f8b9a", error="#c62828", warning="#b8860b", info="#5a6472",
              lifeline="#c9d3e0")

FRAME_COLORS = {"POLL": "poll", "RESP": "resp", "DATA": "data", "ACK": "ack"}
SEVERITY_COLORS = {"error": "error", "warning": "warning", "info": "info"}


def ui_font(size: int = 10) -> QFont:
    families = set(QFontDatabase.families())
    for family in ("Inter", "Segoe UI", "Ubuntu", "Cantarell", "DejaVu Sans"):
        if family in families:
            return QFont(family, size)
    return QFont()


def mono_font(size: int = 9) -> QFont:
    families = set(QFontDatabase.families())
    for family in ("JetBrains Mono", "Cascadia Mono", "Consolas", "DejaVu Sans Mono", "Monospace"):
        if family in families:
            font = QFont(family, size)
            font.setStyleHint(QFont.StyleHint.Monospace)
            return font
    font = QFont("monospace", size)
    font.setStyleHint(QFont.StyleHint.Monospace)
    return font


def style_sheet(t: Theme) -> str:
    return f"""
QWidget {{ background: {t.bg}; color: {t.text}; }}
QMainWindow::separator {{ background: {t.border}; width: 1px; height: 1px; }}
QToolBar {{ background: {t.bg_alt}; border: none; border-bottom: 1px solid {t.border};
           padding: 6px; spacing: 8px; }}
QToolBar QLabel {{ background: transparent; color: {t.muted}; }}
QStatusBar {{ background: {t.bg_alt}; color: {t.muted}; border-top: 1px solid {t.border}; }}
QStatusBar QLabel {{ background: transparent; color: {t.muted}; padding: 0 8px; }}
QLabel {{ background: transparent; }}
QLabel[role="title"] {{ font-size: 15px; font-weight: 600; }}
QLabel[role="eyebrow"] {{ color: {t.accent}; font-size: 10px; font-weight: 700;
                         letter-spacing: 1px; }}
QLabel[role="muted"] {{ color: {t.muted}; }}
QLabel[role="badge"] {{ border-radius: 9px; padding: 2px 10px; font-weight: 600;
                       background: {t.surface2}; border: 1px solid {t.border_strong}; }}
QPushButton, QToolButton {{ background: {t.surface2}; color: {t.text};
    border: 1px solid {t.border_strong}; border-radius: 6px; padding: 5px 12px; }}
QPushButton:hover, QToolButton:hover {{ border-color: {t.accent}; }}
QPushButton:pressed, QToolButton:pressed {{ background: {t.surface}; }}
QPushButton:disabled, QToolButton:disabled {{ color: {t.muted}; border-color: {t.border}; }}
QPushButton[role="primary"] {{ background: {t.accent}; border-color: {t.accent}; color: white;
    font-weight: 600; }}
QPushButton[role="primary"]:hover {{ background: {t.accent_strong}; }}
QPushButton[role="danger"] {{ background: {t.error}; border-color: {t.error}; color: white;
    font-weight: 600; }}
QToolButton:checked, QPushButton:checked {{ background: {t.accent}; color: white;
    border-color: {t.accent}; }}
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QTextEdit {{
    background: {t.surface}; color: {t.text}; border: 1px solid {t.border_strong};
    border-radius: 6px; padding: 4px 6px; selection-background-color: {t.accent}; }}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
QPlainTextEdit:focus {{ border-color: {t.accent}; }}
QComboBox::drop-down {{ border: none; width: 18px; }}
QComboBox QAbstractItemView {{ background: {t.surface}; border: 1px solid {t.border_strong};
    selection-background-color: {t.accent}; }}
QCheckBox {{ background: transparent; spacing: 6px; }}
QCheckBox::indicator {{ width: 14px; height: 14px; border-radius: 3px;
    border: 1px solid {t.border_strong}; background: {t.surface}; }}
QCheckBox::indicator:checked {{ background: {t.accent}; border-color: {t.accent}; }}
QTabWidget::pane {{ border: 1px solid {t.border}; border-radius: 8px; top: -1px;
    background: {t.surface}; }}
QTabBar::tab {{ background: transparent; color: {t.muted}; padding: 7px 14px;
    border: none; border-bottom: 2px solid transparent; }}
QTabBar::tab:selected {{ color: {t.text}; border-bottom: 2px solid {t.accent}; }}
QTabBar::tab:hover {{ color: {t.text}; }}
QTableView, QTreeView, QListView {{ background: {t.surface}; alternate-background-color: {t.surface2};
    border: 1px solid {t.border}; border-radius: 6px; gridline-color: {t.border};
    selection-background-color: {t.accent_strong}; selection-color: white; }}
QHeaderView::section {{ background: {t.bg_alt}; color: {t.muted}; border: none;
    border-right: 1px solid {t.border}; border-bottom: 1px solid {t.border};
    padding: 5px 8px; font-weight: 600; }}
QTableCornerButton::section {{ background: {t.bg_alt}; border: none; }}
QScrollBar:vertical {{ background: {t.bg}; width: 11px; margin: 0; }}
QScrollBar:horizontal {{ background: {t.bg}; height: 11px; margin: 0; }}
QScrollBar::handle {{ background: {t.border_strong}; border-radius: 5px; min-height: 30px;
    min-width: 30px; }}
QScrollBar::handle:hover {{ background: {t.accent}; }}
QScrollBar::add-line, QScrollBar::sub-line {{ width: 0; height: 0; }}
QScrollBar::add-page, QScrollBar::sub-page {{ background: none; }}
QSplitter::handle {{ background: {t.border}; }}
QGroupBox {{ border: 1px solid {t.border}; border-radius: 8px; margin-top: 14px;
    padding: 10px 8px 8px 8px; background: {t.surface}; }}
QGroupBox::title {{ subcontrol-origin: margin; left: 10px; padding: 0 4px; color: {t.accent};
    font-weight: 600; }}
QFrame[role="card"] {{ background: {t.surface}; border: 1px solid {t.border}; border-radius: 10px; }}
QToolTip {{ background: {t.surface2}; color: {t.text}; border: 1px solid {t.border_strong};
    padding: 4px; }}
QMenu {{ background: {t.surface}; border: 1px solid {t.border_strong}; }}
QMenu::item:selected {{ background: {t.accent}; color: white; }}
QSlider::groove:horizontal {{ height: 4px; background: {t.border_strong}; border-radius: 2px; }}
QSlider::handle:horizontal {{ background: {t.accent}; width: 14px; margin: -6px 0;
    border-radius: 7px; }}
"""
