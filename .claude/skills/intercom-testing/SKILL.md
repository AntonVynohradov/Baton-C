---
name: intercom-testing
description: Build and run the intercom host simulation tests, run many seeds with sanitizers, read the results, and add new scenarios. Use after any change to src/ or tests/, or when the user asks to test, verify, or reproduce a bug in the intercom library.
---

# Testing the intercom library

## 1. Building

Build with sanitizers, preferred when verifying changes:

```sh
cmake --preset "INTERCOM[SimTestsSanitize]"          # INTERCOM_BUILD_TESTS + INTERCOM_SANITIZERS
cmake --build "build/INTERCOM[SimTestsSanitize]"
```

- Without sanitizers: preset `INTERCOM[SimTests]` (build dir `build/INTERCOM[SimTests]`). Quote the build dir in the shell, the brackets are glob characters.
- Sanitizers need GCC/Clang on Linux/macOS. MinGW and MSVC have no ASan/UBSan runtime: use `INTERCOM[SimTests]` there.
- With `INTERCOM_BUILD_TESTS=ON` the library is compiled with `-Wall -Wextra -Wpedantic -Wconversion -Werror`. Every warning must be fixed, not silenced.
- The tests use their own configuration: `tests/config/IntercomConfig.h`.

## 2. Running

```sh
B="build/INTERCOM[SimTests]"
"$B/intercom_sim_test"                     # default seed
"$B/intercom_sim_test" 12345               # specific seed (reproduction)
ctest --test-dir "$B" -L sim               # same as the default seed, through CTest
tests/run_seeds.sh "$B" 100                # 100 seeds; prints only the failing ones
# Windows PowerShell:
# .\tests\run_seeds.ps1 -BuildDir "build/INTERCOM[SimTests]" -Count 100
```

**Pass criteria for every change:**
- `ALL TESTS PASSED` on ≥ 100 seeds,
- a warning-free build,
- no ASan/UBSan reports.

One passing seed is not enough: protocol bugs (e.g. acknowledging in advance) show up in ~10–20% of seeds.

## 3. Reading the results

Each scenario prints three lines:

```
sent N deliveries, delivered M msgs (K state), dups D, gaps G, missing X, drops P (live nodes missed L)
queueFull, collisions, crcErr, ackTO, retries, dupFiltered, respTO, badMsg, rxOvf, txTO
polls (failed), max slot ms (limit), stale states
```

| Counter | Must be | Meaning when it is not |
| --- | --- | --- |
| `dups` | 0 | A recipient got the same event twice or out of order. A deduplication or packing-order problem |
| `gaps`, `missing` (no dead node) | 0 | Silent loss. Most often a wrong commit (ACK for a different frame), a lost inflight reference or wrong sequencing |
| `gaps + missing` with a dead node | ≤ `live nodes missed` | A loss not reported through `onDrop` |
| `collisions` | 0 | Two transmitters at once: the token rule was broken, a slot was overrun, or an ACK was sent outside IDLE |
| `max slot` | ≤ limit | The slot budget (`slave_budget`) or the master timeout is miscalculated |
| `badMsg`, `rxOvf`, `txTO` | 0 | Corrupted TLV, RX buffer too small, or no end of TX |
| `stale states` | 0 | The last state value did not arrive |

`crcErr`, `ackTO`, `retries`, `dupFiltered`, `respTO` and `queueFull` are expected under noise or overload.

## 4. Reproducing a bug

1. Take the seed from `run_seeds`.
2. Run `intercom_sim_test <seed>`. `ERROR:` messages show the node, sender, sequence number and time in ms.
3. Narrow the problem down to a single scenario: temporarily shorten the `sc[]` array in `main()`.
4. For tracing, add `printf` in the test (hooks `hk_tx`, `hk_msg`), **not in the library**.

## 5. Adding a scenario

In `tests/sim_test.c`, the `sc[]` array in `main()`:

```c
/* name, nodes, deadIdx, ber, genMs, drainMs, meanGapMs, ttlMs, retries, cycleMs, expectDrops, noEcho */
{ "name", 4, -1, 0.002, 40000, 20000, 150, 60000, 1, 0, 0, 0 },
```

- `nodes` is master + slaves (≤ 8, `MAXN`). `deadIdx` is the index of the powered-off node, or −1.
- `ber` is the probability of corrupting a byte (a random bit).
- `meanGapMs` is the mean interval between events per node. Smaller means higher load.
- `drainMs` is the time without new traffic for draining the queues. It must be long enough, otherwise `missing > 0`.
- `expectDrops = 1` only with a dead node or a short TTL.

New library behaviour (e.g. a node restart) usually also requires new logic in `run()`, e.g. re-running `ic_init` on a node at time `t`, and extending the checks with the semantics of that case.

After adding a scenario, update the table in `docs/testing.md`.

The test build is defined in `cmake/sim_tests.cmake` (not in `tests/`). A new test source file goes there.

## 6. Static analysis (optional)

`cmake --build "build/INTERCOM[Debug]" --target cppcheck-analyze` (also `misra-analyze`, `codeql-analyze`). Do not let a change add new Cppcheck findings in `src/`.

## 7. Code size (optional)

When `arm-none-eabi-gcc` is available:

```sh
mkdir -p build/cfg && cp config/IntercomConfig_template.h build/cfg/IntercomConfig.h
for f in src/*.c; do arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -std=c99 -Iinclude -Ibuild/cfg -c $f -o build/$(basename $f).o; done
arm-none-eabi-size build/*.o
```

v1.0.0 reference point: about 6.6 KB of text and 1668 B of bss. Record any significant growth in `docs/architecture.md` (Memory section) and in `docs/testing.md`.
