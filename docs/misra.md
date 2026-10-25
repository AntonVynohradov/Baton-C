# MISRA C:2012 compliance

This page records how the library is checked against MISRA C:2012 and every deviation
from it. It covers the library code only: `src/`, `inc/` and `baton.h`. Tests, examples
and tools are not in scope.

## Checking

| Check | Where | Gate |
| --- | --- | --- |
| Cppcheck MISRA C:2012 addon | `misra-analyze` target, CI job *MISRA C:2012 Analysis* | Fails on any violation |
| Cppcheck, all checks | `cppcheck-analyze` target, CI job *Cppcheck Analysis* | Report |
| CodeQL `cpp-queries` | `codeql-analyze` target, CI job *CodeQL Analysis* | Report |
| Compiler | `-std=c11 -Wall -Wextra -Wpedantic -Wconversion`, `-Werror` in the test builds | No warnings |
| Manual review | Pull request review | Reviewer |

`misra-analyze` analyzes `src/*.c` with the configuration `tests/config/baton_user_config.h`
and fails when the report contains a violation. Set
`-DBATON_MISRA_FAIL_ON_VIOLATION=OFF` to get the report without failing. A violation
that cannot be fixed must be added to the deviation register below — and, when the
addon reports it, suppressed at that line with a `cppcheck-suppress` comment that names
the deviation.

The addon checks most decidable rules, not the directives or the rules that need the
whole program or the intent of the code; those are covered by the manual review notes.

## Current status

| Item | Result |
| --- | --- |
| Cppcheck MISRA addon (2.16.2) | 0 violations; 4 suppressed findings, all DEV-04 |
| Cppcheck, all checks (2.16.2) | 0 findings |
| Unit test coverage of `src/` | 100 % lines (1053), 100 % branches (601) |
| Deviations | 4, listed below |

## Manual review notes

| Guideline | How the library complies |
| --- | --- |
| Dir 4.1 (run-time failures) | Every array index is bounded: by a length check, by the configuration invariants checked with `#error` and `_Static_assert` (payload, message lengths, queue capacity), or by the parser's own range checks, which also catch a corrupted parser structure. Unsigned time differences wrap by design. |
| Dir 4.11 (validity of arguments) | Every public function checks its pointer arguments and lengths. The `static inline` helpers of `baton_defs.h` take a pointer to 2 or 4 readable bytes, like `memcpy()`. |
| Dir 4.12, Rule 21.3 (dynamic memory) | No heap. All state is in the caller-owned `struct baton_node`. |
| Rule 8.9, 8.4 (definitions) | Constant tables are `static const` at the narrowest scope. |
| Rule 11.3, 11.5 (pointer casts) | None: queue records are serialized byte by byte instead of casting the byte array to a structure. |
| Rule 15.5 (single point of exit) | Every function has one `return` at the end. |
| Rule 17.2 (recursion) | No recursion; the stack is fixed, see [Configuration](configuration.md#stack-usage). |
| Rule 21.6 (standard I/O) | No `<stdio.h>`. The library includes only `<stddef.h>`, `<stdint.h>`, `<stdbool.h>` and `<string.h>`. |

## Deviation register

### DEV-01: `int` as the status type (Dir 4.6, advisory)

- **Location:** return type of the functions that return a status (`baton_node_init()`,
  `baton_node_send()`, `baton_node_publish()`, `baton_parser_feed()`, `baton_encode()`,
  `baton_link_*()`), the `baton_write_fn` callback, and the internal `pack()` result.
- **Rationale:** the values are status codes, not quantities whose size matters. `int`
  lets an integrator pass an existing driver function as `baton_write_fn` without a
  wrapper. `int` has at least 16 bits on every conforming compiler, enough for every
  value used.
- **Risk control:** the library does no arithmetic on status values; it compares them with
  0 or with the named constants.

### DEV-02: macros not used inside the library (Rule 2.5, advisory)

- **Location:** public constants that only applications and tools use, for example
  `BATON_VERSION_*` in `baton.h` and the generated `BATON_MSG_<NAME>_MAX_LEN`.
- **Rationale:** they are the public names of protocol and API values.

### DEV-03: external functions referenced in one translation unit (Rule 8.7, advisory)

- **Location:** the public API functions that the library itself does not call, for
  example `baton_node_get_stats()` and `baton_crc16()`.
- **Rationale:** they are the library interface; internal linkage would remove them.

### DEV-04: application message lists as X-macros (Rules 20.7 required, 20.10 and Dir 4.9 advisory)

- **Location:** `BATON_STATE_LIST(X)` / `BATON_EVENT_LIST(X)` in the application's
  `baton_user_config.h` (Rule 20.7: the parameter `X` is used as a macro name, which
  cannot be parenthesized); `BATON_X_MSG_ENUM_` and `BATON_X_MSG_SIZE_` in
  `inc/baton_node.h` (Rule 20.10: `##` builds `BATON_MSG_<NAME>`); the list macros
  themselves are function-like macros (Dir 4.9).
- **Rationale:** the lists are the single definition of the messages of a bus. From them
  the library generates, at compile time, the message identifiers, the length table, the
  state mailbox layout, a `switch` whose duplicate `case` values reject duplicate IDs,
  and `_Static_assert` checks of every length and of the queue capacity. Writing these by
  hand would duplicate each message in several places, which is the error the rules aim
  to prevent.
- **Risk control:** the expansion is checked by the compiler (`-Wall -Wextra -Wpedantic
  -Wconversion -Werror` in the tests) and by the compile-time assertions; the generated
  code is covered by the unit tests; the suppressions name this deviation at each line
  (`cppcheck-suppress misra-c2012-20.7` / `20.10`). Applications that run their own MISRA
  analysis inherit this deviation for their `baton_user_config.h`.
