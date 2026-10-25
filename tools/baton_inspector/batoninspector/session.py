#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Baton Inspector - a session on a bus: master, passive monitor or
#              simulation; the Python API for automated tests and the engine
#              of the GUI.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Sessions.

A session owns one processing thread that calls the library every millisecond
of session time; every call of the API is executed in that thread, so tests may
call it from anywhere. Modes:

``master``
    The PC is the master of a real bus (serial port). It polls the slaves and can
    send events and publish states.
``monitor``
    The PC only listens (serial port) and decodes the traffic of real nodes.
``simulation``
    Master and slaves run on the PC on a simulated wire; the slaves can be
    scripted, reply automatically, die, answer late or see noise.

Session time: milliseconds since ``start()``. In serial modes it follows the PC
clock. In simulation it follows the clock scaled by ``speed`` (1.0 = real time),
or — with ``speed=0`` — it advances only while the API waits (``wait_for()``,
``expect_none()``, ``run_for()``), which makes tests fast and repeatable.
"""

from __future__ import annotations

import queue
import threading
import time
from concurrent.futures import Future
from typing import Any, Callable, Dict, Iterable, List, Optional, Union

from .busconfig import BusConfig, frame_ms
from .decoder import StreamDecoder
from .layout import LayoutError
from .model import (SEVERITY_ORDER, ErrorRecord, FrameRecord, MessageRecord, Violation)
from .native import (PRIO_HIGH, PRIO_NORMAL, ROLE_MASTER, ROLE_SLAVE, STATUS_TEXT, Bridge, Node)
from .recorder import FORMAT_VERSION, Recorder
from .timing import TimingAnalyzer
from .transport import SerialTransport, SimBus

MODES = ("master", "monitor", "simulation")
Listener = Callable[[str, Any], None]
Where = Union[Dict[str, Any], Callable[[MessageRecord], bool], None]
LIVE_ONLY = ("message", "timing", "stats", "time")   # shown live, not written to the log
REPORT_PERIOD_MS = 500.0


class SessionError(RuntimeError):
    """A request the library or the session rejected."""


class WaitTimeout(AssertionError):
    """A wait ended without the expected message."""


class SimSlave:
    """A simulated slave (simulation mode); use ``Session.slave(address)``."""

    def __init__(self, session: "Session", address: int) -> None:
        self.session = session
        self.address = address
        self.node: Optional[Node] = None
        self.received: List[MessageRecord] = []
        self._publish_next: List[float] = []

    def send(self, dst: Union[int, str], name: str, fields: Optional[Dict[str, Any]] = None,
             data: Optional[bytes] = None, priority: str = "normal") -> None:
        """Queue an event of this slave (as if its application called baton_node_send)."""
        self.session._send_from(self, dst, name, fields, data, priority)

    def publish(self, name: str, fields: Optional[Dict[str, Any]] = None,
                data: Optional[bytes] = None) -> None:
        """Publish a state of this slave."""
        self.session._publish_from(self, name, fields, data)

    def set_dead(self, dead: bool = True) -> None:
        """Unpower (or power again) this slave."""
        self.session._call(lambda: self.session._set_dead(self.address, dead))

    def set_tx_delay(self, delay_ms: float) -> None:
        """Delay the start of each transmission of this slave (provokes late answers)."""
        self.session._call(lambda: self.session._bus.tx_delay_ms.__setitem__(self.address,
                                                                             float(delay_ms)))

    def stats(self) -> Dict[str, int]:
        return self.session._call(lambda: self.node.stats() if self.node else {})


class Session:
    """A session on one bus. Use as a context manager or call start()/stop()."""

    def __init__(self, config: Union[BusConfig, str], mode: str = "simulation",
                 port: Optional[str] = None, rx_echo: Optional[bool] = None,
                 speed: Optional[float] = None, record: Optional[str] = None,
                 live_port: Optional[int] = None, seed: int = 1,
                 log: Optional[Callable[[str], None]] = None) -> None:
        if mode not in MODES:
            raise ValueError(f"mode must be one of {MODES}")
        self.config = BusConfig.load(config) if isinstance(config, str) else config
        self.mode = mode
        self.port = port
        if mode in ("master", "monitor") and not port:
            raise ValueError(f"mode '{mode}' needs a serial port")
        self.rx_echo = self.config.bus["rx_echo"] if rx_echo is None else bool(rx_echo)
        self.speed = self.config.simulation.speed if speed is None else float(speed)
        self.seed = seed
        self._log = log or (lambda text: None)
        self.bridge = Bridge.for_config(self.config.user_config, self._log)
        problems = self.config.check_against(self.bridge.by_name, self.bridge.config.max_nodes)
        if problems:
            raise SessionError("bus definition does not match baton_user_config.h:\n  "
                               + "\n  ".join(problems))
        self.rules = self.config.rules(self.bridge.config.max_payload)
        self.baudrate = int(self.config.bus["baudrate"])

        self.frames: List[FrameRecord] = []
        self.errors: List[ErrorRecord] = []
        self.messages: List[MessageRecord] = []
        self.violations: List[Violation] = []
        self.received: List[MessageRecord] = []     # delivered to the session's master node
        self.polls: List[Dict[str, Any]] = []       # on_poll_done of the master node
        self.drops: List[Dict[str, Any]] = []       # on_drop of the master node
        self.marks: List[Dict[str, Any]] = []
        self._listeners: List[Listener] = []
        self._records_lock = threading.RLock()
        self._changed = threading.Condition(self._records_lock)

        self._commands: "queue.Queue[tuple]" = queue.Queue()
        self._wake = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._running = False
        self._now = 0.0
        self._target = 0.0          # stepped simulation: advance up to this time
        self._t0 = 0.0
        self.wall_start = 0.0
        self._mark_ms = 0.0
        self._recorder: Optional[Recorder] = None
        self._record_path = record
        self._live = None
        self._live_port = live_port
        self.master_node: Optional[Node] = None
        self._sim_slaves: Dict[int, SimSlave] = {}
        self._bus: Optional[SimBus] = None
        self._serial: Optional[SerialTransport] = None
        self._pending_tx_done: List[float] = []
        self._next_stats_ms = 0.0
        self.error: Optional[BaseException] = None

        frame_gap = float(self.config.bus["inter_byte_timeout_ms"])
        if mode != "simulation":
            # USB adapters deliver bytes in chunks: only a long gap is a cut-off frame
            frame_gap += self.rules.tolerance_ms + 16.0
        self.decoder = StreamDecoder(self.config, self.bridge, frame_gap, self._on_record)
        self.analyzer = TimingAnalyzer(self.config, self.rules, self._on_violation)

    # ======================================================================
    # lifecycle
    # ======================================================================

    def __enter__(self) -> "Session":
        self.start()
        return self

    def __exit__(self, *exc: Any) -> None:
        self.stop()

    @property
    def stepped(self) -> bool:
        return self.mode == "simulation" and self.speed <= 0.0

    def start(self) -> "Session":
        if self._running:
            return self
        self.wall_start = time.time()
        self._t0 = time.perf_counter()
        self._now = 0.0
        self._target = 0.0
        if self._record_path:
            self.record(self._record_path)
        if self.mode == "simulation":
            self._start_simulation()
        else:
            self._serial = SerialTransport(self.port, self.baudrate)
            if self.mode == "master":
                self.master_node = self._new_node(ROLE_MASTER, self.config.master, self._own_write,
                                                  self.rx_echo)
        if self._live_port is not None:
            self.serve_live(self._live_port)
        self._running = True
        self._thread = threading.Thread(target=self._loop, name="baton-session", daemon=True)
        self._thread.start()
        self._log(f"session started: {self.mode}")
        return self

    def stop(self) -> None:
        if not self._running:
            return
        self._running = False
        self._wake.set()
        if self._thread is not None:
            self._thread.join(timeout=5.0)
        self.stop_recording()
        if self._live is not None:
            self._live.close()
            self._live = None
        if self._serial is not None:
            self._serial.close()
            self._serial = None
        self._log("session stopped")

    @property
    def now_ms(self) -> float:
        """Current session time."""
        return self._now

    # ======================================================================
    # observation
    # ======================================================================

    def add_listener(self, listener: Listener) -> None:
        """``listener(kind, record)`` for every new record; kind is 'frame', 'message',
        'error', 'violation', 'mark', 'stats'. Called from the session thread."""
        self._listeners.append(listener)

    def remove_listener(self, listener: Listener) -> None:
        if listener in self._listeners:
            self._listeners.remove(listener)

    def _emit(self, kind: str, record: Any) -> None:
        for listener in list(self._listeners):
            try:
                listener(kind, record)
            except Exception as exc:  # noqa: BLE001 - a viewer must not stop the session
                self._log(f"listener error: {exc}")
        body = record.to_json() if hasattr(record, "to_json") else record
        t = getattr(record, "t_end_ms", None)
        if t is None:
            t = getattr(record, "t_ms", None)
        if self._recorder is not None and kind not in LIVE_ONLY:
            self._recorder.write(kind, body, t)
        if self._live is not None and kind != "message":
            payload = {"type": kind}
            payload.update(body)
            self._live.publish(payload)

    def _on_record(self, record: Union[FrameRecord, ErrorRecord]) -> None:
        with self._changed:
            if isinstance(record, FrameRecord):
                self.frames.append(record)
                self.messages.extend(record.messages)
            else:
                self.errors.append(record)
            self._changed.notify_all()
        if isinstance(record, FrameRecord):
            self.analyzer.on_frame(record)
            self._emit("frame", record)
            for message in record.messages:
                self._emit("message", message)
        else:
            self.analyzer.on_error(record)
            self._emit("error", record)

    def _on_violation(self, violation: Violation) -> None:
        with self._changed:
            self.violations.append(violation)
            self._changed.notify_all()
        self._emit("violation", violation)

    def mark(self, text: str = "") -> float:
        """Insert a marker (logged, shown in the GUI); later waits look after this time."""
        t = self._now
        record = {"t_ms": t, "text": text}
        with self._changed:
            self.marks.append(record)
        self._mark_ms = t
        self._emit("mark", record)
        return t

    def apply_config(self, config: BusConfig) -> List[str]:
        """Use new message layouts and timing rules of an edited bus definition (same bus).
        Returns what could not be applied (topology and bus parameters need a restart)."""
        notes = []
        if config.nodes != self.config.nodes or config.bus != self.config.bus:
            notes.append("topology and bus parameters take effect after a restart")
        problems = config.check_against(self.bridge.by_name, self.bridge.config.max_nodes)
        if problems:
            raise SessionError("; ".join(problems))

        def swap() -> None:
            self.config.layouts = config.layouts
            self.config.tolerance_ms = config.tolerance_ms
            self.config.rules_override = config.rules_override
            self.config.raw = config.raw
            self.rules = self.config.rules(self.bridge.config.max_payload)
            self.analyzer.rules = self.rules

        self._call(swap)
        return notes

    # ======================================================================
    # recording and live feed
    # ======================================================================

    def _session_header(self) -> Dict[str, Any]:
        return {"format": FORMAT_VERSION, "start": self.wall_start, "mode": self.mode,
                "name": self.config.name, "bus": self.config.bus, "master": self.config.master,
                "slaves": self.config.slaves,
                "node_names": {str(k): v for k, v in self.config.node_names.items()},
                "limits": self.analyzer.summary()["limits"],
                "library_version": self.bridge.library_version,
                "messages": [{"name": m.name, "id": m.msg_id, "max_length": m.max_length,
                              "kind": "state" if m.is_state else "event"}
                             for m in self.bridge.messages],
                "config": self.config.raw}

    def record(self, path: str) -> None:
        """Start writing the session to a JSON Lines file (see recorder.py)."""
        self.stop_recording()
        self._recorder = Recorder(path, self.wall_start or time.time())
        self._recorder.write("session", self._session_header())
        self._record_path = path

    def stop_recording(self) -> Optional[str]:
        recorder = self._recorder
        if recorder is None:
            return None
        recorder.write("stats", {"nodes": self._stats_unlocked()}, self._now)
        recorder.write("end", {"t_ms": self._now, "timing": self.analyzer.summary(),
                               "violations": len(self.violations)})
        recorder.close()
        self._recorder = None
        return recorder.path

    def serve_live(self, port: int = 0) -> int:
        """Publish the session on 127.0.0.1:port for the GUI ('Attach'); returns the port."""
        from .live import LiveServer  # noqa: PLC0415

        def hello() -> Dict[str, Any]:
            header = self._session_header()
            header["type"] = "hello"
            header["now_ms"] = self._now
            return header

        def backlog() -> List[Dict[str, Any]]:
            with self._records_lock:
                items: List[tuple] = [(f.t_end_ms, "frame", f.to_json()) for f in self.frames]
                items += [(e.t_ms, "error", e.to_json()) for e in self.errors]
                items += [(v.t_ms, "violation", v.to_json()) for v in self.violations]
                items += [(m["t_ms"], "mark", dict(m)) for m in self.marks]
            out = []
            for _, kind, body in sorted(items, key=lambda x: x[0]):
                body = dict(body)
                body["type"] = kind
                out.append(body)
            return out

        if self._live is None:
            self._live = LiveServer(hello, backlog, port)
        return self._live.port

    # ======================================================================
    # the processing thread
    # ======================================================================

    def _call(self, function: Callable[[], Any]) -> Any:
        """Run ``function`` in the session thread and return its result."""
        if not self._running or threading.current_thread() is self._thread:
            return function()
        future: Future = Future()
        self._commands.put((function, future))
        self._wake.set()
        return future.result(timeout=10.0)

    def _run_commands(self) -> None:
        while True:
            try:
                function, future = self._commands.get_nowait()
            except queue.Empty:
                return
            try:
                future.set_result(function())
            except BaseException as exc:  # noqa: BLE001 - handed to the caller
                future.set_exception(exc)

    def _loop(self) -> None:
        try:
            while self._running:
                self._run_commands()
                if self.mode == "simulation":
                    if self.stepped:
                        if self._now + 1.0 <= self._target:
                            self._tick_simulation()
                        else:
                            self._wake.wait(0.05)
                            self._wake.clear()
                    else:
                        wall = (time.perf_counter() - self._t0) * 1000.0 * self.speed
                        steps = 0
                        while self._now + 1.0 <= wall and steps < 200:
                            self._tick_simulation()
                            steps += 1
                        if steps == 0:
                            self._wake.wait(0.0005)
                            self._wake.clear()
                else:
                    self._tick_serial()
        except BaseException as exc:  # noqa: BLE001 - reported to the API users
            self.error = exc
            self._log(f"session thread stopped: {exc!r}")
            self._running = False
        with self._changed:
            self._changed.notify_all()

    def _advance(self) -> None:
        with self._changed:
            self._changed.notify_all()
        if self._now >= self._next_stats_ms and (self._listeners or self._live is not None):
            self._next_stats_ms = self._now + REPORT_PERIOD_MS
            self._emit("time", {"t_ms": self._now})
            self._emit("timing", dict(self.analyzer.summary(), t_ms=self._now))
            self._emit("stats", {"t_ms": self._now, "nodes": self._stats_unlocked()})

    # ---- serial -------------------------------------------------------------------

    def _own_write(self, data: bytes) -> bool:
        """Write function of the master node on a serial port."""
        ok = self._serial.write(data) if self._serial else False
        if ok and not self.rx_echo:
            # no echo: the own bytes are not received; account them at the wire times
            start = self._now
            byte_ms = 10000.0 / self.baudrate
            self.decoder.mark_own(len(data))
            self.decoder.feed(data, [start + (i + 1) * byte_ms for i in range(len(data))])
            self._pending_tx_done.append(start + len(data) * byte_ms)
        elif ok:
            self.decoder.mark_own(len(data))
        return ok

    def _tick_serial(self) -> None:
        self._now = (time.perf_counter() - self._t0) * 1000.0
        data = self._serial.read() if self._serial else b""
        if data:
            times = [self._now] * len(data)
            self.decoder.feed(data, times)
            if self.master_node is not None:
                self.master_node.rx(data)
        if self.master_node is not None:
            for due in list(self._pending_tx_done):
                if self._now >= due:
                    self._pending_tx_done.remove(due)
                    self.master_node.tx_done()
            self.master_node.process(int(self._now))
        self._advance()
        if not data:
            time.sleep(0.0005)

    # ---- simulation ---------------------------------------------------------------

    def _start_simulation(self) -> None:
        sim = self.config.simulation
        self._bus = SimBus(self.baudrate, sim.noise, self.seed, set(sim.dead),
                           dict(sim.tx_delay_ms))
        self.master_node = self._new_node(ROLE_MASTER, self.config.master,
                                          lambda data: self._bus.transmit(self.config.master, data,
                                                                          self._now), True)
        for address in self.config.slaves:
            slave = SimSlave(self, address)
            self._sim_slaves[address] = slave
            slave.node = self._new_node(
                ROLE_SLAVE, address,
                lambda data, a=address: self._bus.transmit(a, data, self._now), True,
                on_message=lambda src, msg_id, data, s=slave: self._sim_received(s, src, msg_id,
                                                                                  data))
            spec = sim.slaves.get(address)
            slave._publish_next = [float(r.get("offset_ms", 0.0)) for r in (spec.publish if spec else [])]

    def _set_dead(self, address: int, dead: bool) -> None:
        if dead:
            self._bus.dead.add(address)
        else:
            self._bus.dead.discard(address)

    def set_noise(self, probability: float) -> None:
        """Simulation: probability that a byte is corrupted (0 = clean bus)."""
        self._require_simulation()
        self._call(lambda: setattr(self._bus, "noise", float(probability)))

    def _tick_simulation(self) -> None:
        self._now += 1.0
        deliveries = self._bus.step(self._now)
        if deliveries:
            data = bytes(d[0] for d in deliveries)
            times = [d[1] for d in deliveries]
            # bytes of the session's master are 'own' frames
            own = sum(1 for d in deliveries if d[2] == self.config.master)
            if own:
                self.decoder.mark_own(own)
            self.decoder.feed(data, times)
            if self.config.master not in self._bus.dead:
                self.master_node.rx(data)
            for address, slave in self._sim_slaves.items():
                if address not in self._bus.dead:
                    slave.node.rx(data)
        now = int(self._now)
        self.master_node.process(now)
        for address, slave in self._sim_slaves.items():
            if address not in self._bus.dead:
                slave.node.process(now)
                self._sim_periodic(slave)
        self._advance()

    def _sim_periodic(self, slave: SimSlave) -> None:
        spec = self.config.simulation.slaves.get(slave.address)
        if spec is None:
            return
        for i, rule in enumerate(spec.publish):
            if self._now >= slave._publish_next[i]:
                slave._publish_next[i] = self._now + float(rule.get("period_ms", 1000))
                fields = {k: self._sim_value(v) for k, v in dict(rule.get("fields", {})).items()}
                data = bytes.fromhex(rule["data"]) if "data" in rule else None
                try:
                    self._publish_now(slave.node, rule["state"], fields or None, data)
                except (SessionError, LayoutError) as exc:
                    self._log(f"simulated publish of 0x{slave.address:02X} failed: {exc}")

    def _sim_value(self, value: Any) -> Any:
        if value == "$uptime_s":
            return int(self._now // 1000)
        if value == "$uptime_ms":
            return int(self._now)
        return value

    def _sim_received(self, slave: SimSlave, src: int, msg_id: int, data: bytes) -> None:
        record = self._delivered(src, slave.address, msg_id, data)
        slave.received.append(record)
        spec = self.config.simulation.slaves.get(slave.address)
        if spec is None:
            return
        for rule in spec.auto_reply:
            if rule.get("on") != record.name:
                continue
            target = src if rule.get("to", "sender") == "sender" else self._address(rule["to"])
            reply_data: Optional[bytes] = None
            fields = None
            if rule.get("copy", False):
                reply_data = data
            elif "data" in rule:
                reply_data = bytes.fromhex(rule["data"])
            else:
                fields = {k: (record.values.get(v[1:]) if isinstance(v, str) and v.startswith("$")
                              else v) for k, v in dict(rule.get("fields", {})).items()}
            try:
                self._send_now(slave.node, target, rule["send"], fields, reply_data,
                               rule.get("priority", "normal"))
            except (SessionError, LayoutError) as exc:
                self._log(f"auto reply of 0x{slave.address:02X} failed: {exc}")

    # ======================================================================
    # nodes, sending, publishing
    # ======================================================================

    def _new_node(self, role: int, address: int, write: Callable[[bytes], bool], rx_echo: bool,
                  on_message: Optional[Callable[[int, int, bytes], None]] = None) -> Node:
        own = role == ROLE_MASTER
        return Node(self.bridge, role, address, self.config.master, self.config.slaves,
                    self.config.timing_params(), rx_echo, int(self._now), write,
                    on_message or (lambda src, msg_id, data: self._master_received(src, msg_id,
                                                                                   data)),
                    on_drop=(lambda msg_id, dst, missed: self.drops.append(
                        {"t_ms": self._now, "msg_id": msg_id, "dst": dst, "missed": missed}))
                    if own else None,
                    on_poll_done=(lambda slave, ok, nack: self.polls.append(
                        {"t_ms": self._now, "slave": slave, "responded": ok, "nack": nack}))
                    if own else None)

    def _delivered(self, src: int, dst: int, msg_id: int, data: bytes) -> MessageRecord:
        info = self.bridge.by_id.get(msg_id)
        name = info.name if info else f"0x{msg_id:02X}"
        layout = self.config.layouts.get(name)
        values = layout.decode(data) if layout and layout.fields else {}
        from .layout import decode_text  # noqa: PLC0415
        return MessageRecord(self._now, -1, src, dst, name, msg_id,
                             ("state" if info.is_state else "event") if info else "unknown",
                             data, values, decode_text(layout, data))

    def _master_received(self, src: int, msg_id: int, data: bytes) -> None:
        record = self._delivered(src, self.config.master, msg_id, data)
        with self._changed:
            self.received.append(record)
            self._changed.notify_all()

    def _address(self, value: Union[int, str]) -> int:
        if isinstance(value, int):
            return value
        if value.lower() == "broadcast":
            return 0xFF
        for address, name in self.config.node_names.items():
            if name == value:
                return address
        return int(value, 0)

    def _payload(self, name: str, fields: Optional[Dict[str, Any]], data: Optional[bytes]) -> bytes:
        if data is not None and fields is not None:
            raise SessionError("give either fields or data, not both")
        if data is not None:
            return bytes(data)
        layout = self.config.layouts.get(name)
        if fields is None:
            fields = {}
        if layout is None or not layout.fields:
            if fields:
                raise SessionError(f"{name} has no field layout in the bus definition; pass data=")
            return b""
        return layout.encode(fields)

    def _message_id(self, name: str, state: bool) -> int:
        info = self.bridge.by_name.get(name)
        if info is None:
            raise SessionError(f"unknown message '{name}'")
        if info.is_state != state:
            raise SessionError(f"{name} is {'a state' if info.is_state else 'an event'}")
        return info.msg_id

    def _send_now(self, node: Node, dst: Union[int, str], name: str,
                  fields: Optional[Dict[str, Any]], data: Optional[bytes], priority: str) -> None:
        payload = self._payload(name, fields, data)
        prio = {"normal": PRIO_NORMAL, "high": PRIO_HIGH}[priority]
        status = node.send(self._address(dst), self._message_id(name, False), payload, prio)
        if status != 0:
            raise SessionError(f"send {name}: {STATUS_TEXT.get(status, status)}")

    def _publish_now(self, node: Node, name: str, fields: Optional[Dict[str, Any]],
                     data: Optional[bytes]) -> None:
        payload = self._payload(name, fields, data)
        status = node.publish(self._message_id(name, True), payload)
        if status != 0:
            raise SessionError(f"publish {name}: {STATUS_TEXT.get(status, status)}")

    def _require_node(self) -> Node:
        if self.master_node is None:
            raise SessionError("this session has no node of its own (monitor mode)")
        return self.master_node

    def _require_simulation(self) -> None:
        if self.mode != "simulation":
            raise SessionError("only in simulation mode")

    def send(self, dst: Union[int, str], name: str, fields: Optional[Dict[str, Any]] = None,
             data: Optional[bytes] = None, priority: str = "normal") -> float:
        """Master: queue an event; ``fields`` uses the layout of the bus definition, ``data``
        gives raw bytes. ``dst`` is an address, a node name or 'broadcast'. Returns the time
        of the request; later waits look after it."""
        node = self._require_node()
        self._call(lambda: self._send_now(node, dst, name, fields, data, priority))
        self._mark_ms = self._now
        return self._mark_ms

    def publish(self, name: str, fields: Optional[Dict[str, Any]] = None,
                data: Optional[bytes] = None) -> float:
        """Master: publish a state."""
        node = self._require_node()
        self._call(lambda: self._publish_now(node, name, fields, data))
        self._mark_ms = self._now
        return self._mark_ms

    def _send_from(self, slave: SimSlave, dst, name, fields, data, priority) -> None:
        self._call(lambda: self._send_now(slave.node, dst, name, fields, data, priority))
        self._mark_ms = self._now

    def _publish_from(self, slave: SimSlave, name, fields, data) -> None:
        self._call(lambda: self._publish_now(slave.node, name, fields, data))
        self._mark_ms = self._now

    def slave(self, address: Union[int, str]) -> SimSlave:
        """Simulation: the simulated slave with this address or name."""
        self._require_simulation()
        return self._sim_slaves[self._address(address)]

    # ======================================================================
    # waiting
    # ======================================================================

    def _wait_until(self, deadline: float, done: Callable[[], bool]) -> bool:
        while True:
            with self._changed:
                if done():
                    return True
                if self._now >= deadline or not self._running:
                    if self.error is not None:
                        raise SessionError(f"session thread failed: {self.error!r}")
                    return done()
                if self.stepped:
                    self._target = min(deadline, self._now + 20.0)
                    self._wake.set()
                self._changed.wait(0.05)

    def run_for(self, duration_ms: float) -> None:
        """Let the bus run for ``duration_ms`` of session time."""
        deadline = self._now + duration_ms
        self._wait_until(deadline, lambda: False)

    def _matches(self, record: MessageRecord, name, src, dst, where: Where,
                 retransmissions: bool) -> bool:
        if record.retransmission and not retransmissions:
            return False
        if name is not None and record.name != name:
            return False
        if src is not None and record.src != self._address(src):
            return False
        if dst is not None and record.dst != self._address(dst):
            return False
        if callable(where):
            return bool(where(record))
        if where:
            return all(record.values.get(k) == v for k, v in where.items())
        return True

    def find(self, name: Optional[str] = None, src=None, dst=None, where: Where = None,
             since_ms: Optional[float] = None, until_ms: Optional[float] = None,
             retransmissions: bool = False) -> List[MessageRecord]:
        """Messages seen on the bus that match (all criteria optional)."""
        since = -1.0 if since_ms is None else since_ms
        until = float("inf") if until_ms is None else until_ms
        with self._records_lock:
            return [m for m in self.messages if since <= m.t_ms <= until
                    and self._matches(m, name, src, dst, where, retransmissions)]

    def wait_for(self, name: Optional[str] = None, src=None, dst=None, where: Where = None,
                 timeout_ms: float = 1000.0, since_ms: Optional[float] = None) -> MessageRecord:
        """Wait for a message seen on the bus after ``since_ms`` (default: the last send,
        publish or mark). ``where`` is a dict of field values or a predicate. Raises
        WaitTimeout."""
        since = self._mark_ms if since_ms is None else since_ms
        found: List[MessageRecord] = []

        def done() -> bool:
            found[:] = self.find(name, src, dst, where, since)
            return bool(found)

        if not self._wait_until(self._now + timeout_ms, done):
            raise WaitTimeout(f"no message name={name} src={src} dst={dst} where={where} within "
                              f"{timeout_ms:g} ms (since t={since:.0f} ms)")
        return found[0]

    def expect_none(self, name: Optional[str] = None, src=None, dst=None, where: Where = None,
                    within_ms: float = 1000.0, since_ms: Optional[float] = None) -> None:
        """Assert that no matching message appears within ``within_ms``."""
        since = self._mark_ms if since_ms is None else since_ms
        found: List[MessageRecord] = []

        def done() -> bool:
            found[:] = self.find(name, src, dst, where, since)
            return bool(found)

        if self._wait_until(self._now + within_ms, done):
            m = found[0]
            raise AssertionError(f"unexpected {m.name} from 0x{m.src:02X} at {m.t_ms:.1f} ms: "
                                 f"{m.text}")

    def latest_state(self, name: str, src) -> Optional[Dict[str, Any]]:
        """The last value of a state seen on the bus from ``src`` (decoded fields, or
        ``{'data': hex}`` without a layout); None if never seen."""
        records = self.find(name, src)
        if not records:
            return None
        last = records[-1]
        return dict(last.values) if last.values else {"data": last.data.hex(" ")}

    # ======================================================================
    # timing and statistics
    # ======================================================================

    def get_violations(self, min_severity: str = "info", rule: Optional[str] = None,
                       since_ms: Optional[float] = None,
                       until_ms: Optional[float] = None) -> List[Violation]:
        level = SEVERITY_ORDER[min_severity]
        since = -1.0 if since_ms is None else since_ms
        until = float("inf") if until_ms is None else until_ms
        with self._records_lock:
            return [v for v in self.violations if SEVERITY_ORDER[v.severity] >= level
                    and (rule is None or v.rule == rule) and since <= v.t_ms <= until]

    def assert_no_violations(self, min_severity: str = "warning",
                             since_ms: Optional[float] = None, until_ms: Optional[float] = None,
                             ignore: Iterable[str] = ()) -> None:
        """Fail when a violation of at least ``min_severity`` occurred in the window."""
        ignored = set(ignore)
        found = [v for v in self.get_violations(min_severity, None, since_ms, until_ms)
                 if v.rule not in ignored]
        if found:
            lines = [f"  {v.t_ms:10.1f} ms  {v.severity:7}  {v.rule}: {v.text}" for v in found[:20]]
            more = f"\n  ... and {len(found) - 20} more" if len(found) > 20 else ""
            raise AssertionError(f"{len(found)} timing/bus violation(s):\n" + "\n".join(lines)
                                 + more)

    def timing_summary(self) -> Dict[str, Any]:
        """Measured slots per slave, ACK times and cycles (count, min, avg, max) and limits."""
        return self._call(self.analyzer.summary)

    def _stats_unlocked(self) -> Dict[str, Dict[str, int]]:
        out = {}
        if self.master_node is not None:
            out[f"0x{self.config.master:02X}"] = self.master_node.stats()
        for address, slave in self._sim_slaves.items():
            out[f"0x{address:02X}"] = slave.node.stats()
        return out

    def stats(self) -> Dict[str, Dict[str, int]]:
        """baton_stats of the session's own nodes (master; simulated slaves)."""
        return self._call(self._stats_unlocked)
