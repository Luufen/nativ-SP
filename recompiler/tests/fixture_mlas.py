#!/usr/bin/env python3
"""Exercise the ARM flag-setting multiply-accumulate with synthetic code."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

RECOMPILER = Path(__file__).resolve().parents[1]
ENTRY = 0x81000000
CODE = struct.pack("<II", 0xE0314392, 0xE12FFF1E)  # mlas r1,r2,r3,r4; bx lr
MODULE = {
    "blocks": [{"start": ENTRY, "size": len(CODE), "init": True, "bytes": CODE.hex()}],
    "functions": {"main": ENTRY},
    "bodies": [[ENTRY, ENTRY + len(CODE) - 1, ENTRY]],
    "module": "fixture",
    "relocs": [],
    "exports": [],
    "variables": [],
    "arm": [ENTRY],
}

with tempfile.TemporaryDirectory(prefix="nativ-mlas-") as temp:
    work = Path(temp)
    data = work / "fixture.json"
    generated = work / "game.c"
    data.write_text(json.dumps(MODULE), encoding="utf-8")
    subprocess.run([sys.executable, str(RECOMPILER / "recomp.py"), str(data), str(generated)], check=True)
    assert "UNIMPL(c," not in generated.read_text(encoding="utf-8")
    harness = work / "run.c"
    harness.write_text(r'''
#include "rt.h"
#include <assert.h>

uint8_t *recomp_mem;
uint32_t recomp_watch_lo, recomp_watch_hi;
void fn_81000000(Cpu *cpu);
void recomp_enter(Cpu *cpu, uint32_t address) {}
void recomp_watch_write(uint32_t address, uint32_t value, int width) { assert(0); }
void recomp_call_external(Cpu *cpu, uint32_t address) { assert(0); }
void recomp_unimplemented(Cpu *cpu, uint32_t address) { assert(0); }
RecompJmp *recomp_setjmp_begin(Cpu *cpu) { return 0; }

static void check(uint32_t a, uint32_t b, uint32_t acc, uint32_t result, int n, int z, int c, int v) {
    Cpu cpu = {0};
    cpu.r[2] = a;
    cpu.r[3] = b;
    cpu.r[4] = acc;
    cpu.n = !n;
    cpu.z = !z;
    cpu.c = c;
    cpu.v = v;
    fn_81000000(&cpu);
    assert(cpu.r[1] == result);
    assert(cpu.n == n && cpu.z == z && cpu.c == c && cpu.v == v);
}

int main(void) {
    check(0xffffffffu, 2, 2, 0, 0, 1, 1, 0);
    check(0x40000000u, 2, 0, 0x80000000u, 1, 0, 0, 1);
    check(3, 4, 5, 17, 0, 0, 1, 1);
}
''', encoding="utf-8")
    binary = work / "run"
    subprocess.run(
        ["cc", "-O2", "-I", str(RECOMPILER), "-o", str(binary), str(harness), str(generated), "-lm"],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
    print("MLAS result and NZCV passed for three cases")
