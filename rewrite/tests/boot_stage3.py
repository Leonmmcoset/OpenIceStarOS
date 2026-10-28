"""Compare real-to-protected-mode startup against the original third stage.

BIOS and CPUID have deterministic models; GDT loads, CR0/CR4 updates, far
jumps, segment accesses and framebuffer clearing execute as guest code.
Stop before PCI/disk I/O, whose recovered implementations have separate tests.
"""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(BUILD / "test-deps"))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INTR, UC_HOOK_CODE, UC_HOOK_INSN
from unicorn.x86_const import (UC_X86_REG_AX, UC_X86_REG_BX, UC_X86_REG_CX,
    UC_X86_REG_DI, UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS,
    UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_CR0, UC_X86_REG_CR4,
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_GDTR, UC_X86_REG_GS, UC_X86_INS_CPUID, UC_X86_INS_IN, UC_X86_INS_OUT)

BASE, MEMORY_BYTES = 0x1a800, 0x800000


def execute(binary, stop, case):
    uc = Uc(UC_ARCH_X86, UC_MODE_16)
    uc.mem_map(0, MEMORY_BYTES)
    uc.mem_write(0, bytes(range(256)) * (MEMORY_BYTES // 256))
    uc.mem_write(BASE, binary)
    uc.mem_write(BASE + 0xa, struct.pack("<I", case["requested"]))
    for reg, value in ((UC_X86_REG_CS, 0x1a80), (UC_X86_REG_DS, 0x1a80),
                       (UC_X86_REG_ES, 0), (UC_X86_REG_SS, 0),
                       (UC_X86_REG_ESP, 0x7c00), (UC_X86_REG_EFLAGS, 0x202)):
        uc.reg_write(reg, value)
    bios, ports = [], []
    finished = False

    def interrupt(machine, number, unused):
        assert number == 0x10, f"unexpected interrupt {number:#x}"
        ax, cx = machine.reg_read(UC_X86_REG_AX), machine.reg_read(UC_X86_REG_CX)
        assert machine.reg_read(UC_X86_REG_ES) == 0
        if ax == 0x4f00:
            assert machine.reg_read(UC_X86_REG_DI) == 0x500
            machine.mem_write(0x504, struct.pack("<H", case["version"]))
            machine.reg_write(UC_X86_REG_AX, case["controller"])
            bios.append([ax])
        elif ax == 0x4f01:
            assert machine.reg_read(UC_X86_REG_DI) == 0x700
            if cx in (0x101, 0x118):
                width, height = case["dimensions"] if cx == 0x101 else (1024, 768)
                machine.mem_write(0x700, struct.pack("<H", 0x91))
                machine.mem_write(0x712, struct.pack("<HH", width, height))
                machine.mem_write(0x719, b"\x20")
                machine.mem_write(0x728, struct.pack("<I", 0x300000 + case["unaligned"]))
                machine.reg_write(UC_X86_REG_AX, 0x4f)
            else:
                machine.reg_write(UC_X86_REG_AX, 0x14f)
            bios.append([ax, cx])
        elif ax == 0x4f02:
            bios.append([ax, machine.reg_read(UC_X86_REG_BX)])
            machine.reg_write(UC_X86_REG_AX, 0x4f)
        elif ax == 0x13:
            bios.append([ax])
        else:
            raise AssertionError(f"unexpected BIOS function {ax:#x}")

    def cpuid(machine, unused):
        assert machine.reg_read(UC_X86_REG_EAX) == 1
        for reg, value in ((UC_X86_REG_EAX, 0x663), (UC_X86_REG_EBX, 0),
                           (UC_X86_REG_ECX, case["ecx"]), (UC_X86_REG_EDX, case["edx"])):
            machine.reg_write(reg, value)
        return True

    def port_in(machine, port, size, unused):
        assert (port, size) == (0x92, 1)
        ports.append(["in", port, size, 0x20])
        return 0x20

    def port_out(machine, port, size, value, unused):
        ports.append(["out", port, size, value])
        assert (port, size, value) == (0x92, 1, 0x22)

    def finish(machine, address, size, unused):
        nonlocal finished
        finished = True
        machine.emu_stop()

    uc.hook_add(UC_HOOK_INTR, interrupt)
    uc.hook_add(UC_HOOK_INSN, cpuid, None, 1, 0, UC_X86_INS_CPUID)
    uc.hook_add(UC_HOOK_INSN, port_in, None, 1, 0, UC_X86_INS_IN)
    uc.hook_add(UC_HOOK_INSN, port_out, None, 1, 0, UC_X86_INS_OUT)
    uc.hook_add(UC_HOOK_CODE, finish, begin=stop, end=stop)
    uc.emu_start(BASE + struct.unpack_from("<H", binary, 4)[0], MEMORY_BYTES, count=10000000)
    assert finished, "startup did not reach the PCI boundary"
    assert uc.reg_read(UC_X86_REG_GS) == 0x10
    assert uc.reg_read(UC_X86_REG_DS) == uc.reg_read(UC_X86_REG_ES) == 8
    assert uc.reg_read(UC_X86_REG_EFLAGS) & 0x200 == 0, "interrupts enabled at handoff"
    memory = bytearray(uc.mem_read(0, MEMORY_BYTES))
    header = bytes(memory[BASE+0xa:BASE+0x7b])
    memory[BASE:BASE+0x1000] = bytes(0x1000)  # implementation code/data differs
    memory[0x7000:0x7c00] = bytes(0xc00)     # private scratch stack differs
    # Loading a descriptor sets its accessed bit. The rebuilt stage uses the
    # flat code/stack selectors; the original uses stage/expand-down selectors.
    for offset in range(0x7e05, 0x7e58, 8):
        memory[offset] &= 0xfe
    return dict(header=header, memory=bytes(memory), bios=bios, ports=ports,
                cr0=uc.reg_read(UC_X86_REG_CR0), cr4=uc.reg_read(UC_X86_REG_CR4),
                gdtr=uc.reg_read(UC_X86_REG_GDTR))


def main():
    started = time.monotonic()
    original = (ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()[0xa00:0x1206]
    assert hashlib.sha256(original).hexdigest() == "d42ee9ccbedc531a76d599fda98ef9ce246e739c9d80d9acc05f6a19c1733536"
    variants = []
    for name in ("stage3_hybrid", "stage3_c_init"):
        symbols = subprocess.check_output(["nm", str(BUILD/f"{name}.elf")], text=True)
        stop = next(int(line.split()[0], 16) for line in symbols.splitlines()
                    if line.split()[-1] == "stage_scan_pci")
        variants.append((name, (BUILD/f"{name}.bin").read_bytes(), stop))
    scenarios = []
    for edx in (0, 0x2000000, 0x789fbff):
        for variant in range(9):
            case = dict(edx=edx, ecx=0xabcd1234, requested=0x101, controller=0x4f,
                        version=0x300, dimensions=(7, 9), unaligned=0)
            if variant == 1: case["dimensions"] = (1, 1)
            if variant == 2: case["dimensions"] = (320, 200)
            if variant == 3: case["dimensions"] = (1024, 768)
            if variant == 4: case["requested"] = 0x1ff
            if variant == 5: case["requested"] = 0x105
            if variant == 6: case["controller"] = 0x14f
            if variant == 7: case["version"] = 0x1ff
            if variant == 8: case["unaligned"] = 0x123
            scenarios.append(case)
    for index, case in enumerate(scenarios):
        expected = execute(original, BASE+0x38c, case)
        for name, binary, stop in variants:
            actual = execute(binary, stop, case)
            for field in expected:
                if actual[field] != expected[field]:
                    detail = ""
                    if isinstance(expected[field], bytes):
                        offset = next(i for i,(a,b) in enumerate(zip(expected[field],actual[field])) if a != b)
                        detail = f" at {offset:#x}: {expected[field][offset]:#x} != {actual[field][offset]:#x}"
                    raise AssertionError(f"{name} case {index} {field} differs{detail}")
    paths = [ROOT/"boot/stage3_hybrid.S", ROOT/"boot/stage3_display.c", Path(__file__),
             *[BUILD/f"{name}.bin" for name,_,_ in variants]]
    summary = dict(scenarios=len(scenarios), executions=len(scenarios)*(1+len(variants)),
        bytes_compared_per_execution=MEMORY_BYTES, seconds=round(time.monotonic()-started,3),
        compared="Boot fields, BIOS/port calls, GDTR, CR0/CR4 and eight MiB of memory including framebuffer",
        exclusions="Stage code/data, private stack, hardware-set descriptor accessed bits; flat entry selectors intentionally differ",
        limitation="Deterministic BIOS/CPUID; stops before PCI. Zero-pixel and unsupported-depth hangs are not exercised.",
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
    (BUILD/"boot_stage3_summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,indent=2))


if __name__ == "__main__":
    main()
