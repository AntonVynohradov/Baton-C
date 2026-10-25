#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Description: The animated isometric hero of the website, the README banner and
#              the PDF cover: the master hands the baton (POLL) to one slave at a
#              time in a fixed cycle, Baton Inspector watches the bus.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Baton-C hero scene, built on svgkit.Iso. Called by tools/docs/diagrams.py.

The baton leaves the master, climbs to one slave and stays there for its slot (the
slave's LED and halo turn amber), then returns as green reply packets; then the next
slave. A cycle ribbon above the slaves shows one slot per node; the last one is red
(over the slot limit), as Baton Inspector on the laptop reports it."""

from __future__ import annotations

import math

from svgkit import HERO_DEFS, HERO_STYLE, MONO, SANS, Iso, _write

AMBER = "#ffb547"
AMBER_GLOW = "#ffd58a"
GREEN = "#3fcf8e"
BLUE = "#2f8cff"

DEFS = HERO_DEFS + (
    '<linearGradient id="baton" x1="0" y1="0" x2="1" y2="0">'
    '<stop offset="0" stop-color="#ffcf7a"/><stop offset="0.5" stop-color="#ffb547"/>'
    '<stop offset="1" stop-color="#e58a10"/></linearGradient>'
    '<filter id="amberGlow" x="-100%" y="-100%" width="300%" height="300%">'
    '<feGaussianBlur stdDeviation="3.2" result="b"/>'
    '<feMerge><feMergeNode in="b"/><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>'
    '</filter>'
    '<radialGradient id="halo" cx="50%" cy="50%" r="50%">'
    '<stop offset="0" stop-color="#ffb547" stop-opacity="0.55"/>'
    '<stop offset="1" stop-color="#ffb547" stop-opacity="0"/></radialGradient>'
)
ARROW_DEFS = "".join(
    f'<marker id="ah-{n}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="5" markerHeight="5" '
    f'orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="{c}"/></marker>'
    for n, c in (("amber", AMBER), ("green", GREEN), ("red", "#ff5c6c")))

NAMES = ("0x11", "0x12", "0x13", "0x14")
BUS_Y, BUS_Z = 6.6, -0.35


# ------------------------------------------------------------------------------
# Scene pieces
# ------------------------------------------------------------------------------

def board(iso: Iso, chip_label: str, chip_sub: str) -> None:
    """The master: a board with the Baton-C chip, the UART and the bus connector."""
    p = iso.p
    iso.box(0, 10, 0, 7.2, -0.4, 0, "url(#boardTop)", "#0a1628", "#071122", "#1e3a66", 1.2)
    traces = []
    for i in range(6):
        y = 2.3 + i * 0.55
        traces.append([p(2.0, y), p(1.1, y), p(0.7, y - 0.4), p(0.25, y - 0.4)])
        x = 2.3 + i * 0.55
        traces.append([p(x, 2.0), p(x, 1.1), p(x + 0.35, 0.75), p(x + 0.35, 0.25)])
    for i in range(5):
        x = 2.4 + i * 0.6
        traces.append([p(x, 5.4), p(x, 6.2), p(x - 0.3, 6.5), p(x - 0.3, 6.95)])
    for trace in traces:
        iso.path(trace, "#1d5fbf", 1.3, ' opacity="0.7"')
    for trace in ([p(5.4, 3.6), p(6.4, 3.6), p(6.9, 4.1), p(7.4, 4.1)],
                  [p(8.7, 4.1), p(8.9, 4.1), p(8.9, 5.6), p(9.2, 5.9)]):
        iso.path(trace, "#1d5fbf", 1.6)
        iso.path(trace, "#5ea6ff", 1.6, ' class="flow"')
    iso.box(7.4, 8.7, 3.4, 4.8, 0, 0.45, "#14243c", "#0c182b", "#09121f", "#2a4870")
    iso.text(8.05, 4.1, 0.46, "UART 1-wire", 0.3, "#6b7d99", 600)
    iso.box(9.2, 10.0, 5.3, 6.5, 0, 0.55, "#c9d6ea", "#8aa0bf", "#6f86a6")
    for i in range(7):
        c = 2.25 + i * 0.45
        iso.box(c - 0.08, c + 0.08, 5.2, 5.45, 0, 0.12, "#8fa6c6", "#6b819f", "#5a6f8b")
        iso.box(5.2, 5.45, c - 0.08, c + 0.08, 0, 0.12, "#8fa6c6", "#6b819f", "#5a6f8b")
    iso.box(2.0, 5.2, 2.0, 5.2, 0, 0.75, "url(#chipTop)", "#0a1322", "#060d18", "#2f8cff", 1.0)
    iso.text(3.6, 3.55, 0.76, chip_label, 0.5, "#e6edf7", 800, 0.03)
    iso.text(3.6, 4.25, 0.76, chip_sub.upper(), 0.26, AMBER, 700, 0.08)
    iso.led(2.45, 2.45, 0.76, color=AMBER)


def slaves(iso: Iso, names, x0: float, step: float):
    xs = [x0 + i * step for i in range(len(names))]
    for x, name in zip(xs, names):
        iso.box(x, x + 2.4, 3.2, 5.4, -0.4, 1.5, "#1b2a40", "#131f31", "#0e1726", "#2a4870")
        iso.box(x + 0.4, x + 2.0, 3.6, 4.0, 1.5, 1.65, "#c9d6ea", "#8aa0bf", "#6f86a6")
        iso.text(x + 1.2, 5.4, 0.35, name, 0.42, "#9aabc4", 700, face="y")
    return xs


def bus(iso: Iso, xs):
    p = iso.p
    end_x = xs[-1] + 3.2
    line = [p(10.0, 5.9, -0.2), p(10.6, BUS_Y, BUS_Z), p(end_x, BUS_Y, BUS_Z)]
    for x in xs:
        drop = [p(x + 1.2, BUS_Y, BUS_Z), p(x + 1.2, 5.4, BUS_Z), p(x + 1.2, 5.4, 0.1)]
        iso.path(drop, "#0e2a4f", 5)
        iso.path(drop, BLUE, 1.6, ' opacity="0.8"')
    iso.path(line, "#0e2a4f", 7)
    iso.path(line, BLUE, 2.2, ' opacity="0.9"')
    iso.path(line, "#9cc8ff", 2.2, ' class="flow-bus"')


def relay(iso: Iso, xs, hold: float = 1.1, speed: float = 9.0) -> None:
    """The baton leaves the master, climbs to one slave, waits there (its slot), comes
    back as a green reply; then the next slave. Node LEDs and halos follow the baton."""
    p = iso.p
    start = p(10.6, BUS_Y, BUS_Z + 0.2)
    windows = []       # (t_from, t_to) per slave while it holds the baton
    back = []          # (t_from, t_to) return trips
    t = 0.0
    points = [start]
    times = [0.0]

    def go(pt):
        nonlocal t
        last = points[-1]
        t += math.dist(last, pt) / (speed * iso.scale)
        points.append(pt)
        times.append(t)

    for x in xs:
        tap = p(x + 1.2, BUS_Y, BUS_Z + 0.2)
        node = p(x + 1.2, 5.4, 0.25)
        go(tap)
        go(node)
        t_hold = t
        t += hold
        points.append(node)
        times.append(t)
        windows.append((t_hold, t))
        t_back = t
        go(tap)
        go(start)
        back.append((t_back, t))
    t += 0.4
    points.append(start)
    times.append(t)
    total = t

    # keyPoints are fractions of the path length; the path skips repeated points
    path_pts = [points[0]]
    for pt in points[1:]:
        if pt != path_pts[-1]:
            path_pts.append(pt)
    seg = [math.dist(a, b) for a, b in zip(path_pts, path_pts[1:])]
    length = sum(seg)
    # cumulative distance at each waypoint (walking the waypoint list in order)
    cum, acc, k = [], 0.0, 0
    for i, pt in enumerate(points):
        if i > 0 and pt != points[i - 1]:
            acc += seg[k]
            k += 1
        cum.append(acc / length)
    d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in path_pts)
    key_points = ";".join(f"{c:.4f}" for c in cum)
    key_times = ";".join(f"{tt / total:.4f}" for tt in times)
    motion = (f'<animateMotion dur="{total:.2f}s" repeatCount="indefinite" path="{d}" '
              f'keyPoints="{key_points}" keyTimes="{key_times}" calcMode="linear"/>')

    def visibility(intervals):
        kt, vals = ["0"], ["0"]
        for a, b in intervals:
            kt += [f"{a / total:.4f}", f"{b / total:.4f}"]
            vals += ["1", "0"]
        return (f'<animate attributeName="opacity" dur="{total:.2f}s" repeatCount="indefinite" '
                f'calcMode="discrete" keyTimes="{";".join(kt)}" values="{";".join(vals)}"/>')

    # the baton is out from the end of the previous reply to the end of the slot
    out_intervals = []
    prev = 0.0
    for (_, slot_end), (_, back_end) in zip(windows, back):
        out_intervals.append((prev, slot_end))
        prev = back_end
    # halos and LEDs of the slaves
    for x, (a, b) in zip(xs, windows):
        hx, hy = p(x + 1.2, 4.3, 1.7)
        iso.parts.append(f'<ellipse cx="{hx:.1f}" cy="{hy:.1f}" rx="46" ry="26" fill="url(#halo)" '
                         f'opacity="0">{visibility([(a, b)])}</ellipse>')
        lx, ly = p(x + 2.0, 5.4, 1.15)
        iso.parts.append(f'<circle cx="{lx:.1f}" cy="{ly:.1f}" r="2.6" fill="#2b3b55"/>')
        iso.parts.append(f'<circle cx="{lx:.1f}" cy="{ly:.1f}" r="3" fill="{AMBER}" opacity="0" '
                         f'filter="url(#amberGlow)">{visibility([(a, b)])}</circle>')
    # baton: outbound and while held
    iso.parts.append(
        f'<g filter="url(#amberGlow)"><g opacity="0">'
        f'<rect x="-11" y="-3.2" width="22" height="6.4" rx="3.2" fill="url(#baton)" '
        f'stroke="{AMBER_GLOW}" stroke-width="0.8" transform="rotate(30)"/>'
        f'{visibility(out_intervals)}</g>{motion}</g>')
    # reply: green packets on the way back
    cubes = "".join(f'<rect x="{-15 + k * 10}" y="-4" width="8" height="8" rx="1.5" '
                    f'fill="{GREEN}" stroke="#a6f0cb" stroke-width="0.8" transform="skewY(30)"/>'
                    for k in range(3))
    iso.parts.append(f'<g filter="url(#glow)"><g opacity="0">{cubes}{visibility(back)}</g>{motion}</g>')


# ------------------------------------------------------------------------------
# Baton Inspector screen (drawn in a 400 x 240 px canvas, mapped on a 3D face)
# ------------------------------------------------------------------------------

def inspector_screen(names):
    """The live sequence diagram of Baton Inspector: (width, height, SVG body)."""
    W, H = 400, 240
    s = [f'<rect width="{W}" height="{H}" rx="6" fill="#0a1322" stroke="#2a4870" stroke-width="2"/>',
         f'<rect width="{W}" height="22" rx="6" fill="#111e33"/>',
         f'<rect y="16" width="{W}" height="6" fill="#111e33"/>']
    for i, c in enumerate(("#ff5c6c", "#ffb547", GREEN)):
        s.append(f'<circle cx="{12 + i * 11}" cy="11" r="3.5" fill="{c}"/>')
    s.append(f'<text x="52" y="15" font-family="{SANS}" font-size="10" font-weight="700" '
             f'fill="#e6edf7">Baton Inspector</text>')
    s.append(f'<text x="{W - 10}" y="15" font-family="{MONO}" font-size="8.5" fill="{GREEN}" '
             f'text-anchor="end">● LIVE  9600 bit/s</text>')
    # sequence diagram (left)
    lanes = ["M"] + list(names)
    left, right = 18, 382
    gap = (right - left) / (len(lanes) - 1)
    lx = [left + i * gap for i in range(len(lanes))]
    for x, name in zip(lx, lanes):
        color = AMBER if name == "M" else "#9aabc4"
        s.append(f'<rect x="{x - 15}" y="30" width="30" height="14" rx="3" fill="#14243c" '
                 f'stroke="#2a4870"/>')
        s.append(f'<text x="{x}" y="40.5" font-family="{MONO}" font-size="8" fill="{color}" '
                 f'text-anchor="middle">{name}</text>')
        s.append(f'<line x1="{x}" y1="46" x2="{x}" y2="{H - 10}" stroke="#2a4870" '
                 f'stroke-dasharray="3 3"/>')
    y = 58
    rows = [(1, "POLL", "RESP 2 msg", "4.2 ms", False), (2, "POLL", "RESP", "1.9 ms", False),
            (3, "POLL", "RESP 1 msg", "3.6 ms", False), (4, "POLL", "timeout", "12 ms", True)]
    for lane, poll, resp, dt, bad in rows:
        x2 = lx[lane]
        s.append(f'<line x1="{lx[0]}" y1="{y}" x2="{x2 - 3}" y2="{y}" stroke="{AMBER}" '
                 f'stroke-width="1.6" marker-end="url(#ah-amber)"/>')
        s.append(f'<text x="{(lx[0] + x2) / 2}" y="{y - 3}" font-family="{MONO}" font-size="7" '
                 f'fill="{AMBER}" text-anchor="middle">{poll}</text>')
        rc = "#ff5c6c" if bad else GREEN
        dash = ' stroke-dasharray="3 2"' if bad else ""
        s.append(f'<line x1="{x2}" y1="{y + 14}" x2="{lx[0] + 3}" y2="{y + 14}" stroke="{rc}" '
                 f'stroke-width="1.6"{dash} marker-end="url(#ah-{"red" if bad else "green"})"/>')
        s.append(f'<text x="{(lx[0] + x2) / 2}" y="{y + 11}" font-family="{MONO}" font-size="7" '
                 f'fill="{rc}" text-anchor="middle">{resp}</text>')
        s.append(f'<rect x="{x2 + 4}" y="{y + 1}" width="34" height="11" rx="5.5" '
                 f'fill="{"#3a1420" if bad else "#10283f"}"/>')
        s.append(f'<text x="{x2 + 21}" y="{y + 9}" font-family="{MONO}" font-size="7" '
                 f'fill="{"#ff8a96" if bad else "#9cc8ff"}" text-anchor="middle">{dt}</text>')
        y += 44
    return W, H, "".join(s)


def screen_on_front(iso: Iso, x0, x1, y, z_top, content, margin=0.18) -> None:
    """Map a 2D screen canvas onto the front (y) face from x0..x1 starting at z_top."""
    W, H, body = content
    width = (x1 - x0) - 2 * margin
    k = width / W
    ox, oy = iso.p(x0 + margin, y, z_top - margin)
    c, s = iso.cos30 * iso.scale, 0.5 * iso.scale
    for corner in ((x0, z_top), (x1, z_top), (x0, z_top - H * k - 2 * margin),
                   (x1, z_top - H * k - 2 * margin)):
        iso.p(corner[0], y, corner[1])  # extend the viewBox
    iso.parts.append(f'<g transform="matrix({c:.3f} {s:.3f} 0 {iso.scale:.3f} {ox:.1f} {oy:.1f})">'
                     f'<g transform="scale({k:.5f})">{body}</g></g>')


def laptop(iso: Iso, x0, x1, y0, depth, content) -> None:
    """Open laptop: lid standing at y0 facing the viewer, keyboard base in front of it."""
    W, H, _ = content
    width = x1 - x0
    lid_h = (width - 0.36) * H / W + 0.36
    iso.box(x0, x1, y0 - 0.22, y0, -0.15, lid_h - 0.15, "#1b2a40", "#141f31", "#0e1726",
            "#2f8cff", 1.0)
    screen_on_front(iso, x0, x1, y0, lid_h - 0.15, content)
    iso.box(x0, x1, y0, y0 + depth, -0.4, -0.15, "#1b2a40", "#131f31", "#0e1726", "#2a4870")
    for r in range(4):
        for c in range(12):
            kx = x0 + 0.5 + c * (width - 1.0) / 12
            ky = y0 + 0.5 + r * 0.55
            iso.box(kx, kx + (width - 1.0) / 12 - 0.12, ky, ky + 0.4, -0.15, -0.1,
                    "#2a3b57", "#1b2a40", "#162236")
    iso.box(x0 + width * 0.35, x0 + width * 0.65, y0 + depth - 1.3, y0 + depth - 0.35,
            -0.15, -0.14, "#22324c", "#1b2a40", "#162236")


def probe(iso: Iso, from_pt, dongle_x, dongle_y, tap_x) -> None:
    """USB-UART dongle: a cable to the laptop and a tap on the bus."""
    p = iso.p
    cable = [from_pt, p(dongle_x - 0.2, dongle_y + 0.4, -0.35)]
    iso.path(cable, "#0e2a4f", 4)
    iso.path(cable, "#5ea6ff", 1.3)
    tap = [p(dongle_x + 0.5, dongle_y - 0.1, -0.35), p(tap_x, BUS_Y + 0.4, BUS_Z),
           p(tap_x, BUS_Y, BUS_Z)]
    iso.path(tap, "#0e2a4f", 4)
    iso.path(tap, "#5ea6ff", 1.3)
    iso.path(tap, "#cfe3ff", 1.3, ' class="flow"')
    iso.box(dongle_x - 0.5, dongle_x + 0.6, dongle_y - 0.3, dongle_y + 0.5, -0.4, 0.05,
            "#c9d6ea", "#8aa0bf", "#6f86a6")
    iso.text(dongle_x + 0.05, dongle_y + 0.1, 0.06, "USB-UART", 0.17, "#3b4a63", 800)


# ------------------------------------------------------------------------------
# The hero: slots on a timeline above the bus, the baton walking through them
# ------------------------------------------------------------------------------

def baton_hero(path: str) -> None:
    iso = Iso()
    p = iso.p
    board(iso, "Baton-C", "master · poll")
    xs = slaves(iso, NAMES, x0=12.4, step=3.6)
    # timeline ribbon floating above the slaves: one slot per slave
    z = 4.2
    y0, y1 = 3.6, 5.0
    iso.box(11.6, xs[-1] + 3.0, y0, y1, z - 0.12, z, "#0f1d33", "#0b1628", "#08111f", "#1e3a66",
            0.8)
    for i, x in enumerate(xs):
        fill = "#183a2c" if i != 3 else "#3a1420"
        edge = GREEN if i != 3 else "#ff5c6c"
        iso.box(x + 0.15, x + 3.25, y0 + 0.2, y1 - 0.2, z, z + 0.1, fill, "#0b1628", "#08111f",
                edge, 0.9)
        iso.text(x + 1.7, (y0 + y1) / 2 + 0.12, z + 0.11, f"slot {NAMES[i]}", 0.3,
                 "#c9d6ea" if i != 3 else "#ff8a96", 700)
        lead = [p(x + 1.2, 4.3, 1.66), p(x + 1.2, 4.3, z - 0.14)]
        iso.path(lead, "#2a4870", 1.0, ' stroke-dasharray="2 3"')
    iso.text(xs[-1] + 1.7, y1 + 0.55, z + 0.11, "12 ms > limit", 0.28, "#ff8a96", 700)
    iso.text(xs[0] + 0.3, y0 - 0.15, z + 0.11, "CYCLE", 0.28, AMBER, 800, 0.1)
    bus(iso, xs)
    probe(iso, iso.p(9.0, 12.4, -0.3), 10.4, 9.6, 11.6)
    relay(iso, xs)
    laptop(iso, 1.0, 9.0, 11.0, 4.0, inspector_screen(NAMES))
    label = ("Baton-C: the master hands the baton to one slave at a time in a fixed cycle; "
             "Baton Inspector checks every slot")
    _write(path, iso.svg(label, DEFS + ARROW_DEFS, HERO_STYLE, glow_at=(14, 6)))
