# Baton Inspector

A diagnostic GUI and a Python API for automated tests of Baton-C buses. The full
documentation is the *Baton Inspector* chapter of the Baton-C manual (`docs/inspector.md`).

| Mode | What runs | Hardware |
| --- | --- | --- |
| **Simulation** | Master and slaves on a simulated single-wire bus; scriptable slaves, auto replies, noise, dead slaves, late answers | None |
| **Master** | The PC is the master of a real bus: polls the slaves, sends events, publishes states | USB-UART or USB-RS-485 adapter |
| **Monitor** | The PC only listens and decodes the traffic of real nodes | USB-UART or USB-RS-485 adapter |
| **Attach** (GUI) | Shows the live feed of a session started by a test | — |

Every node of the tool runs the real Baton-C library: a native bridge built with the
`baton_user_config.h` of the system under test (CMake and a C11 compiler on the PC).
Nothing of the protocol is re-implemented in Python.

## Quick start

```bash
python -m pip install -r tools/baton_inspector/requirements.txt
python tools/baton_inspector/baton_inspector.py --config tools/baton_inspector/examples/example_bus.json --mode simulation --start
```

```python
import sys; sys.path.insert(0, "tools/baton_inspector")
from batoninspector import Session

with Session("tools/baton_inspector/examples/example_bus.json", mode="simulation", speed=0,
             record="run.jsonl") as bus:
    bus.send("0x11", "CMD_SET_OUTPUT", {"output": 2, "value": "on"})
    report = bus.wait_for("OUTPUT_REPORT", src="0x11", timeout_ms=1000)
    bus.assert_no_violations()
```

`examples/test_example_system.py` is a complete pytest suite; `tests/` are the tests of
the tool itself:

```bash
python -m pytest tools/baton_inspector/tests tools/baton_inspector/examples -v
```

The first session builds the native bridge into `tools/baton_inspector/build/<hash>/`
(2 s measured on Linux with GCC 13); later sessions with the same configuration
and library reuse it.
`python tools/baton_inspector/screenshots.py` drives the GUI in simulation mode and
writes the screenshots of the documentation.
