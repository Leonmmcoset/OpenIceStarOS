"""Inspect the source-built stage's handoff using real QEMU BIOS/CPU devices."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def check(case, image, stop, entry):
    name, cpu, vga, requested, depth, sse = case
    prefix = BUILD / f"boot_stage3_qemu_{name}"
    with tempfile.TemporaryDirectory(prefix="openicestaros-stage3-") as directory:
        remote = Path(directory) / "gdb.sock"
        with prefix.with_suffix(".qemu.log").open("w") as log:
            process = subprocess.Popen(["qemu-system-i386", "-machine", "pc", "-m", "128",
                "-cpu", cpu, "-vga", vga, "-drive", f"file={image},format=raw,snapshot=on",
                "-boot", "c", "-display", "none", "-monitor", "none", "-serial", "none",
                "-no-reboot", "-no-shutdown", "-S", "-gdb", f"unix:{remote},server=on,wait=off"],
                stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 10
                while not remote.exists():
                    assert process.poll() is None and time.monotonic() < deadline, "QEMU GDB unavailable"
                    time.sleep(0.05)
                commands = ["set pagination off", "set architecture i386", f"target remote {remote}",
                    f"hbreak *{entry:#x}", "continue", f"set {{unsigned int}}0x1a80a={requested:#x}",
                    "delete breakpoints", f"hbreak *{stop:#x}", "continue",
                    f"dump binary memory {prefix}.header 0x1a800 0x1a87b",
                    f"dump binary memory {prefix}.gdt 0x7e00 0x7e58",
                    "set $base = *(unsigned int*)0x1a822",
                    "set $pixels = *(unsigned int*)0x1a81a * *(unsigned int*)0x1a81e",
                    "set $length = $pixels * (*(unsigned char*)0x1a816 / 8)",
                    f"dump binary memory {prefix}.frame $base ($base + $length)",
                    "monitor info registers", "disconnect"]
                result = subprocess.run(["gdb", "--batch", "--nx", *[arg for line in commands
                                         for arg in ("-ex", line)]],
                                        capture_output=True, text=True, timeout=45)
                prefix.with_suffix(".gdb.log").write_text(result.stdout + result.stderr)
                assert result.returncode == 0, f"GDB failed; see {prefix}.gdb.log"
            finally:
                process.kill()
                process.wait()
    header = prefix.with_suffix(".header").read_bytes()
    gdt = prefix.with_suffix(".gdt").read_bytes()
    frame = prefix.with_suffix(".frame").read_bytes()
    width, height, framebuffer = (struct.unpack_from("<I", header, n)[0] for n in (0x1a,0x1e,0x22))
    assert header[0x16] == depth, f"{name}: unexpected color depth {header[0x16]}"
    assert (width, height) == (1024,768), f"{name}: unexpected dimensions {(width,height)}"
    assert len(frame) == width * height * depth // 8 and not any(frame), "framebuffer was not cleared"
    features = struct.unpack_from("<I", header, 0x68)[0]
    assert bool(features & (1 << 25)) == sse, "CPUID SSE handoff differs"
    registers = {name:int(value,16) for name,value in re.findall(
        r"\b(CR[04])=([0-9a-fA-F]+)", result.stdout + result.stderr)}
    assert registers["CR0"] & 1 and registers["CR4"] & 0x600 == (0x600 if sse else 0)
    if sse:
        assert registers["CR0"] & 6 == 2
    expected = [(0,0),(0xffff,0xcf9200),(0xffff,0x40920a),(0x7c00fffe,0xcf9600),
                (0xa800ffff,0x409801),(0xffff,0x9200),(0xffff,0x9800),
                (0xffff,0xcf9200),(0xffff,0xcf9800),(0xffff,0xcff200),(0xffff,0xcff800)]
    mode = struct.unpack_from("<I",header,0xa)[0]
    if mode:
        pixels = width * height
        expected[2] = (((framebuffer << 16) | (pixels & 0xffff)) & 0xffffffff,
                       (framebuffer & 0xff000000) | ((framebuffer >> 16) & 255) |
                       (pixels & 0xffff0000) | 0xc09200)
    for index,pair in enumerate(expected):
        low,high = struct.unpack_from("<II",gdt,index*8)
        assert (low, high & ~0x100) == pair, f"{name}: GDT descriptor {index} differs"
    return dict(name=name,cpu=cpu,vga=vga,requested=hex(requested),selected=hex(mode),
                dimensions=[width,height],depth=depth,framebuffer=hex(framebuffer),
                cleared_bytes=len(frame),sse=sse,registers=registers)


def main():
    started = time.monotonic()
    image = BUILD / "OpenIceStarOS_c_init.vhd"
    elf = BUILD / "stage3_c_init.elf"
    symbols = {line.split()[-1]:int(line.split()[0],16)
               for line in subprocess.check_output(["nm",str(elf)],text=True).splitlines()}
    cases = [("std_sse", "qemu32", "std", 0x101, 32, True),
             ("std_8bit", "qemu32", "std", 0x105, 8, True),
             ("cirrus_8bit", "qemu32", "cirrus", 0x101, 8, True),
             ("std_no_sse", "pentium", "std", 0x101, 32, False)]
    image_sha256 = hashlib.sha256(image.read_bytes()).hexdigest()
    summary = dict(cases=[],limitation="Handoff stops before PCI; requested mode is set at the real-mode entry by GDB.")
    for case in cases:
        result = check(case,image,symbols["stage_scan_pci"],symbols["stage_real_entry"])
        summary["cases"].append(result)
        print(json.dumps(result),flush=True)
    assert hashlib.sha256(image.read_bytes()).hexdigest() == image_sha256, "snapshot boot modified the production image"
    summary["seconds"] = round(time.monotonic()-started,3)
    summary["sha256"] = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (image,elf,Path(__file__))}
    (BUILD/"boot_stage3_qemu_summary.json").write_text(json.dumps(summary,indent=2)+"\n")


if __name__ == "__main__":
    main()
