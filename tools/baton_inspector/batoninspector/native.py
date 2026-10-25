#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - builds the native bridge for a system
#              configuration and binds it with ctypes.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Access to the Baton-C library from Python.

The bridge (``native/inspector_bridge.c``) is the real library compiled with the
``baton_user_config.h`` of the system under test. It is built once per
configuration (CMake) and cached in ``tools/baton_inspector/build/<hash>``; the
hash covers the configuration file, the library sources and the bridge, so a
change of any of them triggers a rebuild. Nothing here re-implements the protocol.
"""

from __future__ import annotations

import ctypes
import hashlib
import os
import shutil
import subprocess
import sys
import threading
from dataclasses import dataclass
from typing import Callable, Dict, List, Optional, Tuple

TOOL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_ROOT = os.path.abspath(os.path.join(TOOL_DIR, "..", ".."))
NATIVE_DIR = os.path.join(TOOL_DIR, "native")
CACHE_DIR = os.path.join(TOOL_DIR, "build")
INTERFACE_VERSION = 1

if os.name == "nt":
    LIB_FILE = "baton_inspector_native.dll"
elif sys.platform == "darwin":
    LIB_FILE = "baton_inspector_native.dylib"
else:
    LIB_FILE = "baton_inspector_native.so"

ROLE_MASTER = 0
ROLE_SLAVE = 1
PRIO_NORMAL = 0
PRIO_HIGH = 1

STATUS_TEXT = {0: "OK", -1: "invalid parameter", -2: "node not initialized",
               -3: "unknown message id or wrong class", -4: "data too long",
               -5: "unknown or own address", -6: "event queue full"}

STAT_NAMES = ("tx_frames", "rx_frames", "crc_errors", "rx_timeouts", "rx_overflows",
              "collisions", "tx_timeouts", "ack_timeouts", "retries", "resp_timeouts",
              "duplicates", "bad_messages", "events_dropped", "queue_full",
              "queue_high_used", "queue_normal_used")

WRITE_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8),
                            ctypes.c_uint32)
MESSAGE_CB = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint8,
                              ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8)
DROP_CB = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint32)
POLL_CB = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_uint8, ctypes.c_int, ctypes.c_uint32)


class NativeError(RuntimeError):
    """The bridge cannot be built or loaded."""


@dataclass(frozen=True)
class MessageInfo:
    """One entry of the message lists compiled into the bridge."""

    name: str
    msg_id: int
    max_length: int
    is_state: bool


@dataclass(frozen=True)
class BuildConfig:
    """Build-time sizes of the bridge (BATON_* macros)."""

    max_nodes: int
    max_payload: int
    max_msgs_per_frame: int
    queue_high: int
    queue_normal: int
    rx_buf: int
    max_msg_len: int
    frame_max: int


def _sources_hash(user_config: str) -> str:
    digest = hashlib.sha256()
    paths = [user_config, os.path.join(NATIVE_DIR, "inspector_bridge.c"),
             os.path.join(NATIVE_DIR, "CMakeLists.txt")]
    for folder in ("src", "inc"):
        base = os.path.join(REPO_ROOT, folder)
        paths += [os.path.join(base, f) for f in sorted(os.listdir(base))]
    paths.append(os.path.join(REPO_ROOT, "baton.h"))
    for path in paths:
        with open(path, "rb") as f:
            digest.update(f.read())
    return digest.hexdigest()[:16]


def bridge_path(user_config: str) -> str:
    """Path the bridge for this configuration is (or will be) built at."""
    return os.path.join(CACHE_DIR, _sources_hash(user_config), "lib", LIB_FILE)


def build_bridge(user_config: str, log: Optional[Callable[[str], None]] = None) -> str:
    """Build the bridge for ``user_config`` (a baton_user_config.h) unless cached."""
    user_config = os.path.abspath(user_config)
    if not os.path.isfile(user_config):
        raise NativeError(f"baton_user_config.h not found: {user_config}")
    target = bridge_path(user_config)
    if os.path.isfile(target):
        return target
    cmake = shutil.which("cmake")
    if cmake is None:
        raise NativeError("CMake was not found; it is needed to build the native bridge")
    build_dir = os.path.dirname(os.path.dirname(target))
    config_dir = os.path.join(build_dir, "config")
    os.makedirs(config_dir, exist_ok=True)
    shutil.copyfile(user_config, os.path.join(config_dir, "baton_user_config.h"))
    configure = [cmake, "-S", NATIVE_DIR, "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release",
                 f"-DBATON_CONFIG_DIR={config_dir}"]
    if os.name == "nt" and shutil.which("gcc") and struct_bits() == 64:
        configure += ["-G", "MinGW Makefiles" if shutil.which("mingw32-make") else "Unix Makefiles"]
    for command in (configure, [cmake, "--build", build_dir, "--config", "Release"]):
        if log:
            log(" ".join(command))
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            raise NativeError("building the native bridge failed:\n" + result.stdout + result.stderr)
    if not os.path.isfile(target):
        raise NativeError(f"the build did not produce {target}")
    return target


def struct_bits() -> int:
    return ctypes.sizeof(ctypes.c_void_p) * 8


class Bridge:
    """The loaded bridge: message table, sizes, nodes and parsers."""

    _cache: Dict[str, "Bridge"] = {}
    _lock = threading.Lock()

    def __init__(self, path: str) -> None:
        self.path = path
        if os.name == "nt":
            self.lib = ctypes.CDLL(path, winmode=0)
        else:
            self.lib = ctypes.CDLL(path)
        lib = self.lib
        lib.bi_interface_version.restype = ctypes.c_int
        if lib.bi_interface_version() != INTERFACE_VERSION:
            raise NativeError("native bridge has another interface version; rebuild it")
        lib.bi_library_version.restype = ctypes.c_char_p
        lib.bi_config.argtypes = [ctypes.POINTER(ctypes.c_uint32)]
        lib.bi_msg_count.restype = ctypes.c_int
        lib.bi_msg_info.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint8),
                                    ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint8)]
        lib.bi_msg_info.restype = ctypes.c_char_p
        lib.bi_node_new.restype = ctypes.c_void_p
        lib.bi_node_free.argtypes = [ctypes.c_void_p]
        lib.bi_node_init.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint8), WRITE_CB, MESSAGE_CB, DROP_CB,
                                     POLL_CB, ctypes.c_void_p, ctypes.c_uint32]
        lib.bi_node_init.restype = ctypes.c_int
        lib.bi_node_process.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        lib.bi_node_rx.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32]
        lib.bi_node_tx_done.argtypes = [ctypes.c_void_p]
        lib.bi_node_send.argtypes = [ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint8,
                                     ctypes.c_char_p, ctypes.c_uint8, ctypes.c_uint8]
        lib.bi_node_send.restype = ctypes.c_int
        lib.bi_node_publish.argtypes = [ctypes.c_void_p, ctypes.c_uint8, ctypes.c_char_p,
                                        ctypes.c_uint8]
        lib.bi_node_publish.restype = ctypes.c_int
        lib.bi_node_stats.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
        lib.bi_parser_new.restype = ctypes.c_void_p
        lib.bi_parser_free.argtypes = [ctypes.c_void_p]
        lib.bi_parser_reset.argtypes = [ctypes.c_void_p]
        lib.bi_parser_idle.argtypes = [ctypes.c_void_p]
        lib.bi_parser_idle.restype = ctypes.c_int
        lib.bi_parser_feed.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
        lib.bi_parser_feed.restype = ctypes.c_int
        lib.bi_parser_frame.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8),
                                        ctypes.POINTER(ctypes.c_uint8)]
        lib.bi_encode.argtypes = [ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_char_p,
                                  ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint32]
        lib.bi_encode.restype = ctypes.c_int

        sizes = (ctypes.c_uint32 * 8)()
        lib.bi_config(sizes)
        self.config = BuildConfig(*[int(v) for v in sizes])
        self.library_version = lib.bi_library_version().decode()
        self.messages: List[MessageInfo] = []
        for i in range(lib.bi_msg_count()):
            msg_id, max_len, is_state = ctypes.c_uint8(), ctypes.c_uint8(), ctypes.c_uint8()
            name = lib.bi_msg_info(i, ctypes.byref(msg_id), ctypes.byref(max_len),
                                   ctypes.byref(is_state))
            self.messages.append(MessageInfo(name.decode(), msg_id.value, max_len.value,
                                             bool(is_state.value)))
        self.by_name = {m.name: m for m in self.messages}
        self.by_id = {m.msg_id: m for m in self.messages}

    @classmethod
    def for_config(cls, user_config: str, log: Optional[Callable[[str], None]] = None) -> "Bridge":
        """Build (if needed) and load the bridge for a baton_user_config.h; cached per process."""
        with cls._lock:
            path = build_bridge(user_config, log)
            if path not in cls._cache:
                cls._cache[path] = Bridge(path)
            return cls._cache[path]

    def encode(self, dst: int, src: int, ctrl: int, payload: bytes) -> bytes:
        out = (ctypes.c_uint8 * self.config.frame_max)()
        n = self.lib.bi_encode(dst, src, ctrl, bytes(payload), len(payload), out,
                               self.config.frame_max)
        if n < 0:
            raise ValueError("payload too long for BATON_MAX_PAYLOAD")
        return bytes(out[:n])


class Parser:
    """The library's incremental frame parser (baton_parser_feed)."""

    def __init__(self, bridge: Bridge) -> None:
        self.bridge = bridge
        self._lib = bridge.lib
        self._handle = self._lib.bi_parser_new()
        self._header = (ctypes.c_uint8 * 4)()
        self._payload = (ctypes.c_uint8 * max(bridge.config.max_payload, 1))()

    def __del__(self) -> None:
        handle = getattr(self, "_handle", None)
        if handle:
            self._lib.bi_parser_free(handle)
            self._handle = None

    def reset(self) -> None:
        self._lib.bi_parser_reset(self._handle)

    @property
    def idle(self) -> bool:
        return bool(self._lib.bi_parser_idle(self._handle))

    def feed(self, byte: int) -> int:
        """1 frame ready, 0 more bytes needed, -1 invalid LEN or CRC."""
        return self._lib.bi_parser_feed(self._handle, byte)

    def frame(self) -> Tuple[int, int, int, bytes]:
        """(dst, src, ctrl, payload) of the frame reported by feed() == 1."""
        self._lib.bi_parser_frame(self._handle, self._header, self._payload)
        length = self._header[3]
        return self._header[0], self._header[1], self._header[2], bytes(self._payload[:length])


class Node:
    """One Baton node (master or slave) running the library."""

    def __init__(self, bridge: Bridge, role: int, own: int, master: int, slaves: List[int],
                 timing: Dict[str, int], rx_echo: bool, now_ms: int,
                 write: Callable[[bytes], bool],
                 on_message: Optional[Callable[[int, int, bytes], None]] = None,
                 on_drop: Optional[Callable[[int, int, int], None]] = None,
                 on_poll_done: Optional[Callable[[int, bool, int], None]] = None) -> None:
        self.bridge = bridge
        self._lib = bridge.lib
        self.address = own
        self._handle = self._lib.bi_node_new()
        if not self._handle:
            raise MemoryError("bi_node_new")

        def _write(_user, data, length):
            return 0 if write(ctypes.string_at(data, length)) else -1

        def _message(_user, src, msg_id, data, length):
            if on_message:
                on_message(src, msg_id, ctypes.string_at(data, length) if length else b"")

        def _drop(_user, msg_id, dst, missed):
            if on_drop:
                on_drop(msg_id, dst, missed)

        def _poll(_user, slave, responded, nack):
            if on_poll_done:
                on_poll_done(slave, bool(responded), nack)

        # keep the ctypes callbacks alive as long as the node
        self._callbacks = (WRITE_CB(_write), MESSAGE_CB(_message), DROP_CB(_drop), POLL_CB(_poll))
        params = (ctypes.c_uint32 * 13)(
            role, own, master, len(slaves), timing["max_retries"], timing["tx_gap_ms"],
            timing["inter_byte_timeout_ms"], 1 if rx_echo else 0, timing["baudrate"],
            timing["slot_time_ms"], timing["ack_timeout_ms"], timing["cycle_period_ms"],
            timing["event_ttl_ms"])
        slave_array = (ctypes.c_uint8 * max(len(slaves), 1))(*slaves)
        status = self._lib.bi_node_init(self._handle, params, slave_array, *self._callbacks, None,
                                        now_ms & 0xFFFFFFFF)
        if status != 0:
            self._lib.bi_node_free(self._handle)
            self._handle = None
            raise ValueError(f"baton_node_init failed: {STATUS_TEXT.get(status, status)} "
                             f"(node 0x{own:02X})")

    def close(self) -> None:
        if self._handle:
            self._lib.bi_node_free(self._handle)
            self._handle = None

    def __del__(self) -> None:
        self.close()

    def process(self, now_ms: int) -> None:
        self._lib.bi_node_process(self._handle, now_ms & 0xFFFFFFFF)

    def rx(self, data: bytes) -> None:
        if data:
            self._lib.bi_node_rx(self._handle, bytes(data), len(data))

    def tx_done(self) -> None:
        self._lib.bi_node_tx_done(self._handle)

    def send(self, dst: int, msg_id: int, data: bytes, priority: int = PRIO_NORMAL) -> int:
        return self._lib.bi_node_send(self._handle, dst, msg_id, bytes(data), len(data), priority)

    def publish(self, msg_id: int, data: bytes) -> int:
        return self._lib.bi_node_publish(self._handle, msg_id, bytes(data), len(data))

    def stats(self) -> Dict[str, int]:
        out = (ctypes.c_uint32 * 16)()
        self._lib.bi_node_stats(self._handle, out)
        return {name: int(out[i]) for i, name in enumerate(STAT_NAMES)}
