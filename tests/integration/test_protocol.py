#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Integration tests: Python codec, reference frames and a
#              cross-check against the C codec (compiled with gcc, ctypes).
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
#
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

from __future__ import annotations

import ctypes
import os
import random
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
LIBRARY_ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)

from baton import protocol as p  # noqa: E402

# Reference frames of docs/protocol.md
POLL = bytes.fromhex("A5 11 01 20 01 00 7C 44")
ACK = bytes.fromhex("A5 12 11 C3 00 45 5B")
RESP = bytes.fromhex("A5 01 11 60 02 80 00 C9 91")
DATA = bytes.fromhex("A5 12 11 A0 04 10 02 AB CD D8 BA")


class PythonCodecTest(unittest.TestCase):

    def test_crc_check_value(self):
        self.assertEqual(p.crc16(b"123456789"), 0x29B1)

    def test_reference_frames(self):
        self.assertEqual(p.build_frame(0x11, 0x01, p.ctrl(p.FT_POLL, True, 0), b"\x00"), POLL)
        self.assertEqual(p.build_frame(0x12, 0x11, p.ctrl(p.FT_ACK, False, 3)), ACK)
        self.assertEqual(p.build_frame(0x01, 0x11, p.ctrl(p.FT_RESP, True, 0), b"\x80\x00"), RESP)
        self.assertEqual(p.build_frame(0x12, 0x11, p.ctrl(p.FT_DATA, True, 0),
                                       p.tlv(0x10, b"\xAB\xCD")), DATA)

    def test_parse_reference_frames(self):
        frames = p.Parser().feed_all(POLL + ACK + RESP + DATA)
        self.assertEqual([f.frame_type for f in frames], [p.FT_POLL, p.FT_ACK, p.FT_RESP, p.FT_DATA])
        self.assertEqual(frames[3].payload, b"\x10\x02\xAB\xCD")
        self.assertEqual(p.describe(frames[0]), "POLL 01->11 first seq=0 [00]")

    def test_crc_error_is_rejected(self):
        parser = p.Parser()
        bad = bytearray(POLL)
        bad[-1] ^= 0x01
        self.assertEqual([parser.feed(b) for b in bad][-1], -1)
        self.assertEqual(len(parser.feed_all(POLL)), 1)

    def test_length_above_maximum_is_rejected_immediately(self):
        parser = p.Parser()
        results = [parser.feed(b) for b in (p.SYNC, 0x11, 0x01, 0x20, p.MAX_PAYLOAD + 1)]
        self.assertEqual(results[-1], -1)

    def test_garbage_before_and_between_frames(self):
        stream = b"\x00\x13\xFF" + POLL + b"\x42" + DATA
        self.assertEqual(len(p.Parser().feed_all(stream)), 2)

    def test_tlv_records(self):
        payload = p.tlv(0x10, b"\x01\x02") + p.tlv(0x12) + p.tlv(0x01, b"\xFF")
        records, complete = p.parse_tlv(payload)
        self.assertTrue(complete)
        self.assertEqual(records, [(0x10, b"\x01\x02"), (0x12, b""), (0x01, b"\xFF")])
        self.assertFalse(p.parse_tlv(payload + b"\x10")[1])
        self.assertFalse(p.parse_tlv(payload + b"\x10\x05\x00")[1])

    def test_ctrl_round_trip(self):
        for frame_type in range(4):
            for first in (False, True):
                for seq in range(16):
                    self.assertEqual(p.ctrl_fields(p.ctrl(frame_type, first, seq)),
                                     (frame_type, first, seq))

    def test_payload_limit(self):
        with self.assertRaises(ValueError):
            p.build_frame(1, 2, 3, bytes(p.MAX_PAYLOAD + 1))


@unittest.skipUnless(shutil.which("gcc"), "gcc not available")
class CCodecCrossTest(unittest.TestCase):
    """Builds the C codec as a shared library and checks that the Python codec
    and the C codec produce and accept identical frames."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        lib = os.path.join(cls.tmp, "baton.dll" if os.name == "nt" else "libbaton.so")
        flags = ["-fPIC"] if os.name != "nt" else []
        subprocess.check_call(["gcc", "-std=c11", "-shared", *flags, "-O1",
                               "-I", LIBRARY_ROOT,
                               "-I", os.path.join(LIBRARY_ROOT, "inc"),
                               "-I", os.path.join(LIBRARY_ROOT, "tests", "config"),
                               os.path.join(LIBRARY_ROOT, "src", "baton_protocol.c"), "-o", lib])
        cls.lib = ctypes.CDLL(lib)
        cls.lib.baton_encode.argtypes = [ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8,
                                         ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p,
                                         ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        cls.lib.baton_encode.restype = ctypes.c_int
        cls.lib.baton_parser_init.argtypes = [ctypes.c_void_p]
        cls.lib.baton_parser_feed.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
        cls.lib.baton_parser_feed.restype = ctypes.c_int
        cls.lib.baton_crc16.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        cls.lib.baton_crc16.restype = ctypes.c_uint16

    @classmethod
    def tearDownClass(cls):
        # Windows keeps the DLL locked while it is loaded; leave it behind there.
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _c_encode(self, dst, src, control, payload):
        out = ctypes.create_string_buffer(256)
        n = ctypes.c_size_t()
        rc = self.lib.baton_encode(dst, src, control, payload if payload else None, len(payload),
                                   out, 256, ctypes.byref(n))
        self.assertEqual(rc, 0)
        return out.raw[:n.value]

    def test_crc_matches(self):
        rng = random.Random(99)
        for _ in range(100):
            data = bytes(rng.randint(0, 255) for _ in range(rng.randint(0, 80)))
            self.assertEqual(self.lib.baton_crc16(data, len(data)), p.crc16(data))

    def test_encoders_match_and_parsers_agree(self):
        rng = random.Random(1234)
        parser = ctypes.create_string_buffer(512)  # >= sizeof(struct baton_parser)
        for _ in range(300):
            n = rng.randint(0, p.MAX_PAYLOAD)
            payload = bytes(rng.choice((p.SYNC, 0x00, 0xFF, rng.randint(0, 255))) for _ in range(n))
            dst, src = rng.randint(0, 0xFE), rng.randint(0, 0xFE)
            control = p.ctrl(rng.randint(0, 3), rng.random() < 0.5, rng.randint(0, 15))
            py = p.build_frame(dst, src, control, payload)
            self.assertEqual(py, self._c_encode(dst, src, control, payload), payload.hex())

            # the C parser accepts the Python frame
            self.lib.baton_parser_init(parser)
            results = [self.lib.baton_parser_feed(parser, b) for b in py]
            self.assertEqual(results[-1], 1)
            raw = parser.raw
            self.assertEqual((raw[0], raw[1], raw[2], raw[3]), (dst, src, control, n))
            self.assertEqual(raw[4:4 + n], payload)

            # corrupted frames are rejected by both parsers in the same way
            bad = bytearray(py)
            bad[rng.randint(1, len(bad) - 1)] ^= 1 << rng.randint(0, 7)
            self.lib.baton_parser_init(parser)
            c_results = [self.lib.baton_parser_feed(parser, b) for b in bad]
            py_parser = p.Parser()
            py_results = [py_parser.feed(b) for b in bad]
            self.assertEqual(c_results, py_results, bytes(bad).hex())


if __name__ == "__main__":
    unittest.main()
