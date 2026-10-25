---
name: baton-testing
description: Build and run the Baton-C tests — GoogleTest with the 100 % coverage gate, the bus simulation on many seeds (with ASan/UBSan on Linux), the Python twin and C/Python cross-check, the examples, MISRA — read the results and add tests or simulation scenarios. Use after any change to src/, inc/ or tests/, or when the user asks to test, verify, or reproduce a bug.
---

# Testing Baton-C

Quote the build directories in the shell: the brackets are glob characters.

## 1. Unit tests and coverage (gate)

```sh
cmake --preset "BATON[UnitTests]"
cmake --build "build/BATON[UnitTests]" --target coverage
```

- 132+ GoogleTest tests in `tests/unit/gtest_baton_*.cpp` (registered in `baton.cmake`),
  helpers in `baton_test_support.h/.cpp` (`Harness`: one node with captured writes,
  callbacks, critical-section counters, `inject()`, `echo_last()`, `run_until_write()`).
- The library sources are compiled into the test executable with `--coverage` and
  `-Werror`. The target fails below 100 % line or branch coverage of `src/`; the HTML
  report is in `build/BATON[UnitTests]/Coverage/`.
- Find what is missing:
  `gcovr -r . --object-directory "build/BATON[UnitTests]" --filter src --txt-metric branch`
  (run the tests first; `coverage` deletes the counters).
- A branch that cannot be reached through the API (or with white-box access to the
  struct members) is dead code: remove it and document the invariant.
- The tests use `tests/config/baton_user_config.h` (8 nodes, payload 64, ring 256).

## 2. Bus simulation on many seeds

```sh
cmake --build "build/BATON[UnitTests]" --target baton_sim
"build/BATON[UnitTests]/baton_sim" 12345          # one seed (reproduction)
tests/run_seeds.sh "build/BATON[UnitTests]" 100   # Windows: tests\run_seeds.ps1 -BuildDir ... -Count 100
# Linux, with ASan + UBSan:
cmake --preset "BATON[Sanitize]" && cmake --build "build/BATON[Sanitize]" --target baton_sim
tests/run_seeds.sh "build/BATON[Sanitize]" 100
```

Pass: `ALL TESTS PASSED` on ≥ 100 seeds, no sanitizer report. One seed is not enough:
protocol bugs (early acknowledgement) appear in 10–20 % of seeds. MinGW/MSVC have no
ASan runtime; use a Linux container (`docker run ubuntu:24.04`).

Each scenario prints three lines:

| Counter | Must be | Otherwise |
| --- | --- | --- |
| `dups` | 0 | An event delivered twice or out of order: dedup or packing order |
| `gaps`, `missing` (no dead node) | 0 | Silent loss: wrong commit, lost inflight reference, sequencing |
| `gaps + missing` with a dead node | ≤ `live nodes missed` | A loss not reported by `on_drop` |
| `collisions` | 0 | Token rule broken, slot overrun, ACK outside IDLE |
| `max slot` | ≤ limit | Slot budget or master timeout wrong |
| `badMsg`, `rxOvf`, `txTO` | 0 | Corrupted TLV, ring too small, no end of TX |
| `stale states` | 0 | The last state value did not arrive |

`crcErr`, `ackTO`, `retries`, `dupFiltered`, `respTO`, `queueFull` are expected under
noise or overload.

## 3. Adding a simulation scenario

`g_scenarios[]` in `tests/sim/baton_bus_sim.c`:

```c
/* name, nodes, deadIdx, ber, genMs, drainMs, meanGapMs, ttlMs, retries, cycleMs, expectDrops, noEcho */
{ "name", 4, -1, 0.002, 40000, 20000, 150, 60000, 1, 0, 0, 0 },
```

It becomes a GoogleTest case automatically (`BusSimScenario/N`). New behavior (for
example a node restart) usually needs new logic in `run()`. Trace with `printf` in the
simulator hooks, never in the library.

## 4. Integration tests (Python twin)

```sh
cmake --preset "BATON[IntegrationTests]" -DPython3_EXECUTABLE=<python with pytest>
cmake --build "build/BATON[IntegrationTests]" --target integration_tests
# or directly: python -m pytest tests/integration -q
```

The cross-check compiles `src/baton_protocol.c` with `gcc` and compares 300 random frames
(encode, parse, corrupted byte). A codec change must update `tests/integration/baton/protocol.py`.

## 5. Examples, analysis, format, docs

```sh
cmake --preset "BATON[Examples]" && cmake --build "build/BATON[Examples]" && ctest --test-dir "build/BATON[Examples]" -L example
cmake --build "build/BATON[Debug]" --target misra-analyze      # gate: 0 violations
cmake --build "build/BATON[Debug]" --target cppcheck-analyze
clang-format --dry-run --Werror baton.h src/*.c inc/*.h tests/unit/*.cpp
python tools/docs/check_snippets.py                              # every C snippet of docs/ compiles
```

## 6. Measurements for the documentation

When a change may alter a frame or a structure, re-measure and update
`docs/configuration.md` (commands are on that page): `arm-none-eabi-gcc -Os -fstack-usage
-fcallgraph-info=su` + `python tools/docs/stack_usage.py <dir>`, `arm-none-eabi-size`, and
`sizeof(struct baton_node)` for the template configuration.
