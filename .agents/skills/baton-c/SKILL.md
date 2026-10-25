---
name: baton-c
description: "Use when working on the Baton-C library: deterministic master/slave messaging over a single-wire UART — the C codec (CRC-16, parser, encoder), the half-duplex link, the event queue, the master/slave node, the Python twin, the bus simulation, Baton Inspector, the examples (bare metal, Zephyr, host serial), MISRA C:2012 compliance, releases, or integrating the library into firmware or host tools."
---

# Baton-C: deterministic master/slave messaging over a single-wire UART

## Scope

This repository is a portable C11 static library plus a Python twin of the codec and
the Baton Inspector tool. The public API in `baton.h` and `inc/` follows Semantic
Versioning. The library implements the Baton bus protocol (`docs/protocol.md`): the
wire format, the half-duplex link and the master/slave node. Message meaning, data
layouts, UART drivers, threads and interrupt setup belong to the applications.

| Part | Files | Rule |
| --- | --- | --- |
| Umbrella | `baton.h` | Includes all public headers; fallback `BATON_VERSION_*` |
| Configuration | `inc/baton_config.h`, application `baton_user_config.h` | Sizes `#ifndef`-guarded and range-checked; message lists (X-macros) from the application |
| Definitions | `inc/baton_defs.h` | Wire constants and `static inline` helpers only |
| Codec | `inc/baton_protocol.h`, `src/baton_protocol.c` | CRC-16/CCITT-FALSE, incremental parser, in-place encoder |
| Link | `inc/baton_link.h`, `src/baton_link.c` | RX ring, inter-byte timeout, bus gap, echo check, TX timeout; reports events |
| Queue | `inc/baton_queue.h`, `src/baton_queue.c` | Variable-size event records with a serialized header |
| Node | `inc/baton_node.h`, `src/baton_node.c` | Master/slave state machines, TLV packing, stop-and-wait, dedup, TTL, application API |
| Python | `tests/integration/baton/protocol.py` | Standard library only; must stay byte-identical to the C codec |
| Simulation | `tests/sim/` | Bus simulator: unit-test scenarios and the `baton_sim` multi-seed runner |
| Build | `CMakeLists.txt`, `baton.cmake`, `cmake/`, `CMakePresets.json` | Library target in `baton.cmake`; tests and analysis in `cmake/` |
| Examples | `examples/bare_metal`, `examples/host_serial`, `examples/zephyr` | Not library code; built with `BATON_EXAMPLES=ON` (Zephyr with `west`) |
| Tools | `tools/baton_inspector` | Baton Inspector GUI and Python test API; its native bridge links the library |

## Hard rules

- The library sources include only `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`,
  `<string.h>` and Baton headers. No heap, no RTOS, no driver headers, no global
  mutable state, no clock reads (time comes in as `now_ms`), no blocking.
- Public symbols use the `baton_` / `BATON_` prefix; structures are `struct baton_…`.
  Public headers keep the `extern "C"` guards and Doxygen comments for every public item.
- Keep the public API and the wire format stable within a major version. Record every
  user-visible change under `[Unreleased]` in `CHANGELOG.md`.
- The version comes from the `vX.Y.Z` Git tag (`cmake/version.cmake`); keep the
  fallbacks `BATON_VERSION_*` and `BATON_VERSION` in `baton.h` equal to
  `BATON_DEFAULT_VERSION` there.
- MISRA C:2012: `misra-analyze` must report 0 violations (it fails the build
  otherwise). An unavoidable violation needs an entry in the deviation register in
  `docs/misra.md` and a `cppcheck-suppress` comment naming it. Library functions have a
  single `return` at the end, parenthesize mixed-operator expressions, cast ignored
  return values to `(void)`, keep `++`/`--` as separate statements, index arrays
  instead of doing pointer arithmetic, and do not cast between object pointer types.
- Every public function validates its pointer arguments and lengths (Dir 4.11). Do not
  add code that cannot be reached: 100 % branch coverage is a gate; remove such a branch
  and document the invariant in a comment instead.
- The protocol invariants N1-N22 (slot budget, stop-and-wait, sequencing of frames with
  content only, FIFO packing, queue release) are listed in
  `.claude/skills/baton-core-change/SKILL.md`; a change that breaks one must be
  discussed first.
- Code must compile without warnings with `-std=c11 -Wall -Wextra -Wconversion -Wpedantic`.
- Style: `.clang-format` (4 spaces, Allman braces, 100 columns); file banners and
  section comments as in the existing files (`.vscode/cpp.json.code-snippets`).

## Tests (definition of done)

Every codec, link or node behavior change needs a test. All of these must pass:

```sh
cmake --preset "BATON[Debug]" && cmake --build "build/BATON[Debug]"
cmake --preset "BATON[UnitTests]" && cmake --build "build/BATON[UnitTests]" --target coverage
cmake --build "build/BATON[UnitTests]" --target baton_sim && tests/run_seeds.sh "build/BATON[UnitTests]" 100
cmake --preset "BATON[IntegrationTests]" && cmake --build "build/BATON[IntegrationTests]" --target integration_tests
cmake --build "build/BATON[Debug]" --target misra-analyze
cmake --preset "BATON[Examples]" && cmake --build "build/BATON[Examples]" && ctest --test-dir "build/BATON[Examples]" -L example
python -m pytest tools/baton_inspector/tests tools/baton_inspector/examples
```

The `coverage` target fails below 100 % line or branch coverage of `src/`
(`BATON_COVERAGE_MIN`); `misra-analyze` fails on any violation. The bus simulation
(`tests/sim/`) checks exactly-once in-order delivery, final state delivery, no
collisions and the slot length; run it on 100 seeds (with ASan/UBSan on Linux, preset
`BATON[Sanitize]`). The pytest suite in `tests/integration/` includes a cross-check
that compiles `src/baton_protocol.c` with `gcc` and compares 300 random frames against
the Python twin; it is skipped without `gcc`, so say so when reporting results on such
a machine.

Reference frames (`docs/protocol.md`) that must keep passing:
POLL `A5 11 01 20 01 00 7C 44`, ACK `A5 12 11 C3 00 45 5B`,
RESP `A5 01 11 60 02 80 00 C9 91`, DATA `A5 12 11 A0 04 10 02 AB CD D8 BA`.

## Documentation

The documentation is a MkDocs Material website (`mkdocs.yml`, pages in `docs/`) and a
printable PDF manual built from the same pages (`python tools/docs/build_manual.py`;
chapter order = `mkdocs.yml` nav, `index.md` and the Doxygen `reference/` pages are
website-only). All documentation is in English.

| Page | Contents |
| --- | --- |
| `overview.md`, `guide.md` | Introduction, getting started, build, docs tooling |
| `architecture.md`, `usage.md`, `extending.md` | How it works, how to use it, how to extend it |
| `protocol.md`, `integration.md`, `examples.md`, `inspector.md` | Protocol specification, UART integration, examples, Baton Inspector |
| `api.md`, `configuration.md`, `misra.md` | API reference, configuration, memory, code size and stack usage, MISRA |
| `about.md`, `LICENSE.md`, `changelog.md` | Author and company, license, changelog (included from `CHANGELOG.md`) |

Update the affected pages, `README.md` and `CHANGELOG.md` together with any API or
behavior change; re-measure the size and stack tables (`tools/docs/stack_usage.py`)
when a function's frame changes, and compile the C snippets of the pages
(`python tools/docs/check_snippets.py`). The documentation tooling is the shared base of
the personal `sfs-protocol-library` skill (SystemFromScratch® protocol libraries); keep
it identical to the skill's templates and to DXL2-C and change project data, not
structure: the landing page `overrides/home.html` is driven by
`mkdocs.yml → extra.landing`, the PDF builder `tools/docs/build_manual.py` by
`extra.manual`, and `docs/index.md` only holds the Quick start Markdown. Diagrams,
including the animated isometric hero and the README banner, are generated by
`tools/docs/diagrams.py` with `tools/docs/svgkit.py`: edit the script and run it, never
edit the SVG files. Links between pages are relative `.md` links (the PDF builder turns
them into page references). Avoid Material-only markup (grid cards, icons, tabs)
outside `index.md` and `about.md`, because the PDF does not render it. Author: Anton
Vynohradov (github.com/AntonVynohradov, linkedin.com/in/vynohradov), company
SystemFromScratch® (systemfromscratch.com).

## Releases

1. Set `BATON_DEFAULT_VERSION` in `cmake/version.cmake` and the fallbacks in `baton.h`
   to the new version.
2. Move the `[Unreleased]` entries in `CHANGELOG.md` to `[X.Y.Z] - date`.
3. Merge to `main`, then push the tag `vX.Y.Z`. The Release workflow
   (`.github/workflows/release.yml`) refuses a tag that does not match these files,
   runs all quality gates and publishes the library, headers, MISRA report and PDF
   manual as a GitHub Release.

Never push a tag or publish a release without the maintainer's explicit request.
