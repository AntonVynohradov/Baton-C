# Testing

The tests run on a host computer (Linux, macOS, or Windows with MinGW). They simulate a bus with several nodes in one process: each node is a separate `ic_t` instance.

## Running

```sh
cmake --preset "INTERCOM[SimTests]"
cmake --build "build/INTERCOM[SimTests]"
ctest --test-dir "build/INTERCOM[SimTests]" -L sim --output-on-failure
# or directly, with a chosen random seed:
"./build/INTERCOM[SimTests]/intercom_sim_test" 12345
# many seeds (prints only the failing ones):
tests/run_seeds.sh "build/INTERCOM[SimTests]" 100
# Windows: .\tests\run_seeds.ps1 -BuildDir "build/INTERCOM[SimTests]" -Count 100
```

The same steps are available as VS Code tasks (`.vscode/tasks.json`, "Tests: ..."). Without presets: `cmake -S . -B build -DINTERCOM_BUILD_TESTS=ON`.

With tests enabled (`INTERCOM_BUILD_TESTS=ON`) the library is compiled with `-Wall -Wextra -Wpedantic -Wconversion -Werror` and with its own configuration from `tests/config/IntercomConfig.h`. The `INTERCOM[SimTestsSanitize]` preset (`INTERCOM_SANITIZERS=ON`) adds AddressSanitizer and UBSan; it needs GCC or Clang on Linux/macOS (MinGW does not ship the sanitizer runtimes).

## Static analysis

The top-level CMake project defines analysis targets when the tools are installed:

| Target | Tool | Report |
| --- | --- | --- |
| `cppcheck-analyze` | Cppcheck, all checks (paths in `cppcheck-excludes.txt` skipped) | `build/<preset>/cppcheck.txt` |
| `misra-analyze` | Cppcheck MISRA C:2012 addon; rule texts via `-DINTERCOM_MISRA_RULE_TEXTS=<file>` | `build/<preset>/misra.txt` |
| `codeql-analyze` | CodeQL `cpp-queries` on the library only | `build/<preset>/codeql-report.sarif` |

```sh
cmake --preset "INTERCOM[Debug]"
cmake --build "build/INTERCOM[Debug]" --target cppcheck-analyze
```

## Bus model

- Bytes are transmitted in real time for 9600 bit/s (1041 µs per byte), and the simulation steps in 50 µs increments.
- Every byte reaches all powered nodes, including the sender (echo). RS-485 mode disables the echo and calls `Intercom_TxDoneIsr()`.
- Overlapping transmissions are counted as collisions, and the test requires zero.
- Noise flips a random bit in a byte with a given probability.
- `Intercom_Process()` is called every 500 µs with a random phase offset on each node.

## What is checked

- Every event arrives **exactly once** and **in order** for each sender, recipient and priority. The payload carries a sequence number, and the rest of the data is a pattern verified on receipt.
- After traffic generation stops and the queues have had time to drain, **everything** accepted by `Intercom_Send()` arrives.
- With a dead node: every event that a live node did not receive has been reported through `onDrop`. There are no silent losses.
- States never go backwards, and the last published value reaches every node.
- No collisions on the bus.
- A slot, measured from the start of POLL to `onPollDone`, does not exceed the budget.
- API tests: `Intercom_Init` validation, `Send`/`Publish` error codes, queue full.
- Queue fuzz: 200,000 random allocate and release operations with no overlapping records.

## Scenarios

| Scenario | Nodes | Noise | Notes |
| --- | --- | --- | --- |
| clean bus | 1M + 3S | – | Baseline |
| noisy bus | 1M + 3S | 0.3% of bytes | Retry, deduplication |
| 7 nodes, cycle 1 s | 1M + 6S | 0.1% | `cyclePeriodMs`, 2 retries |
| overload | 1M + 3S | – | Constant `ERR_FULL`; no losses after draining |
| dead slave | 1M + 3S | – | One slave powered off, TTL 2 s |
| dead slave + noise | 1M + 4S | 0.2% | TTL 3 s |
| no echo (RS-485) | 1M + 3S | 0.2% | `rxEcho = false` |
| very noisy bus | 1M + 3S | 1% of bytes | About 20% of slots without a response |

Status at release: all scenarios pass on 100 seeds, also with AddressSanitizer and UBSan.

## Code size

Approximately, for Cortex-M3, `-Os`, the template configuration: about 6.6 KB of flash and 1668 B of RAM.
