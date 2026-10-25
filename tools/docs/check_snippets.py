#******************************************************************************
# Baton-C: Deterministic Master/Slave Messaging over a Single-Wire UART
#
# Project:     Baton-C - Deterministic Master/Slave Messaging over a Single-Wire UART
# Description: Compiles every ```c block of docs/*.md against the library.
#
# Author:      Anton Vynohradov
# Email:       avynohradov@systemfromscratch.com
#
# License:     Apache License 2.0
#
# Copyright (c) 2026 Anton Vynohradov
#
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

"""Compile the C snippets of the documentation.

    python tools/docs/check_snippets.py [compiler]

Each ```c block of docs/*.md is compiled on its own with
-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror, after the stub header
below that declares the board functions the snippets stand in for. A snippet
passes when it compiles with the configuration template or with the example
configuration (the quick start uses the example messages).
"""

from __future__ import annotations

import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

STUBS = r"""
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "baton.h"
int board_uart_write(const uint8_t* data, size_t length);
uint32_t board_millis(void);
void board_wait_for_tick(void);
void motor_stop(uint8_t mode);
void mark_offline(uint8_t slave);
void log_unreachable(uint8_t slave, uint32_t nodes);
void log_value(const char* name, uint32_t value);
void print_frame(uint8_t src, uint8_t dst, uint8_t type, uint8_t seq, const uint8_t* payload,
                 uint8_t length);
bool usart_rx_not_empty(void);
uint8_t usart_read_byte(void);
bool usart_tx_empty(void);
void usart_write_byte(uint8_t byte);
extern const uint8_t* tx_buffer;
extern size_t tx_position;
extern size_t tx_length;
"""

# Definitions a snippet may use without defining them itself.
OPTIONAL = {
    "struct baton_node node": "struct baton_node node;\n",
    "uart_write(": ("static int uart_write(void* context, const uint8_t* data, size_t length)\n"
                    "{ (void) context; return board_uart_write(data, length); }\n"),
    "on_message(": ("static void on_message(void* context, uint8_t src, uint8_t msg_id,\n"
                    "    const uint8_t* data, uint8_t length)\n"
                    "{ (void) context; (void) src; (void) msg_id; (void) data; (void) length; }\n"),
}


def build_source(block: str) -> str:
    """Stubs, then what the block uses but does not define, then the block."""
    if "#define BATON_STATE_LIST" in block:
        return block + "typedef int baton_snippet_unit; /* a configuration fragment */"
    source = STUBS
    for key, definition in OPTIONAL.items():
        defined = re.search(r"^(static )?[\w ]*" + re.escape(key), block, re.M) is not None
        name = key.rstrip("(").split()[-1]
        if not defined and re.search(r"\b" + re.escape(name) + r"\b", block):
            source += definition
    return source + block.replace('#include "baton.h"', "")


def snippets():
    for path in sorted(glob.glob(os.path.join(ROOT, "docs", "*.md"))):
        with open(path, encoding="utf-8") as f:
            text = f.read()
        for i, block in enumerate(re.findall(r"```c\n(.*?)```", text, re.S)):
            yield os.path.relpath(path, ROOT), i + 1, block


def compiles(cc: str, source: str, config_dir: str, tmp: str) -> str:
    path = os.path.join(tmp, "snippet.c")
    with open(path, "w", encoding="utf-8") as f:
        f.write(source)
    result = subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion",
                             "-Werror", "-Wno-unused-function", "-Wno-unused-variable",
                             "-c", path, "-o", os.devnull,
                             "-I", ROOT, "-I", os.path.join(ROOT, "inc"), "-I", config_dir],
                            capture_output=True, text=True)
    return "" if result.returncode == 0 else result.stderr


def main() -> int:
    cc = sys.argv[1] if len(sys.argv) > 1 else "gcc"
    failures = 0
    count = 0
    with tempfile.TemporaryDirectory() as tmp:
        template = os.path.join(tmp, "template")
        os.makedirs(template)
        with open(os.path.join(ROOT, "config", "baton_user_config_template.h"),
                  encoding="utf-8") as f:
            content = f.read()
        with open(os.path.join(template, "baton_user_config.h"), "w", encoding="utf-8") as f:
            f.write(content)
        configs = [template, os.path.join(ROOT, "examples", "config")]
        for page, number, block in snippets():
            count += 1
            source = build_source(block)
            errors = [compiles(cc, source, c, tmp) for c in configs]
            if all(errors):
                failures += 1
                print(f"FAIL {page} block {number}\n{errors[0]}")
            else:
                print(f"ok   {page} block {number}")
    print(f"{count - failures}/{count} snippets compile")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
