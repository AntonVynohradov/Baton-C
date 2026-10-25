#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Description: Generates the SVG diagrams of the documentation
#              (docs/assets/images/*.svg). Run after changing a diagram:
#                  python tools/docs/diagrams.py
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Documentation diagrams of Baton-C. Every diagram is drawn from the code:
frame layout from inc/baton_defs.h, parser steps from src/baton_protocol.c,
layers from inc/, state machines from src/baton_node.c. Preview each one as PNG
(headless Chrome --screenshot) after a change."""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from svgkit import (BLUE, GREEN, GREY, LIGHT, LINE, NAVY, ORANGE, RED, TEAL, WHITE,  # noqa: E402
                    Svg, hero_scene)

OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                                    "docs", "assets", "images"))


def frame_structure() -> None:
    s = Svg(960, 420, "Baton frame structure")
    s.field_row(52, [
        ("SYNC", 20, 90, ["A5"], NAVY),
        ("DST", 120, 80, ["DST"], TEAL),
        ("SRC", 210, 80, ["SRC"], TEAL),
        ("CTRL", 300, 90, ["CTRL"], ORANGE),
        ("LEN", 400, 80, ["LEN"], BLUE),
        ("PAYLOAD", 490, 300, ["P0", "P1", "…", "Pn-1"], GREY),
        ("CRC16", 800, 140, ["HI", "LO"], NAVY),
    ])
    for x, w, size in ((20, 90, "1 byte"), (120, 80, "1 byte"), (210, 80, "1 byte"),
                       (300, 90, "1 byte"), (400, 80, "1 byte"), (490, 300, "0 … 64 bytes"),
                       (800, 140, "2 bytes, big endian")):
        s.text(x + w / 2, 118, size, size=12, fill=GREY)
    s.bracket(490, 790, 150, "LEN = number of payload bytes (≤ BATON_MAX_PAYLOAD)", BLUE)
    s.bracket(120, 790, 196, "CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over DST … last payload byte",
              NAVY)

    # control byte
    s.text(20, 250, "CTRL", size=14, weight="bold", anchor="start", fill=ORANGE)
    bits = [("TYPE", 2, ORANGE), ("FIRST", 1, TEAL), ("res.", 1, LINE), ("SEQ", 4, BLUE)]
    x = 90
    for name, count, color in bits:
        w = 60 * count
        s.rect(x, 232, w, 34, fill=WHITE, stroke=color, rx=4, sw=2)
        s.text(x + w / 2, 254, name, size=13, weight="bold",
               fill=color if color != LINE else GREY)
        x += w + 4
    s.text(90, 284, "bit 7 … 6", size=11.5, fill=GREY, anchor="start")
    s.text(214, 284, "5", size=11.5, fill=GREY, anchor="start")
    s.text(278, 284, "4", size=11.5, fill=GREY, anchor="start")
    s.text(342, 284, "3 … 0", size=11.5, fill=GREY, anchor="start")
    s.text(600, 246, "TYPE: 0 POLL · 1 RESP · 2 DATA · 3 ACK", size=13, fill=GREY, anchor="start")
    s.text(600, 268, "FIRST: set until the first acknowledgement", size=13, fill=GREY,
           anchor="start")

    s.text(20, 332, "Example: POLL from the master 0x01 to slave 0x11, nothing acknowledged",
           size=14, weight="bold", anchor="start")
    example = [("A5", 20, 90, NAVY), ("11", 120, 80, TEAL), ("01", 210, 80, TEAL),
               ("20", 300, 90, ORANGE), ("01", 400, 80, BLUE), ("00 (ACKB)", 490, 300, GREY),
               ("7C 44", 800, 140, NAVY)]
    for value, x, w, color in example:
        s.rect(x, 346, w, 36, fill=WHITE, stroke=color, rx=4)
        s.text(x + w / 2, 370, value, size=14, fill=color, mono=True)
    s.text(480, 406, "CTRL 0x20 = POLL, FIRST, SEQ 0 · reference frame of the unit and "
           "integration tests", size=12, fill=GREY)
    s.save(OUT + "/frame-structure.svg")


def payloads() -> None:
    s = Svg(960, 400, "Payload of each frame type")
    rows = [
        ("POLL", "master → slave", [("ACKB", 70, ORANGE), ("TLV messages for the slave", 520, GREY)]),
        ("RESP", "slave → master", [("ACKB", 70, ORANGE), ("NACK mask", 120, RED),
                                     ("TLV messages for the master", 400, GREY)]),
        ("DATA", "slave → slave", [("TLV messages for the peer", 590, GREY)]),
        ("ACK", "slave → slave", [("no payload: SEQ in CTRL is the acknowledged number", 590, LINE)]),
    ]
    y = 24
    for name, direction, fields in rows:
        s.text(20, y + 26, name, size=16, weight="bold", anchor="start", fill=NAVY)
        s.text(90, y + 26, direction, size=12.5, fill=GREY, anchor="start")
        x = 250
        for label, w, color in fields:
            s.rect(x, y, w, 40, fill=WHITE if color == LINE else LIGHT, stroke=color, rx=5, sw=2)
            s.text(x + w / 2, y + 25, label, size=13, weight="bold",
                   fill=GREY if color in (LINE, GREY) else color)
            x += w + 6
        y += 56
    # TLV
    s.text(20, 268, "TLV message", size=16, weight="bold", anchor="start", fill=NAVY)
    s.field_row(286, [("ID", 250, 70, ["ID"], TEAL), ("LEN", 326, 70, ["LEN"], BLUE),
                      ("DATA", 402, 240, ["D0", "…", "Dlen-1"], GREY),
                      ("next message", 648, 192, ["ID", "LEN", "…"], LINE)], height=40)
    s.text(250, 352, "ACKB: bit 7 valid, bits 3 … 0 acknowledged SEQ · NACK: one bit per node "
           "index that did not acknowledge DATA in the slot", size=12.5, fill=GREY, anchor="start")
    s.text(250, 374, "Every ID and LEN is checked against the message lists of baton_user_config.h",
           size=12.5, fill=GREY, anchor="start")
    s.save(OUT + "/payloads.svg")


def architecture() -> None:
    s = Svg(960, 640, "Baton-C architecture")
    s.rect(140, 20, 680, 90, fill="#FFF7EC", stroke=ORANGE)
    s.text(480, 52, "Application (your firmware or host tool)", size=17, weight="bold",
           fill=ORANGE)
    s.text(480, 80, "baton_node_send() · baton_node_publish() · on_message · on_drop · on_poll_done",
           size=13, fill=GREY, mono=True)
    s.rect(120, 140, 720, 380, fill=WHITE, stroke=BLUE, rx=14, dash=True)
    s.text(140, 166, "Baton-C library", size=14, weight="bold", fill=BLUE, anchor="start")
    # node
    s.rect(160, 180, 640, 80, fill=LIGHT, stroke=BLUE)
    s.text(480, 208, "baton_node — master/slave schedule, reliable events, states", size=16,
           weight="bold")
    s.text(480, 236, "slot FSM · channels (SEQ, FIRST) · TLV packing · duplicate filter · TTL",
           size=13, fill=GREY)
    # queue + link
    s.rect(160, 274, 200, 70, fill=LIGHT, stroke=TEAL)
    s.text(260, 302, "baton_queue", size=15, weight="bold", fill=TEAL)
    s.text(260, 326, "HIGH + NORMAL events", size=12.5, fill=GREY)
    s.rect(376, 274, 424, 70, fill=LIGHT, stroke=BLUE)
    s.text(588, 302, "baton_link — half-duplex link", size=15, weight="bold", fill=BLUE)
    s.text(588, 326, "RX ring · inter-byte timeout · bus gap · echo · TX timeout", size=12.5,
           fill=GREY)
    # protocol
    for x, title, sub in ((160, "Parser", "baton_parser_feed()"),
                          (378, "Encoder", "baton_encode()"),
                          (596, "CRC-16", "baton_crc16()")):
        s.rect(x, 360, 204, 70, fill=LIGHT, stroke=TEAL)
        s.text(x + 102, 388, title, size=15, weight="bold", fill=TEAL)
        s.text(x + 102, 412, sub, size=12.5, fill=GREY, mono=True)
    s.text(480, 452, "baton_protocol — codec: framing, LEN check, CRC", size=13.5, fill=TEAL)
    s.rect(160, 466, 640, 34, fill=WHITE, stroke=LINE)
    s.text(480, 488, "baton_defs · baton_config — frame layout, CTRL helpers, sizes, message lists",
           size=12.5, fill=GREY)
    s.rect(140, 550, 680, 70, fill=WHITE, stroke=GREY, dash=True)
    s.text(480, 578, "UART driver (application-owned)", size=15, weight="bold", fill=GREY)
    s.text(480, 602, "RX interrupt → baton_node_rx_isr() · write callback starts a non-blocking TX",
           size=13, fill=GREY)
    s.line(70, 580, 70, 70, color=TEAL, arrow="teal", width=2.4)
    s.line(70, 580, 138, 580, color=TEAL, width=2.4)
    s.line(70, 70, 138, 70, color=TEAL, arrow="teal", width=2.4)
    s.text(48, 330, "receive: bytes → messages", size=13, fill=TEAL, weight="bold", rotate=-90)
    s.line(890, 70, 890, 580, color=ORANGE, arrow="orange", width=2.4)
    s.line(822, 70, 890, 70, color=ORANGE, width=2.4)
    s.line(890, 580, 822, 580, color=ORANGE, arrow="orange", width=2.4)
    s.text(912, 330, "transmit: messages → bytes", size=13, fill=ORANGE, weight="bold", rotate=90)
    s.save(OUT + "/architecture.svg")


def data_flow() -> None:
    s = Svg(990, 330, "Receive and transmit data flow")
    xs = [20, 214, 408, 602, 796]
    rx = [("UART RX ISR", "every byte"), ("baton_node_rx_isr()", "store in the ring"),
          ("baton_node_process()", "main loop, every 1 ms"), ("baton_parser_feed()", "frame + CRC"),
          ("on_message", "your code")]
    s.text(20, 28, "Receive", size=14, weight="bold", fill=TEAL, anchor="start")
    for i, ((title, sub), x) in enumerate(zip(rx, xs)):
        s.rect(x, 40, 172, 64, fill=LIGHT, stroke=TEAL if i in (1, 2, 3) else LINE)
        s.text(x + 86, 68, title, size=12.5 if len(title) > 18 else 13.5, weight="bold",
               mono=title.endswith(")"))
        s.text(x + 86, 90, sub, size=12, fill=GREY)
        if i < 4:
            s.line(x + 172, 72, x + 192, 72, color=TEAL, arrow="teal", width=2)
    tx = [("baton_node_send()", "copy into a queue"), ("pack (TLV)", "in the slot of this node"),
          ("baton_encode()", "header + CRC"), ("write callback", "start the UART TX")]
    s.text(20, 158, "Transmit", size=14, weight="bold", fill=ORANGE, anchor="start")
    for i, ((title, sub), x) in enumerate(zip(tx, xs[:4])):
        s.rect(x, 170, 172, 64, fill=LIGHT, stroke=ORANGE if i in (0, 1, 2) else LINE)
        s.text(x + 86, 198, title, size=13.5, weight="bold", mono=title.endswith(")"))
        s.text(x + 86, 220, sub, size=12, fill=GREY)
        s.line(x + 172, 202, x + 192, 202, color=ORANGE, arrow="orange", width=2)
    s.rect(796, 170, 172, 64, fill=LIGHT, stroke=LINE)
    s.text(882, 198, "echo / TX done", size=13.5, weight="bold")
    s.text(882, 220, "ends the transmission", size=12, fill=GREY)
    s.text(20, 274, "Everything after the ring runs inside baton_node_process(), in the caller's "
           "context. The library", size=12.5, fill=GREY, anchor="start")
    s.text(20, 294, "never blocks: it transmits only in the slot the master gave to this node, "
           "after the bus was silent for tx_gap_ms.", size=12.5, fill=GREY, anchor="start")
    s.save(OUT + "/data-flow.svg")


def parser_flow() -> None:
    s = Svg(830, 760, "baton_parser_feed() algorithm")
    cx = 290

    def outcome(y, label, color):
        s.rect(580, y - 20, 230, 40, fill=WHITE, stroke=color, rx=20)
        s.text(695, y + 5, label, size=13, fill=color, weight="bold")

    def side(y, half_width, label, color, answer="no"):
        s.line(cx + half_width, y, 578, y, arrow="end")
        s.text(cx + half_width + 10, y - 8, answer, size=12.5, fill=GREY, anchor="start")
        outcome(y, label, color)

    def down(y1, y2, label="yes"):
        s.line(cx, y1, cx, y2, arrow="end")
        s.text(cx + 8, y1 + 16, label, size=12.5, fill=GREY, anchor="start")

    s.rect(cx - 60, 14, 120, 36, fill=NAVY, stroke=NAVY, rx=18)
    s.text(cx, 37, "byte in", size=14, fill=WHITE, weight="bold")
    s.line(cx, 50, cx, 70, arrow="end")
    s.diamond(cx, 112, 300, 80, ["waiting for SYNC:", "byte == 0xA5?"])
    side(112, 150, "return 0 (ignore the byte)", GREY)
    down(152, 184)
    s.rect(cx - 170, 186, 340, 52, fill=LIGHT, stroke=BLUE)
    s.text(cx, 208, "store DST, SRC, CTRL, LEN", size=13.5, weight="bold")
    s.text(cx, 228, "CRC register updated byte by byte", size=12, fill=GREY)
    s.line(cx, 238, cx, 266, arrow="end")
    s.diamond(cx, 308, 300, 80, ["LEN ≤", "BATON_MAX_PAYLOAD?"])
    side(308, 150, "back to SYNC, return ERROR", RED)
    down(348, 380)
    s.rect(cx - 170, 382, 340, 52, fill=LIGHT, stroke=BLUE)
    s.text(cx, 404, "store LEN payload bytes", size=13.5, weight="bold")
    s.text(cx, 424, "return 0 after each one", size=12, fill=GREY)
    s.line(cx, 434, cx, 462, arrow="end")
    s.rect(cx - 170, 464, 340, 44, fill=LIGHT, stroke=BLUE)
    s.text(cx, 491, "read CRC high and low byte", size=13.5, weight="bold")
    s.line(cx, 508, cx, 536, arrow="end")
    s.diamond(cx, 578, 300, 80, ["received CRC ==", "computed CRC?"])
    side(578, 150, "back to SYNC, return ERROR", RED)
    down(618, 650)
    s.rect(cx - 170, 652, 340, 52, fill="#EEF7EF", stroke=GREEN)
    s.text(cx, 674, "frame complete in parser->frame", size=13.5, weight="bold")
    s.text(cx, 694, "back to SYNC", size=12, fill=GREY)
    s.line(cx + 170, 678, 578, 678, arrow="end")
    outcome(678, "return 1 (frame ready)", GREEN)
    s.text(20, 740, "The link resets the parser when more than inter_byte_timeout_ms pass "
           "between two bytes of a frame.", size=12.5, fill=GREY, anchor="start")
    s.save(OUT + "/parser-flow.svg")


def bus_topology() -> None:
    s = Svg(980, 380, "Baton bus topology")
    s.rect(20, 120, 200, 130, fill=NAVY, stroke=NAVY, rx=12)
    s.text(120, 160, "Master 0x01", size=17, weight="bold", fill=WHITE)
    s.text(120, 184, "MCU or PC", size=13, fill="#C9D6EA")
    s.text(120, 206, "Baton-C + UART", size=13, fill="#C9D6EA")
    s.text(120, 228, "polls the slaves in turn", size=12, fill="#C9D6EA")
    s.line(220, 170, 940, 170, color=TEAL, width=3)
    s.text(232, 188, "single wire (UART half duplex) · GND", size=10.5, fill=GREY, anchor="start")
    for i, x in enumerate((340, 540, 740)):
        s.line(x + 60, 170, x + 60, 260, color=TEAL, width=2)
        s.rect(x, 260, 120, 90, fill=LIGHT, stroke=BLUE, rx=10)
        s.text(x + 60, 296, f"Slave 0x1{i + 1}", size=14, weight="bold")
        s.text(x + 60, 318, "MCU + Baton-C", size=12, fill=GREY)
        s.text(x + 60, 336, f"node index {i + 1}", size=11.5, fill=GREY)
    s.rect(250, 62, 200, 38, fill=WHITE, stroke=ORANGE, rx=6)
    s.text(350, 86, "POLL → 0x12 (baton)", size=13.5, fill=ORANGE, weight="bold")
    s.line(250, 130, 590, 130, color=ORANGE, arrow="orange", width=2.4)
    s.rect(470, 62, 220, 38, fill=WHITE, stroke=BLUE, rx=6)
    s.text(580, 86, "DATA 0x12 → 0x13, ACK", size=13.5, fill=BLUE, weight="bold")
    s.line(610, 140, 790, 140, color=BLUE, arrow="end", width=2.4)
    s.rect(710, 62, 240, 38, fill=WHITE, stroke=GREEN, rx=6)
    s.text(830, 86, "RESP 0x12 → master", size=13.5, fill=GREEN, weight="bold")
    s.line(600, 150, 240, 150, color=GREEN, arrow="green", width=2.4)
    s.text(490, 30, "Only the node holding the baton transmits: no arbitration, no collisions, "
           "a bounded cycle.", size=13.5, fill=GREY)
    s.save(OUT + "/bus-topology.svg")


def slot_sequence() -> None:
    s = Svg(960, 560, "One poll slot")
    lanes = [("Master 0x01", 110, NAVY), ("Slave 0x11 (polled)", 350, BLUE),
             ("Slave 0x12", 590, TEAL), ("Slave 0x13", 830, TEAL)]
    for name, x, color in lanes:
        s.rect(x - 95, 16, 190, 40, fill=color, stroke=color, rx=8)
        s.text(x, 41, name, size=13.5, fill=WHITE, weight="bold")
        s.line(x, 56, x, 540, color=LINE, width=1.6, dash=True)

    def msg(y, x1, x2, label, color, arrow, dash=False):
        s.line(x1, y, x2, y, color=color, arrow=arrow, width=2.2, dash=dash)
        s.text((x1 + x2) / 2, y - 8, label, size=12.5, fill=color, weight="bold")

    msg(92, 110, 348, "POLL seq=m [ACKB] [TLV for 0x11]", ORANGE, "orange")
    s.rect(352, 100, 8, 400, fill="#E7EEF8", stroke=BLUE, rx=2)
    s.text(370, 120, "slot: at most slot_time_ms", size=12, fill=BLUE, anchor="start")
    msg(160, 356, 588, "DATA seq=x [TLV]", BLUE, "end")
    msg(200, 588, 356, "ACK seq=x", GREEN, "green", dash=True)
    msg(260, 356, 828, "DATA seq=y [TLV]", BLUE, "end")
    s.text(840, 290, "no ACK (ack_timeout_ms)", size=12, fill=RED, anchor="end")
    msg(330, 356, 828, "DATA seq=y again (same content)", BLUE, "end")
    msg(370, 828, 356, "ACK seq=y", GREEN, "green", dash=True)
    msg(440, 356, 112, "RESP seq=r [ACKB=m] [NACK=0] [TLV for master]", GREEN, "green")
    s.text(110, 480, "next POLL → 0x12", size=12.5, fill=ORANGE, weight="bold")
    s.text(480, 532, "A DATA frame is started only if the ACK and a maximum-size RESP still fit "
           "into the slot.", size=12.5, fill=GREY)
    s.save(OUT + "/slot-sequence.svg")


def state_machines() -> None:
    s = Svg(960, 560, "Slave and master state machines")

    def state(x, y, label, color=BLUE, w=150):
        s.rect(x - w / 2, y - 22, w, 44, fill=LIGHT, stroke=color, rx=22, sw=2)
        s.text(x, y + 5, label, size=13.5, weight="bold", fill=color)

    s.text(20, 30, "Slave", size=16, weight="bold", fill=NAVY, anchor="start")
    state(130, 140, "IDLE", NAVY)
    state(420, 70, "DATA_TX")
    state(720, 70, "WAIT_ACK")
    state(420, 210, "RESP_TX", GREEN)
    state(130, 330, "ACK_TX", TEAL)
    # IDLE -> DATA_TX / RESP_TX
    s.line(205, 128, 343, 80, arrow="end")
    s.text(262, 86, "POLL, data to send", size=12, fill=GREY, anchor="middle")
    s.line(205, 152, 343, 202, arrow="end")
    s.text(300, 152, "POLL, nothing to send", size=12, fill=GREY, anchor="start")
    # DATA_TX <-> WAIT_ACK
    s.line(495, 64, 643, 64, arrow="end")
    s.text(569, 56, "sent", size=12, fill=GREY)
    s.path("M700,92 C680,130 470,130 440,94", arrow="end")
    s.text(570, 140, "ACK and more data / timeout: retry", size=12, fill=GREY)
    # WAIT_ACK -> RESP_TX
    s.path("M760,92 L760,210 L497,210", arrow="end")
    s.text(770, 160, "nothing left,", size=12, fill=GREY, anchor="start")
    s.text(770, 176, "no budget or no retry", size=12, fill=GREY, anchor="start")
    # RESP_TX -> IDLE
    s.path("M420,232 L420,270 L175,270 L175,164", arrow="end")
    s.text(300, 264, "sent", size=12, fill=GREY)
    # IDLE <-> ACK_TX
    s.line(90, 162, 90, 306, arrow="end")
    s.text(82, 240, "DATA to me", size=12, fill=GREY, anchor="end")
    s.line(130, 306, 130, 164, arrow="end")
    s.text(138, 300, "sent", size=12, fill=GREY, anchor="start")

    s.line(20, 380, 940, 380, color=LINE)
    s.text(20, 410, "Master", size=16, weight="bold", fill=NAVY, anchor="start")
    state(140, 480, "IDLE", NAVY)
    state(420, 480, "POLL_TX", ORANGE)
    state(700, 480, "WAIT_RESP", GREEN)
    s.line(215, 480, 343, 480, arrow="end")
    s.text(279, 470, "next slave", size=12, fill=GREY)
    s.line(495, 480, 623, 480, arrow="end")
    s.text(559, 470, "sent", size=12, fill=GREY)
    s.path("M700,502 C700,548 160,548 140,502", arrow="end")
    s.text(420, 524, "RESP or timeout: on_poll_done", size=12, fill=GREY)
    s.text(140, 446, "end of cycle: wait cycle_period_ms", size=12, fill=GREY)
    s.text(700, 446, "unreachable slaves: every 4th cycle", size=12, fill=GREY)
    s.save(OUT + "/state-machines.svg")


def main() -> None:
    os.makedirs(OUT, exist_ok=True)
    hero_scene(os.path.join(OUT, "hero-3d.svg"), chip_label="Baton-C",
               chip_sub="single-wire UART bus", transceiver="UART 1-wire",
               devices=["0x11", "0x12", "0x13"], device_style="module")
    frame_structure()
    payloads()
    architecture()
    data_flow()
    parser_flow()
    bus_topology()
    slot_sequence()
    state_machines()


if __name__ == "__main__":
    main()
