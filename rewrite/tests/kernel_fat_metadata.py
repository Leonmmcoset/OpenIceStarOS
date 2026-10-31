"""Differentially execute the FAT/root metadata flush at 0x827cc4."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path[:0] = [str(ROOT), str(BUILD / "test-deps")]
from tools.kernel_image import Fat16, ORIGINAL_SHA256
from tests.kernel_x_loader import FLAGS, run
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_ERR_EXCEPTION
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP)

SLOT = ("fat_flush_metadata", "kernel_fat_flush_metadata_827cc4", 0x827cc4, 0x827d49)
WRITER = 0x827c72
BASE, BPB, FAT, STACK, STOP = 0x800000, 0x520000, 0x600000, 0x40f000, 0x200000

SOURCE_BYTES = bytes((index * 11 + (index >> 8) * 7) & 255 for index in range(0x10000))

def compile_variants(original, installed):
    obj = BUILD / "kernel_fat_metadata_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/fat_metadata.c", "-o", str(obj)])
    outputs, sizes = [], {}
    for fixed in (False, True):
        label = "fixed" if fixed else "ordinary"
        script = BUILD / f"fat_metadata_{label}.ld"
        elf = BUILD / f"fat_metadata_{label}.elf"
        if fixed:
            sections = (f".{SLOT[0]} {SLOT[2]:#x} : "
                        f"{{ *(.text.{SLOT[1]}) }}\n"
                        f"ASSERT({SLOT[1]} == {SLOT[2]:#x} && "
                        f"SIZEOF(.{SLOT[0]}) <= {SLOT[3]-SLOT[2]}, \"slot overflow\")")
        else:
            sections = ". = 0x300000; .text : { *(.text*) }"
        script.write_text(
            "OUTPUT_FORMAT(\"elf32-i386\")\nENTRY(" + SLOT[1] + ")\n"
            f"PROVIDE(kernel_fat_write_sectors_827c72 = {WRITER:#x});\n"
            "SECTIONS {\n" + sections +
            "\n/DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
            ".unexpected : { *(*) }\n"
            "ASSERT(SIZEOF(.unexpected) == 0, \"unexpected section\")\n}\n")
        run(["ld", "-m", "elf_i386", "-T", str(script), "-o", str(elf), str(obj)])
        symbols = {line.split()[-1]: int(line.split()[0], 16)
                   for line in run(["nm", str(elf)], capture_output=True, text=True).stdout.splitlines()}
        if fixed:
            path = BUILD / "fat_metadata_preview.fat_flush_metadata.bin"
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
            path = BUILD / "fat_metadata_candidate.x"
            path.write_bytes(candidate)
            outputs.append((label, bytes(candidate), BASE, symbols[SLOT[1]]))
        else:
            path = BUILD / "fat_metadata_ordinary.bin"
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
        self.uc.hook_add(UC_HOOK_CODE, self.writer, begin=WRITER, end=WRITER)

    def finish(self, uc, address, size, unused):
        self.done = True
        uc.emu_stop()

    def writer(self, uc, address, size, unused):
        sp = uc.reg_read(UC_X86_REG_ESP)
        self.args = struct.unpack("<3I", uc.mem_read(sp + 4, 12))
        self.trace.append(self.args)
        lba, source, count = self.args
        self.payloads.append(bytes(uc.mem_read(source, 64)))
        uc.reg_write(UC_X86_REG_EAX, self.case.get("writer_eax", 0xdeadbeef))
        uc.reg_write(UC_X86_REG_ECX, 0x11112222)
        uc.reg_write(UC_X86_REG_EDX, 0x33334444)
        uc.reg_write(UC_X86_REG_ESP, sp + 4)
        uc.reg_write(UC_X86_REG_EIP, struct.unpack("<I", uc.mem_read(sp, 4))[0])
        if self.case.get("mutate"):
            uc.mem_write(0x7dc6, struct.pack("<I", self.case["mutated_partition"]))
            uc.mem_write(0x1a83a, struct.pack("<I", self.case["mutated_fat"]))

    def execute(self, case):
        self.case, self.done, self.trace, self.args, self.payloads = case, False, [], None, []
        uc = self.uc
        uc.mem_write(0, bytes(0x20000))
        uc.mem_write(BPB, bytes(512))
        uc.mem_write(FAT, SOURCE_BYTES)
        uc.mem_write(0x700000, SOURCE_BYTES)
        uc.mem_write(STACK - 0x1000, bytes(0x2000))
        bpb = bytearray(512)
        struct.pack_into("<H", bpb, 11, case["bps"])
        bpb[14] = case["reserved"] & 255
        bpb[15] = (case["reserved"] >> 8) & 255
        bpb[16] = case["fats"]
        struct.pack_into("<H", bpb, 17, case["entries"])
        struct.pack_into("<H", bpb, 22, case["fat_sectors"])
        uc.mem_write(BPB, bytes(bpb))
        uc.mem_write(0x7dc6, struct.pack("<I", case["partition"]))
        uc.mem_write(0x1a83a, struct.pack("<I", case["fat_pointer"]))
        uc.mem_write(STACK, struct.pack("<I", STOP))
        saved = [(UC_X86_REG_EBX, 0x12345678), (UC_X86_REG_ESI, 0x87654321),
                 (UC_X86_REG_EDI, 0xABCDEF12), (UC_X86_REG_EBP, 0x11223344)]
        for reg, value in [*saved, (UC_X86_REG_EAX, 0x99887766),
                           (UC_X86_REG_ECX, 0x55443322), (UC_X86_REG_EDX, 0xCAFEBABE),
                           (UC_X86_REG_ESP, STACK)]:
            uc.reg_write(reg, value)
        try:
            uc.emu_start(self.entry, STOP + 1, count=100000)
        except UcError as error:
            assert case.get("fault") and error.errno == UC_ERR_EXCEPTION, (self.label, case, error)
            assert not self.trace
            return ("fault", self.trace, self.payloads)
        assert not case.get("fault")
        assert self.done, (self.label, case, hex(uc.reg_read(UC_X86_REG_EIP)))
        assert uc.reg_read(UC_X86_REG_ESP) == STACK + 4
        assert all(uc.reg_read(reg) == value for reg, value in saved), "callee-saved register changed"
        assert uc.mem_read(STACK + 4, 4) == b"\0\0\0\0"
        assert bytes(uc.mem_read(FAT, len(SOURCE_BYTES))) == SOURCE_BYTES
        assert bytes(uc.mem_read(0x700000, len(SOURCE_BYTES))) == SOURCE_BYTES
        return (uc.reg_read(UC_X86_REG_EAX), self.trace, self.payloads)


def cases():
    values = []
    for bps in (1, 128, 512, 1024, 4096, 0xffff):
        for reserved in (0, 1, 17, 0xffff):
            for fats in (0, 1, 2, 255):
                for fat_sectors in (0, 1, 16, 0xffff):
                    for entries in (0, 1, 32, 512, 0xffff):
                        values.append(dict(bps=bps, reserved=reserved, fats=fats,
                            fat_sectors=fat_sectors, entries=entries,
                            partition=17, fat_pointer=FAT))
    values.extend([
        dict(bps=0, reserved=1, fats=2, fat_sectors=16, entries=512,
             partition=17, fat_pointer=FAT, fault=True),
        dict(bps=512, reserved=0x100, fats=2, fat_sectors=16, entries=512,
             partition=0xfffffff0, fat_pointer=FAT),
        dict(bps=512, reserved=1, fats=2, fat_sectors=16, entries=512,
             partition=17, fat_pointer=FAT, mutate=True,
             mutated_partition=0x1234, mutated_fat=0x680000, writer_eax=0x01020304),
        dict(bps=0xffff, reserved=0xffff, fats=255, fat_sectors=0xffff,
             entries=0xffff, partition=0xffffffff, fat_pointer=0x700000),
    ])
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
    writes = 0
    for number, case in enumerate(all_cases):
        current = [Machine(*spec) for spec in specs] if case.get("fault") else machines
        expected = current[0].execute(case)
        if not case.get("fault"):
            count = case["fats"] * case["fat_sectors"] + case["entries"] * 32 // case["bps"]
            parameters = ((case["partition"] + case["reserved"]) & 0xffffffff,
                          case["fat_pointer"], count)
            assert expected == (count, [parameters], [SOURCE_BYTES[:64]]), (case, expected)
            writes += 1
        for machine in current[1:]:
            actual = machine.execute(case)
            assert actual == expected, (number, case, machine.label, expected, actual)
    summary = dict(installed=args.installed, scenarios=len(all_cases), executions=len(all_cases)*3,
                   writes=writes, slots=sizes,
                   seconds=round(time.monotonic() - started, 3),
                   sha256={path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                           for path in ("kernel/fat_metadata.c", "kernel/fat_metadata.h",
                                        "tests/kernel_fat_metadata.py")})
    (BUILD / "fat_metadata_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
