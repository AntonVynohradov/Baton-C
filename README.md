# intercom

Deterministic master/slave message exchange over a single-wire UART (half-duplex, e.g. 9600 bit/s) for embedded systems.

- The master polls the slaves one by one (POLL), and the polled slave is the only transmitter on the bus for a bounded time. There are no collisions and no arbitration.
- During its slot a slave sends data to other slaves (DATA + ACK) and finally answers the master (RESP).
- **Events** (EVENT) are delivered exactly once, in order, with ACK and retry. **States** (STATE) work like a mailbox: the latest value always arrives.
- The library never blocks, does not use `malloc`, and allocates all memory statically. It takes about 6.6 KB of code and about 1.7 KB of RAM in the default configuration (Cortex-M3, `-Os`).
- The application includes a single header: `Intercom.h`, plus your own `IntercomConfig.h`.

```c
IntercomCfg cfg;
Intercom_GetDefaultCfg(&cfg);
cfg.role = INTERCOM_ROLE_SLAVE;  cfg.ownAddr = 0x11;  /* ... */
Intercom_Init(&cfg);

for (;;) {
    Intercom_Process();                       /* non-blocking, call every ≤1 ms */
    Intercom_Send(0x12, INTERCOM_MSG_CMD_START, &cmd, sizeof cmd, INTERCOM_PRIO_HIGH);
}
```

## Documentation

Start with [docs/index.md](docs/index.md):

- [Getting started](docs/getting-started.md): adding as a submodule, configuration, first node
- [Protocol](docs/protocol.md): frames, slots, ACK, sequencing, time budget
- [Architecture](docs/architecture.md): layers, queues, state machines
- [Configuration](docs/configuration.md): `IntercomConfig.h` and `IntercomCfg`
- [API](docs/api.md): functions and callbacks
- [Porting](docs/porting.md): STM32 single-wire, RS-485, RTOS
- [Testing](docs/testing.md): bus simulator

## Layout

```
include/Intercom.h                 public API (the only header for the application)
config/IntercomConfig_template.h   configuration template, copy it into your project
src/                               implementation (private headers)
tests/                             bus simulator + tests (host)
docs/                              documentation
CMakeLists.txt, CMakePresets.json  top-level build: presets, tests, static analysis targets
intercom.cmake                     library target (can be include()d by an application)
cmake/                             version from Git tag, simulation tests, MISRA check
CLAUDE.md, .claude/skills/         context and skills for Claude Code
```
