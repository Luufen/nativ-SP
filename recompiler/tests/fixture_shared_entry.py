#!/usr/bin/env python3
"""An interior function pointer keeps its own entry and hook while sharing the host body."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


RECOMPILER = Path(__file__).resolve().parents[1]
RECOMP = Path(sys.argv[1]) if len(sys.argv) > 1 else RECOMPILER / "recomp.py"
ENTRY = 0x81000000
ALIAS = ENTRY + 2
BRANCH_TARGET = ENTRY + 4
BRANCH = ENTRY + 8
CODE = struct.pack("<5H", 0x2005, 0x3001, 0x4770, 0xBF00, 0xE7FC).ljust(0x48, b"\x00")  # movs; adds; bx lr; gap; b to bx
POINTERS = struct.pack("<II", ALIAS | 1, BRANCH | 1)
MODULE = {
    "blocks": [
        {"start": ENTRY, "size": len(CODE), "init": True, "bytes": CODE.hex(), "name": ".text"},
        {"start": ENTRY + 0x100, "size": len(POINTERS), "init": True, "bytes": POINTERS.hex()},
    ],
    "functions": {"main": ENTRY},
    "bodies": [[ENTRY, BRANCH_TARGET + 1, ENTRY]],
    "module": "fixture",
    "relocs": [],
    "exports": [],
    "variables": [],
    "arm": [],
}

with tempfile.TemporaryDirectory(prefix="nativ-shared-entry-") as temp:
    work = Path(temp)
    data = work / "fixture.json"
    data.write_text(json.dumps(MODULE), encoding="utf-8")
    subprocess.run(
        [sys.executable, str(RECOMP), str(data), str(work / "game.c"), "--split", "2",
         "--hooks", f"{ALIAS:#x}:alias"],
        check=True, stdout=subprocess.PIPE,
    )
    parts = sorted(work.glob("game_part*.c"))
    generated = "".join(part.read_text(encoding="utf-8") for part in parts)
    harness = work / "run.c"
    harness.write_text(r'''
#include "game.h"
#include <assert.h>

uint8_t *recomp_mem;
uint32_t recomp_watch_lo, recomp_watch_hi;
static uint32_t entered;
static unsigned entry_count, hook_count;

void recomp_enter(Cpu *cpu, uint32_t address) {
    entered = address;
    entry_count++;
}
void recomp_hook_alias(Cpu *cpu) {
    assert(entered == 0x81000002u);
    cpu->r[0] = 7;
    hook_count++;
}
void recomp_watch_write(uint32_t addr, uint32_t value, int width) { assert(0); }
void recomp_call_external(Cpu *cpu, uint32_t address) { assert(0); }
void recomp_unimplemented(Cpu *cpu, uint32_t address) { assert(0); }
RecompJmp *recomp_setjmp_begin(Cpu *cpu) { return 0; }

int main(void) {
    static Cpu cpu;
    recomp_lookup(0x81000000u)(&cpu);
    assert(entered == 0x81000000u && cpu.r[0] == 6 && entry_count == 1 && hook_count == 0);
    recomp_lookup(0x81000002u)(&cpu);
    assert(entered == 0x81000002u && cpu.r[0] == 8 && entry_count == 2 && hook_count == 1);
    cpu.r[14] = 0x12345679u;
    GuestFn branch = recomp_lookup(0x81000008u);
    assert(branch);
    branch(&cpu);
    assert(entered == 0x81000004u && entry_count == 4 && hook_count == 1);
    assert(cpu.r[0] == 8 && cpu.r[14] == 0x12345679u);
}
''', encoding="utf-8")
    binary = work / "run"
    subprocess.run(
        ["cc", "-O2", "-DRECOMP_VERIFY", "-I", str(RECOMPILER), "-o", str(binary),
         str(harness), str(work / "game.c"), *map(str, parts), "-lm"],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
    assert generated.count(f"L_{ALIAS:08x}:") == 1, "alternate entry emitted a duplicate body"
    print("Pointer and branch entries share one translated body, preserving hooks and LR")
