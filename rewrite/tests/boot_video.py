"""Execute the original 16-bit VBE selector and its C recovery in Unicorn."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(BUILD / "test-deps"))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INTR, UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import (UC_X86_REG_AX, UC_X86_REG_BX, UC_X86_REG_CX,
    UC_X86_REG_DI, UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES,
    UC_X86_REG_SS, UC_X86_REG_ESP, UC_X86_REG_EFLAGS)  # noqa: E402

FLAGS = ["-m16", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
         "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
         "-fno-delete-null-pointer-checks", "-mpreferred-stack-boundary=2",
         "-Wall", "-Wextra", "-Werror"]


def run(command):
    subprocess.run(command, cwd=ROOT, check=True)


def execute(binary, entry, original, setup):
    machine = Uc(UC_ARCH_X86, UC_MODE_16)
    machine.mem_map(0, 0x200000)
    low = bytes((i*37+(i//128)*11+setup["seed"]) & 255 for i in range(0x10000))
    machine.mem_write(0, low)
    machine.mem_write(0x1a800, bytes(0x1000))
    machine.mem_write(0x1a800, binary)
    # Observable header starts identically, independently of code layout.
    header = bytearray(0x7b)
    struct.pack_into("I", header, 0xa, setup["requested"])
    struct.pack_into("I", header, 0x38, setup["count"])
    struct.pack_into("H", header, 0x56, setup["found"])
    struct.pack_into("H", header, 0x58, setup["fallback"])
    machine.mem_write(0x1a800, bytes(header))
    machine.reg_write(UC_X86_REG_CS, 0x1a80)
    machine.reg_write(UC_X86_REG_DS, 0x1a80)
    machine.reg_write(UC_X86_REG_ES, 0)
    machine.reg_write(UC_X86_REG_SS, 0)
    machine.reg_write(UC_X86_REG_ESP, 0x7000)
    machine.reg_write(UC_X86_REG_EFLAGS, 0x202)
    machine.mem_write(0x7000, struct.pack("<H" if original else "<I", 0xf000))
    calls = []
    queries = {}
    finished = False

    def interrupt(uc, number, unused):
        assert number == 16
        ax = uc.reg_read(UC_X86_REG_AX)
        cx = uc.reg_read(UC_X86_REG_CX)
        bx = uc.reg_read(UC_X86_REG_BX)
        if ax == 0x4f00:
            assert uc.reg_read(UC_X86_REG_ES) == 0 and uc.reg_read(UC_X86_REG_DI) == 0x500
            uc.mem_write(0x504, struct.pack("<H", setup["version"]))
            uc.reg_write(UC_X86_REG_AX, setup["controller_status"])
            calls.append([ax])
        elif ax == 0x4f01:
            assert uc.reg_read(UC_X86_REG_ES) == 0 and uc.reg_read(UC_X86_REG_DI) == 0x700
            queries[cx] = queries.get(cx, 0)+1
            mode = setup["modes"].get(cx, (0x14f, 0, 0, 0, 0, 0))
            status, attributes, width, height, depth, framebuffer = mode
            if queries[cx] > 1 and setup.get("final_fail"):
                status = 0x14f
            uc.mem_write(0x700, struct.pack("<H", attributes))
            uc.mem_write(0x712, struct.pack("<HH", width, height))
            uc.mem_write(0x719, bytes([depth]))
            uc.mem_write(0x728, struct.pack("<I", framebuffer))
            uc.reg_write(UC_X86_REG_AX, status)
            calls.append([ax, cx])
        elif ax == 0x4f02:
            calls.append([ax, bx])
            uc.reg_write(UC_X86_REG_AX, setup["set_status"])
        elif ax == 0x13:
            calls.append([ax])
        else:
            raise AssertionError(f"unexpected BIOS call {ax:#x}")

    def code(uc, address, size, unused):
        nonlocal finished
        if address == 0x29800:
            finished = True
            uc.emu_stop()

    machine.hook_add(UC_HOOK_INTR, interrupt)
    machine.hook_add(UC_HOOK_CODE, code, begin=0x29800, end=0x29800)
    machine.emu_start(0x1a800+entry, 0x29801, count=2000000)
    assert finished, "selector did not return"
    # Compare all low memory, except each implementation's private stack.
    memory = bytearray(machine.mem_read(0, 0x10000))
    memory[0x6800:0x7010] = bytes(0x810)
    return bytes(machine.mem_read(0x1a800, 0x7b)), bytes(memory), calls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--installed", action="store_true",
                        help="compare both production-linked third stages as well as the preview")
    arguments = parser.parse_args()
    started = time.monotonic()
    BUILD.mkdir(exist_ok=True)
    obj, io = BUILD/"stage3_video_preview.o", BUILD/"stage3_video_io_preview.o"
    run(["gcc", *FLAGS, "-c", "boot/stage3_video.c", "-o", str(obj)])
    run(["gcc", "-m32", "-c", "boot/stage3_video_io.S", "-o", str(io)])
    linker = BUILD/"stage3_video_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(stage_video_select)\nSECTIONS {\n . = 0x800;\n"
        " .video : { *(.text*) *(.rodata*) }\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n}\n")
    elf = BUILD/"stage3_video_preview.elf"
    binary = BUILD/"stage3_video_preview.bin"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj), str(io)])
    run(["objcopy", "-O", "binary", str(elf), str(binary)])
    original = (ROOT.parent/"IceStarOS_x86_32M.vhd").read_bytes()[0xa00:0x1206]
    assert hashlib.sha256(original).hexdigest() == (
        "d42ee9ccbedc531a76d599fda98ef9ce246e739c9d80d9acc05f6a19c1733536"), "original stage SHA-256 mismatch"
    variants = [("preview", bytes(0x800)+binary.read_bytes(), 0x800)]
    if arguments.installed:
        for name in ("stage3_hybrid", "stage3_c_init"):
            symbols = subprocess.check_output(["nm", str(BUILD/f"{name}.elf")], text=True)
            entry = next(int(line.split()[0],16)-0x1a800 for line in symbols.splitlines()
                         if line.split()[-1] == "stage_video_select")
            variants.append((name, (BUILD/f"{name}.bin").read_bytes(), entry))
    rng = random.Random(0x1892c3)
    scenarios = []
    for requested in (0x101, 0x105, 0x118, 0x1ff, 0, 0xabcd0101):
        for variant in range(12):
            modes = {0x101:(0x4f,0x91,640,480,32,0xe0000000),
                     0x118:(0x4f,0x91,1024,768,32,0xe1000000),
                     0x1ff:(0x4f,1,1024,768,32,0xd0000000)}
            case = dict(seed=variant, requested=requested, count=0, found=0, fallback=0x105,
                        modes=modes, version=0x300, controller_status=0x4f, set_status=0x4f)
            if variant == 1: case["controller_status"] = 0x14f
            if variant == 2: case["version"] = 0x1ff
            if variant == 3: case["modes"] = {}
            if variant == 4: modes[0x101] = (0x4f,0x91,640,480,8,0xe0000000)
            if variant == 5: modes[0x118] = (0x4f,0x90,1024,768,32,0xe1000000)
            if variant == 6: case["final_fail"] = True
            if variant == 7: case["set_status"] = 0x14f
            if variant == 8: case["found"] = 1
            if variant == 9: case["count"] = 0xffff0000
            if variant == 10: case["count"] = 0x1999
            if variant == 11: case["version"] = 0x200
            scenarios.append(case)
    for seed in range(96):
        modes = {}
        for _ in range(rng.randrange(1,16)):
            modes[rng.randrange(0x100,0x200)] = (rng.choice((0x4f,0x14f)), rng.randrange(256),
                rng.choice((320,640,1024,1280,65535)), rng.choice((200,480,768,1024,65535)),
                rng.choice((8,16,24,32)), rng.getrandbits(32))
        scenarios.append(dict(seed=seed, requested=rng.choice((0x105,0x101,rng.randrange(0x100,0x200))),
            count=0, found=0, fallback=0x105, modes=modes, version=0x300,
            controller_status=0x4f, set_status=0x4f))
    bios_calls = 0
    for index, case in enumerate(scenarios):
        a = execute(original, 0x189, True, case)
        for name, recovered, entry in variants:
            b = execute(recovered, entry, False, case)
            if a != b:
                for region, (left, right) in enumerate(zip(a, b)):
                    if left != right:
                        if isinstance(left, bytes):
                            offset = next(i for i,(x,y) in enumerate(zip(left,right)) if x != y)
                            raise AssertionError(f"{name} case {index} region {region} offset {offset:#x}: {left[offset]:#x}!={right[offset]:#x}")
                        raise AssertionError(f"{name} case {index} BIOS requests differ")
        bios_calls += len(a[2])
    paths = [ROOT/p for p in ("boot/stage3_video.c", "boot/stage3_video.h", "boot/stage3_video_io.S", "tests/boot_video.py")]
    summary = dict(scenarios=len(scenarios), executions=len(scenarios)*(1+len(variants)), bios_calls=bios_calls,
        variants=[name for name,_,_ in variants],
        seconds=round(time.monotonic()-started,3), compiled_bytes=len(binary.read_bytes()),
        compared="All boot fields, all 64 KiB low memory except private stack, ordered relevant BIOS requests",
        limitation="INT 10h is a deterministic VBE model; real firmware is tested separately by QEMU boot.",
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [*paths,elf,*([BUILD/f"{n}.bin" for n,_,_ in variants[1:]])]})
    output = "boot_video_installed_summary.json" if arguments.installed else "boot_video_summary.json"
    (BUILD/output).write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,indent=2))


if __name__ == "__main__":
    main()
