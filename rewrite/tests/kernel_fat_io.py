"""Differentially execute the recovered contiguous FAT sector writer.

The original ATA one-sector routine is intercepted at its real entry so the
tests compare the original body, ordinary linked C, and the fixed-address C
replacement without requiring privileged I/O.  The writer's unsigned end
calculation, live BPB stride, exact call order, ignored helper return value,
and cdecl register/stack contract are exercised.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
import sys
sys.path[:0] = [str(ROOT), str(BUILD / "test-deps")]
from tools.kernel_image import Fat16, ORIGINAL_SHA256
from tests.kernel_x_loader import FLAGS, run
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP)

SLOT = ("fat_write_sectors", "kernel_fat_write_sectors_827c72", 0x827c72, 0x827cc3)
HELPER = 0x828c18
BASE, BPB, DEST, STACK, STOP = 0x800000, 0x520000, 0x610000, 0x40f000, 0x200000

SOURCE_BYTES = bytes((index * 11 + (index >> 8) * 7) & 255 for index in range(0x200000))

def compile_variants(original, installed):
    obj = BUILD / "kernel_fat_io_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/fat_io.c", "-o", str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = "fixed" if fixed else "ordinary"
        script = BUILD / f"fat_io_{label}.ld"
        elf = BUILD / f"fat_io_{label}.elf"
        if fixed:
            sections = (f".{SLOT[0]} {SLOT[2]:#x} : "
                        f"{{ *(.text.{SLOT[1]}) }}\n"
                        f"ASSERT({SLOT[1]} == {SLOT[2]:#x} && "
                        f"SIZEOF(.{SLOT[0]}) <= {SLOT[3]-SLOT[2]}, \"slot overflow\")")
        else:
            sections = ". = 0x300000; .text : { *(.text*) }"
        script.write_text(
            "OUTPUT_FORMAT(\"elf32-i386\")\nENTRY(" + SLOT[1] + ")\n"
            f"PROVIDE(kernel_fat_write_sector_828c18 = {HELPER:#x});\n"
            "SECTIONS {\n" + sections +
            "\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
            ".unexpected : { *(*) }\n"
            "ASSERT(SIZEOF(.unexpected) == 0, \"unexpected section\")\n}\n")
        run(["ld", "-m", "elf_i386", "-T", str(script), "-o", str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
                   for line in run(["nm", str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            path = BUILD / "fat_io_preview.fat_write_sectors.bin"
            run(["objcopy", "-O", "binary", "-j", "." + SLOT[0], str(elf), str(path)])
            body = path.read_bytes()
            assert 0 < len(body) <= SLOT[3] - SLOT[2]
            candidate = bytearray(original)
            candidate[SLOT[2]-BASE:SLOT[3]-BASE] = body + b"\x90" * (SLOT[3]-SLOT[2]-len(body))
            sizes[SLOT[0]] = {"compiled_bytes": len(body), "slot_bytes": SLOT[3]-SLOT[2]}
            if installed:
                production = (BUILD / "openicestaros_kernel.x").read_bytes()
                assert production[SLOT[2]-BASE:SLOT[3]-BASE] == candidate[SLOT[2]-BASE:SLOT[3]-BASE]
                candidate = bytearray(production)
            path = BUILD / "fat_io_candidate.x"
            path.write_bytes(candidate)
            outputs.append((label, bytes(candidate), BASE, symbols[SLOT[1]]))
        else:
            path = BUILD / "fat_io_ordinary.bin"
            run(["objcopy", "-O", "binary", str(elf), str(path)])
            outputs.append((label, path.read_bytes(), 0x300000, symbols[SLOT[1]]))
    return outputs, sizes


class Machine:
    def __init__(self, label, binary, base, entry):
        self.label, self.entry = label, entry
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(0, 0x1000000)
        self.uc.mem_write(base, binary)
        self.uc.hook_add(UC_HOOK_CODE, self.finish, begin=STOP, end=STOP)
        self.uc.hook_add(UC_HOOK_CODE, self.sector, begin=HELPER, end=HELPER)

    def finish(self, uc, address, size, unused):
        self.done = True
        uc.emu_stop()

    def sector(self, uc, address, size, unused):
        sp = uc.reg_read(UC_X86_REG_ESP)
        lba, source = struct.unpack("<2I", uc.mem_read(sp + 4, 8))
        self.trace.append((lba, source))
        self.payloads.append(bytes(uc.mem_read(source, 512)))
        if self.case.get("bpb_mutation") and len(self.trace) == 1:
            uc.mem_write(BPB + 11, struct.pack("<H", self.case["mutated_bps"]))
        uc.reg_write(UC_X86_REG_EAX, self.case.get("helper_eax", 0xdeadbeef))
        uc.reg_write(UC_X86_REG_ECX, 0x11112222)
        uc.reg_write(UC_X86_REG_EDX, 0x33334444)
        uc.reg_write(UC_X86_REG_ESP, sp + 4)
        uc.reg_write(UC_X86_REG_EIP, struct.unpack("<I", uc.mem_read(sp, 4))[0])

    def execute(self, case):
        self.case, self.done, self.trace, self.payloads = case, False, [], []
        uc = self.uc
        uc.mem_write(0, bytes(0x20000))
        uc.mem_write(BPB, bytes(512))
        uc.mem_write(0x600000, SOURCE_BYTES)
        struct.pack_into("<H", (bpb := bytearray(512)), 11, case["bps"])
        uc.mem_write(BPB, bytes(bpb))
        args = (case["lba"], case["source"], case["count"])
        uc.mem_write(STACK, struct.pack("<4I", STOP, *args))
        saved = [(UC_X86_REG_EBX, 0x12345678), (UC_X86_REG_ESI, 0x87654321),
                 (UC_X86_REG_EDI, 0xABCDEF12), (UC_X86_REG_EBP, 0x11223344)]
        for reg, value in [*saved, (UC_X86_REG_EAX, 0x99887766),
                           (UC_X86_REG_ECX, 0x55443322), (UC_X86_REG_EDX, 0xCAFEBABE),
                           (UC_X86_REG_ESP, STACK)]:
            uc.reg_write(reg, value)
        uc.emu_start(self.entry, STOP + 1, count=100000)
        assert self.done, (self.label, case, hex(uc.reg_read(UC_X86_REG_EIP)))
        assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4
        assert all(uc.reg_read(reg) == value for reg, value in saved), "callee-saved register changed"
        assert uc.mem_read(STACK + 4, 12) == struct.pack("<3I", *args)
        # The helper intentionally changes caller-clobbered registers; the
        # recovered writer must not make a contract out of its return value.
        assert bytes(uc.mem_read(0x600000, len(SOURCE_BYTES))) == SOURCE_BYTES, "writer changed source"
        return self.trace, self.payloads


def cases():
    values = []
    for bps in (128, 256, 512, 1024, 4096, 0xffff):
        for lba in (0, 1, 29, 0x7fffffff):
            for count in (0, 1, 2, 3, 16):
                values.append(dict(bps=bps, lba=lba, count=count,
                                   source=DEST + (lba & 7) * 0x4000))
    # A wrapped end is below the starting LBA, so the original unsigned loop
    # performs no I/O.  Keep these explicit to prevent accidental saturation.
    for lba, count in ((0xffffffff, 1), (0xfffffff0, 0x20), (0xfffffffe, 3)):
        values.append(dict(bps=512, lba=lba, count=count, source=DEST))
    for mutated in (128, 256, 1024, 0xffff):
        values.append(dict(bps=512, lba=17, count=5, source=DEST,
                           bpb_mutation=True, mutated_bps=mutated,
                           helper_eax=0x01020304))
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    started = time.monotonic()
    source = (ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()
    original = Fat16(source).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    variants, sizes = compile_variants(original, args.installed)
    specs = [("original", original, BASE, SLOT[2]), *variants]
    machines = [Machine(*spec) for spec in specs]
    all_cases = cases()
    calls = 0
    for number, case in enumerate(all_cases):
        expected = machines[0].execute(case)
        end = (case["lba"] + case["count"]) & 0xffffffff
        stride = case.get("mutated_bps", case["bps"])
        addresses = [(lba, case["source"] + index * stride)
                     for index, lba in enumerate(range(case["lba"], end))]
        assert expected[0] == addresses
        assert expected[1] == [SOURCE_BYTES[p-0x600000:p-0x600000+512] for _, p in addresses]
        calls += len(addresses)
        for machine in machines[1:]:
            actual = machine.execute(case)
            assert actual == expected, (number, case, machine.label, expected[0], actual[0])
    summary = dict(installed=args.installed, scenarios=len(all_cases), executions=len(all_cases)*3,
                   calls=calls, slots=sizes,
                   seconds=round(time.monotonic() - started, 3),
                   sha256={path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                           for path in ("kernel/fat_io.c", "kernel/fat_io.h", "tests/kernel_fat_io.py")})
    (BUILD / "fat_io_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
