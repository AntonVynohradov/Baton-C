# Changelog

All notable changes to Baton-C are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/): the public API in `baton.h` and
`inc/` and the wire format stay compatible within a major version.

## [Unreleased]

## [1.0.0] - 2026-10-25

First public release. Baton-C continues the internal
`intercom` library (C99, single static instance, `Intercom_*` API) under a new name,
API and repository layout; the wire format and the delivery rules are unchanged.

### Added

- Layered library: definitions (`baton_defs.h`), codec (`baton_protocol`: CRC-16/CCITT-FALSE,
  incremental parser, in-place encoder), half-duplex link (`baton_link`: receive ring,
  inter-byte timeout, bus gap, echo check, transmit timeout), event queue (`baton_queue`)
  and node (`baton_node`: master/slave schedule, TLV packing, stop-and-wait channels,
  duplicate filter, TTL).
- Instance-based application API `baton_node_*` on a caller-owned `struct baton_node`;
  several buses in one program; explicit `now_ms` instead of a clock callback.
- Compile-time checks of the message lists: unique IDs, message lengths, queue capacity.
- Unit tests (GoogleTest, 132 tests) with the bus simulation as test cases, a multi-seed
  simulation runner (`baton_sim`, `tests/run_seeds.*`), and integration tests with a
  Python twin of the codec and a C/Python cross-check.
- Examples: bare-metal slave (runs in CI against a simulated bus with line noise), PC
  serial-port master (Linux, macOS, Windows), Zephyr slave.
- Documentation website (MkDocs Material) with the protocol specification, guides,
  measured memory, code size and stack, API reference, MISRA C:2012 page, and a printable
  PDF user manual; generated diagrams (`tools/docs/diagrams.py`) and a stack analysis
  script (`tools/docs/stack_usage.py`).
- Baton Inspector (`tools/baton_inspector`): diagnostic GUI (PySide6) and headless Python API
  for automated tests. Master, passive monitor and simulation modes on the real library
  (native bridge built per `baton_user_config.h`), live UML sequence diagram with frame
  timing, slot/ACK/cycle measurement and timing and bus-usage rules, JSON field layouts,
  JSON Lines session logs, live feed for watching a test from the GUI, example pytest suite.
- CI workflows: build, unit tests with the coverage gate, integration tests, static
  analysis (Cppcheck, MISRA, CodeQL), format check, static library, documentation, SBOM
  and release.

### Changed

- C11 instead of C99; `include/Intercom.h` replaced by `baton.h` and `inc/`;
  `IntercomConfig.h` replaced by `baton_user_config.h` (`BATON_*` macros); CMake target
  `baton` in `baton.cmake`, presets `BATON[...]`.
- Queue records are serialized byte by byte (no pointer casts); every function has a
  single exit point (MISRA C:2012).

### Quality

- MISRA C:2012: 0 violations reported by the Cppcheck addon; four deviations recorded in
  `docs/misra.md`. CI fails on any violation.
- 100 % line and branch coverage of `src/`, enforced by the `coverage` target.
- Code size 6620 B and 280 B of worst-case stack on Cortex-M4 (`-Os`), 1696 B of RAM per
  node with the configuration template.

[Unreleased]: https://github.com/AntonVynohradov/Baton-C/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/AntonVynohradov/Baton-C/releases/tag/v1.0.0
