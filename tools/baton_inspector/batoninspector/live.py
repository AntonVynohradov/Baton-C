#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - live feed of a running session over a local
#              TCP socket, so the GUI can attach to a headless test session.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Live feed.

The session listens on 127.0.0.1 only. Each client first receives a ``hello``
record (bus definition, message table, limits) and the records collected so far,
then every new record as it happens — the same JSON objects as the JSON Lines log,
one per line. The feed is read-only: an attached GUI cannot control the test.
"""

from __future__ import annotations

import json
import socket
import threading
from typing import Any, Callable, Dict, List, Optional


class LiveServer:
    """Broadcasts records to every connected client."""

    def __init__(self, hello: Callable[[], Dict[str, Any]],
                 backlog: Callable[[], List[Dict[str, Any]]], port: int = 0) -> None:
        self._hello = hello
        self._backlog = backlog
        self._clients: List[socket.socket] = []
        self._lock = threading.Lock()
        self._server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server.bind(("127.0.0.1", port))
        self._server.listen(4)
        self.port = self._server.getsockname()[1]
        self._running = True
        self._thread = threading.Thread(target=self._accept, name="baton-live", daemon=True)
        self._thread.start()

    @staticmethod
    def _line(record: Dict[str, Any]) -> bytes:
        return (json.dumps(record, default=str) + "\n").encode()

    def _accept(self) -> None:
        while self._running:
            try:
                client, _ = self._server.accept()
            except OSError:
                break
            try:
                with self._lock:
                    client.sendall(self._line(self._hello()))
                    for record in self._backlog():
                        client.sendall(self._line(record))
                    self._clients.append(client)
            except OSError:
                client.close()

    def publish(self, record: Dict[str, Any]) -> None:
        data = self._line(record)
        with self._lock:
            for client in list(self._clients):
                try:
                    client.sendall(data)
                except OSError:
                    self._clients.remove(client)
                    client.close()

    def close(self) -> None:
        self._running = False
        try:
            self._server.close()
        except OSError:
            pass
        with self._lock:
            for client in self._clients:
                client.close()
            self._clients.clear()


class LiveClient:
    """Reads a live feed in a thread and hands every record to ``on_record``."""

    def __init__(self, port: int, on_record: Callable[[Dict[str, Any]], None],
                 on_closed: Optional[Callable[[], None]] = None, host: str = "127.0.0.1") -> None:
        self._socket = socket.create_connection((host, port), timeout=3.0)
        self._socket.settimeout(None)
        self._on_record = on_record
        self._on_closed = on_closed
        self._thread = threading.Thread(target=self._read, name="baton-live-client", daemon=True)
        self._thread.start()

    def _read(self) -> None:
        buffer = b""
        try:
            while True:
                chunk = self._socket.recv(65536)
                if not chunk:
                    break
                buffer += chunk
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    if line.strip():
                        self._on_record(json.loads(line))
        except OSError:
            pass
        finally:
            if self._on_closed:
                self._on_closed()

    def close(self) -> None:
        try:
            self._socket.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self._socket.close()
