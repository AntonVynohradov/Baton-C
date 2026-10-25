# Getting Started

This chapter shows how to get Baton-C, add it to your project, and build, test and
analyze it.

## Get the library

```bash
git clone https://github.com/AntonVynohradov/Baton-C.git
```

The usual way to use it is as a Git submodule of the firmware project:

```bash
git submodule add https://github.com/AntonVynohradov/Baton-C.git lib/Baton-C
```

Each [release](https://github.com/AntonVynohradov/Baton-C/releases) provides the source
code, a prebuilt static library for Linux x86-64 with its headers (built with the
configuration template), the MISRA report and the PDF user manual.

## Add it to your project

**1. Configuration.** Copy `config/baton_user_config_template.h` into your project as
`baton_user_config.h`, define your message lists and, if needed, the sizes. Every node of
a bus must use the same message lists. Do not edit the template inside the submodule.

**2a. CMake.**

```cmake
set(BATON_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)   # folder with baton_user_config.h
include(lib/Baton-C/baton.cmake)
target_link_libraries(app PRIVATE baton)
```

**2b. Any other build system** (vendor IDE, Make). Compile `src/*.c` with C11 and add the
repository root, `inc/` and the folder of your `baton_user_config.h` to the include path
of the library and of the application.

**3. Code.** Include `baton.h`, fill a `struct baton_node_config` and follow
[Using the Library](usage.md).

The prebuilt package of a release is compiled with the configuration template; it is
useful for host tools with the same message lists. Firmware normally builds the sources
with its own configuration.

## Prerequisites for development

- C11 compiler (GCC or Clang), CMake 3.20+, Make
- Unit tests: GoogleTest and gcovr
- Integration tests: Python 3 with pytest, and `gcc` on the path for the cross-check
- Analysis: Cppcheck (with its `misra.py` addon) and optionally the CodeQL CLI
- Formatting: clang-format
- Documentation: the packages of `requirements-dev.txt` (MkDocs Material, mkdoxy,
  WeasyPrint), Doxygen, and Pango for WeasyPrint

## Build with presets

| Preset | Purpose |
| --- | --- |
| `BATON[Debug]` | Static library with debug information (configuration template) |
| `BATON[Release]` | Optimized static library |
| `BATON[UnitTests]` | GoogleTest executable with coverage, bus simulation runner; target `coverage` |
| `BATON[IntegrationTests]` | pytest suite; target `integration_tests` |
| `BATON[Examples]` | Host examples and the bare-metal simulation test |
| `BATON[Sanitize]` | Like `UnitTests`, the simulation runner built with ASan and UBSan (Linux) |

```bash
cmake --preset "BATON[Debug]" && cmake --build "build/BATON[Debug]"
```

| CMake option | Default | Effect |
| --- | --- | --- |
| `BATON_CONFIG_DIR` | — | Folder with `baton_user_config.h` (required when included by an application) |
| `BATON_UNIT_TESTS` | `OFF` | GoogleTest executable, `coverage` target, `baton_sim` |
| `BATON_INTEGRATION_TESTS` | `OFF` | pytest through CTest, `integration_tests` target |
| `BATON_EXAMPLES` | `OFF` | Host examples |
| `BATON_SANITIZERS` | `OFF` | ASan/UBSan for `baton_sim` |
| `BATON_COVERAGE_MIN` | `100` | Minimum line and branch coverage of `coverage` |
| `BATON_MISRA_FAIL_ON_VIOLATION` | `ON` | `misra-analyze` fails on a violation |
| `BATON_MISRA_RULE_TEXTS` | — | Optional MISRA rule texts file for readable messages |

## Unit tests and coverage

```bash
cmake --preset "BATON[UnitTests]"
cmake --build "build/BATON[UnitTests]" --target coverage
```

The 132 GoogleTest tests cover the codec, the link, the queue and both state machines,
plus the bus simulation: several nodes on a simulated single-wire bus with the real byte
time of 9600 bit/s, bit errors, a dead node and an RS-485 variant without echo. Each
scenario checks exactly-once in-order delivery, final state delivery, no collisions and
the slot length. The `coverage` target fails below `BATON_COVERAGE_MIN` (100 %) line or
branch coverage of `src/`; the HTML report is in `build/BATON[UnitTests]/Coverage/`.

The same simulation runs optimized for many seeds:

```bash
cmake --build "build/BATON[UnitTests]" --target baton_sim
tests/run_seeds.sh "build/BATON[UnitTests]" 100            # Windows: tests\run_seeds.ps1
```

## Integration tests

```bash
cmake --preset "BATON[IntegrationTests]"
cmake --build "build/BATON[IntegrationTests]" --target integration_tests
```

`tests/integration/baton/protocol.py` is an independent Python implementation of the
codec using only the standard library (the *Python twin*). The tests check the reference
frames of the [protocol chapter](protocol.md#reference-frames), and compile
`src/baton_protocol.c` with `gcc` to compare 300 random frames between C and Python —
encoding, parsing, and the reaction to a corrupted byte. Without `gcc` the cross-check is
skipped and pytest says so. When CMake finds a Python without pytest, pass
`-DPython3_EXECUTABLE=<python with pytest>`.

## Static analysis

| Target | Tool | Gate |
| --- | --- | --- |
| `misra-analyze` | Cppcheck MISRA C:2012 addon (`cmake/misra_check.cmake`) | Fails on any violation |
| `cppcheck-analyze` | Cppcheck, all checks, `cppcheck-excludes.txt` | Report in `cppcheck.txt` |
| `codeql-analyze` | CodeQL `cpp-queries` | SARIF report |

```bash
cmake --build "build/BATON[Debug]" --target misra-analyze
```

The analysis uses `tests/config/baton_user_config.h`. See [MISRA C:2012](misra.md) for the
status and the deviation register.

## Formatting

```bash
clang-format -i baton.h src/*.c inc/*.h tests/unit/*.cpp tests/unit/*.h tests/sim/*.[ch] examples/*/*.[ch]
```

`.clang-format` is LLVM-based: 4 spaces, Allman braces, 100 columns, pointer to the type.
The *Format Check* workflow fails a pull request that changes under clang-format.

## Versioning and releases

The version comes from the latest `vX.Y.Z` Git tag (`cmake/version.cmake`); without a
tag, `BATON_DEFAULT_VERSION` is used. `baton.h` has the same fallbacks for builds without
CMake, and a unit test checks that `BATON_VERSION` matches the numbers. A release:

1. Set `BATON_DEFAULT_VERSION` and the fallbacks in `baton.h` to X.Y.Z.
2. Move the `[Unreleased]` entries of `CHANGELOG.md` to `## [X.Y.Z] - YYYY-MM-DD`.
3. Merge to `main`; the maintainer tags and pushes `vX.Y.Z`.

The *Release* workflow checks that the tag, `baton.h`, `cmake/version.cmake` and the
changelog agree, runs every gate (coverage, integration, MISRA, examples), builds the
package, the MISRA report and the PDF manual, and publishes the GitHub release.

## Documentation

| Task | Command | VS Code task |
| --- | --- | --- |
| Live preview | `mkdocs serve` | *Docs: Serve on localhost* |
| Build the website | `mkdocs build` | *Docs: Build site* |
| Build the PDF manual | `python tools/docs/build_manual.py` | *Docs: Build PDF manual* |
| Regenerate diagrams | `python tools/docs/diagrams.py` | *Docs: Generate diagrams* |
| Re-measure the stack | `python tools/docs/stack_usage.py <dir>` | — |
