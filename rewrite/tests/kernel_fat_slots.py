"""Differentially execute the recovered FAT directory/cluster scans.

All four bodies run as original x86, ordinary linked C, and fixed-address C.
Installed runs additionally compare against the source-built production image.
The tests exercise the legacy two-pass directory rule and signed FAT bounds.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path[:0] = [str(ROOT), str(BUILD / "test-deps")]
from tools.kernel_image import Fat16, ORIGINAL_SHA256
from tests.kernel_x_loader import FLAGS, run
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP)

SLOTS = [
    ("fat_root_empty", "kernel_fat_root_empty_82798c", 0x82798c, 0x827a40),
    ("fat_directory_empty", "kernel_fat_directory_empty_827a40", 0x827a40, 0x827ace),
    ("fat_find_free_cluster", "kernel_fat_find_free_cluster_827ace", 0x827ace, 0x827b44),
    ("fat_find_free_after", "kernel_fat_find_free_after_827b44", 0x827b44, 0x827bba),
]
BASE, BPB, FAT, DIRECTORY, STACK, STOP = 0x800000, 0x520000, 0x500000, 0x540000, 0x40f000, 0x200000


def compile_variants(original, installed):
    obj = BUILD / "kernel_fat_slots_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/fat_slots.c", "-o", str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = "fixed" if fixed else "ordinary"
        script, elf = BUILD / f"fat_slots_{label}.ld", BUILD / f"fat_slots_{label}.elf"
        sections = "\n".join(
            f".{name} {start:#x} : {{ *(.text.{symbol}) }}\n"
            f"ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
            for name, symbol, start, end in SLOTS) if fixed else ". = 0x300000; .text : { *(.text*) }"
        script.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_fat_root_empty_82798c)\nSECTIONS {\n" +
            sections + "\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n" +
            ".unexpected : { *(*) }\nASSERT(SIZEOF(.unexpected)==0, \"unexpected section\")\n}\n")
        run(["ld", "-m", "elf_i386", "-T", str(script), "-o", str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
                   for line in run(["nm", str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            candidate = bytearray(original)
            for name, symbol, start, end in SLOTS:
                path = BUILD / f"fat_slots_preview.{name}.bin"
                run(["objcopy", "-O", "binary", "-j", "." + name, str(elf), str(path)])
                body = path.read_bytes()
                assert 0 < len(body) <= end - start
                candidate[start-BASE:end-BASE] = body + b"\x90" * (end-start-len(body))
                sizes[name] = {"compiled_bytes": len(body), "slot_bytes": end-start}
            if installed:
                production = (BUILD / "openicestaros_kernel.x").read_bytes()
                for _, _, start, end in SLOTS:
                    assert production[start-BASE:end-BASE] == candidate[start-BASE:end-BASE]
                candidate = bytearray(production)
            path = BUILD / "fat_slots_candidate.x"
            path.write_bytes(candidate)
            outputs.append(("fixed", bytes(candidate), BASE,
                            [symbols[symbol] for _, symbol, _, _ in SLOTS]))
        else:
            path = BUILD / "fat_slots_ordinary.bin"
            run(["objcopy", "-O", "binary", str(elf), str(path)])
            outputs.append(("ordinary", path.read_bytes(), 0x300000,
                            [symbols[symbol] for _, symbol, _, _ in SLOTS]))
    return outputs, sizes


class Machine:
    def __init__(self, label, binary, base, entries):
        self.label, self.entries = label, entries
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(0, 0x1000000)
        self.uc.mem_write(base, binary)
        self.uc.hook_add(UC_HOOK_CODE, self.finish, begin=STOP, end=STOP)

    def finish(self, uc, address, size, unused):
        self.done = True
        uc.emu_stop()

    def execute(self, function, case):
        uc = self.uc
        self.done = False
        bpb = bytearray(512)
        struct.pack_into("<H", bpb, 11, case.get("bps", 512))
        struct.pack_into("<H", bpb, 14, case.get("reserved", 1))
        bpb[16] = case.get("fats", 2)
        struct.pack_into("<H", bpb, 17, case.get("entries", 8))
        struct.pack_into("<H", bpb, 22, case.get("fat_sectors", 16))
        uc.mem_write(BPB, bytes(bpb))
        uc.mem_write(FAT, b"\xff" * 0x20000)
        fat_pointer = case.get("fat_pointer", FAT)
        uc.mem_write(0x1a83a, struct.pack("<I", fat_pointer))
        if fat_pointer != FAT:
            uc.mem_write(fat_pointer, b"\xff" * 0x20000)

        root = (BPB + ((case.get("reserved", 1) + case.get("fats", 2) *
                        case.get("fat_sectors", 16)) * case.get("bps", 512))) & 0xffffffff
        root_states = case.get("root_states", [])
        directory_states = case.get("directory_states", [])
        root_bytes = bytearray(max(len(root_states), 1) * 32)
        for index, value in enumerate(root_states):
            root_bytes[index*32] = value
        uc.mem_write(root, bytes(root_bytes))
        directory = case.get("directory", DIRECTORY)
        directory_bytes = bytearray(max(len(directory_states), 1) * 32)
        for index, value in enumerate(directory_states):
            directory_bytes[index*32] = value
        uc.mem_write(directory, bytes(directory_bytes))

        fat_entries = case.get("fat_entries", {})
        for index, value in fat_entries.items():
            uc.mem_write(fat_pointer + index * 2, struct.pack("<H", value))

        args = [directory] if function == 1 else ([case.get("cluster", 0)] if function == 3 else [])
        uc.mem_write(STACK, struct.pack(f"<{len(args)+3}I", STOP, *args, 0x13579bdf, 0x2468ace0))
        saved = [(UC_X86_REG_EBX, 0x12345678), (UC_X86_REG_ESI, 0x87654321),
                 (UC_X86_REG_EDI, 0xabcdef12), (UC_X86_REG_EBP, 0x11223344)]
        for reg, value in [*saved, (UC_X86_REG_EAX, 0x99887766),
                           (UC_X86_REG_ECX, 0x55443322), (UC_X86_REG_EDX, 0xdeadbeef),
                           (UC_X86_REG_ESP, STACK)]:
            uc.reg_write(reg, value)
        uc.emu_start(self.entries[function], STOP+1, count=100000)
        assert self.done, (self.label, function, case, hex(uc.reg_read(UC_X86_REG_EIP)))
        assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4
        assert all(uc.reg_read(reg) == value for reg, value in saved), "cdecl callee-saved register changed"
        stack = bytes(uc.mem_read(STACK+4, len(args)*4+8))
        assert stack == struct.pack(f"<{len(args)+2}I", *args, 0x13579bdf, 0x2468ace0)
        return (uc.reg_read(UC_X86_REG_EAX), stack)


def make_cases():
    cases = []
    state_sets = [
        [], [0], [0x41], [0xe5], [0x41, 0xe5], [0xe5, 0, 0x41],
        [0xe5, 0x41, 0xe5, 0x41], [0x41, 0x41, 0xe5, 0x41, 0xe5],
    ]
    for function in (0, 1):
        key = "root_states" if function == 0 else "directory_states"
        for count in (0, 1, 2, 4, 8, 17, 512):
            for states in state_sets:
                bounded = (states + [0x41] * count)[:count]
                cases.append((function, {"entries": count, key: bounded}))
        cases.extend((function, {"entries": count, key: states})
                     for count, states in ((8, [0xe5, 0, 0x41]), (8, [0x41]*8),
                                           (8, [0x41, 0xe5, 0x41, 0xe5]),
                                           (1, [0xe5]), (1, [0x41])))
    for function in (2, 3):
        for fat_sectors, bps in ((0, 512), (1, 512), (4, 512), (16, 512),
                                 (1, 128), (4, 1024), (0xffff, 0xffff),
                                 (0xffff, 0x8000)):
            count = ((fat_sectors * bps) & 0xffffffff)
            signed = count if count < 0x80000000 else count - 0x100000000
            limit = int(signed / 2)  # truncates toward zero like IDIV/SAR correction
            probes = [2, 3, 7, 31]
            for free in probes:
                entries = {index: 0xfff8 for index in range(2, min(max(limit, 2), 40))}
                entries[free] = 0
                cases.append((function, {"fat_sectors": fat_sectors, "bps": bps,
                    "fat_entries": entries, "cluster": max(0, free-1),
                    "expected": free if function == 2 or free > max(0, free-1)+1 else None}))
        for cluster in (0, 1, 2, 6, 7, 30, 31, 0x7ffffffe):
            # A high starting cluster exits at the signed FAT bound before
            # dereferencing its entry.  Do not seed an intentionally
            # unreachable entry outside the emulated address space.
            entries = {} if cluster == 0x7ffffffe else {cluster + 1: 0}
            cases.append((3, {"cluster": cluster, "fat_sectors": 4, "bps": 512,
                              "fat_entries": entries}))
        cases.append((2, {"fat_sectors": 4, "bps": 512,
                          "fat_entries": {2: 0xfff8, 3: 0, 4: 0}}))
        cases.append((3, {"cluster": 2, "fat_sectors": 4, "bps": 512,
                          "fat_entries": {3: 0xfff8, 4: 0}}))
        cases.append((2, {"fat_sectors": 0xffff, "bps": 0xffff,
                          "fat_entries": {2: 0}}))
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    started = time.monotonic()
    source = (ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()
    original = Fat16(source).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    variants, sizes = compile_variants(original, args.installed)
    specs = [("original", original, BASE, [slot[2] for slot in SLOTS]), *variants]
    machines = [Machine(*spec) for spec in specs]
    cases = make_cases()
    for case_number, (function, case) in enumerate(cases):
        expected = machines[0].execute(function, case)
        for machine in machines[1:]:
            actual = machine.execute(function, case)
            assert actual == expected, (case_number, function, case, machine.label, expected, actual)
    summary = {
        "installed": args.installed,
        "scenarios": len(cases),
        "executions": len(cases) * len(machines),
        "functions": [hex(slot[2]) for slot in SLOTS],
        "slots": sizes,
        "seconds": round(time.monotonic() - started, 3),
        "sha256": {
            path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
            for path in ("kernel/fat_slots.c", "kernel/fat_slots.h", "tests/kernel_fat_slots.py")
        },
    }
    (BUILD / "fat_slots_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
