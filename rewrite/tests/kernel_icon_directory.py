"""Compile and check the ICO directory dispatch against original x86 code."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time
import struct

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = (
    ("icon_draw_id", "kernel_icon_draw_id", 0x82af08, 0x82af4c),
    ("icon_draw_centered", "kernel_icon_draw_centered", 0x82af4c, 0x82b004),
    ("icon_draw_frame", "kernel_icon_draw_frame", 0x82b004, 0x82b0a0),
)
BOUNDARIES = {0x8241d0: "kernel_icon_png_8241d0", 0x82b20c: "kernel_icon_dib_82b20c"}


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    disk = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes())
    original = disk.read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original kernel hash mismatch")
    (BUILD / "icon_directory_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    obj = BUILD / "kernel_icon_directory_preview.o"
    run(["gcc", *flags, "-c", "kernel/icon_directory.c", "-o", str(obj)])
    linker = BUILD / "icon_directory_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_icon_draw_id)\n" +
        "\n".join(f"PROVIDE({name} = {address:#x});" for address, name in BOUNDARIES.items()) +
        "\nSECTIONS {\n" +
        "\n".join(f" .{section} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                  f" ASSERT({symbol} == {start:#x} && SIZEOF(.{section}) <= {end-start}, \"{section} overflow\")"
                  for section, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    elf = BUILD / "icon_directory_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    preview = bytearray(original)
    sizes = {}
    for section, symbol, start, end in SLOTS:
        binary = BUILD / f"icon_directory_preview.{section}.bin"
        run(["objcopy", "-O", "binary", "-j", "."+section, str(elf), str(binary)])
        body = binary.read_bytes()
        if not 0 < len(body) <= end-start:
            raise RuntimeError("invalid " + section + " size")
        preview[start-0x800000:end-0x800000] = body + b"\x90" * (end-start-len(body))
        sizes[section] = {"compiled_bytes": len(body), "slot_bytes": end-start}
    (BUILD / "icon_directory_preview.x").write_bytes(preview)
    candidate = BUILD / "icestar_c.x" if args.installed else BUILD / "icon_directory_preview.x"
    patches = []
    for label, path, base in (("original", BUILD / "icon_directory_original.x", 0x900000),
                              ("candidate", candidate, 0x800000)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x82af08", "--stop-address=0x82b0a0", str(path)],
                       capture_output=True, text=True).stdout
        calls = [(int(a, 16), int(t, 16)) for a, t in re.findall(
            r"^\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", assembly, re.M)]
        if sorted(t for _, t in calls) != [0x8241d0, 0x82b004, 0x82b004, 0x82b20c]:
            raise RuntimeError("unexpected ICO external/internal calls: " + repr(calls))
        for address, target in calls:
            if target in BOUNDARIES:
                call = base + address - 0x800000
                patches.append(f"    DWORD({call+1:#x}) = (uint32_t){BOUNDARIES[target]} - {call+5:#x};")
    (BUILD / "icon_directory_calls.h").write_text("\n".join(patches)+"\n")
    fixtures, resources = [], []
    for name in ("QQ", "STAR", "DISK", "TASKBAR"):
        data = disk.read_file(b"ICESTAR    ", b"ICO        ", (name.ljust(8)+"ICO").encode())
        reserved, kind, count = struct.unpack_from("<HHH", data)
        if reserved or kind != 1 or not 0 < count <= 20:
            raise RuntimeError("unexpected original ICO header")
        directory = data[:6+16*count]
        entries = []
        for i in range(count):
            width, height, colors, zero, planes, depth, size, offset = struct.unpack_from(
                "<BBBBHHII", data, 6+16*i)
            if offset+size > len(data):
                raise RuntimeError("ICO payload outside resource")
            png = data[offset:offset+8] == b"\x89PNG\r\n\x1a\n"
            if png != (width == 0 and height == 0):
                raise RuntimeError("original resource disagrees with observed dispatch")
            entries.append(dict(width=width, height=height, size=size, offset=offset, png=png))
        fixtures.append("{"+str(len(directory))+", {"+",".join(str(b) for b in directory)+"}}")
        resources.append(dict(name=name+".ICO", sha256=hashlib.sha256(data).hexdigest(), entries=entries))
    (BUILD / "icon_directory_resources.h").write_text(
        "static const struct { uint32_t size; uint8_t bytes[326]; } resource_fixtures[] = {\n" +
        ",\n".join(fixtures)+"\n};\n")
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_icon_directory_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_icon_directory.c", "tests/icon_directory_abi.S",
         "kernel/icon_directory.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=240)
    paths = [ROOT / p for p in ("kernel/icon_directory.c", "kernel/icon_directory.h",
             "tests/kernel_icon_directory.c", "tests/kernel_icon_directory.py", "tests/icon_directory_abi.S")]
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "slots": sizes, "resources": resources,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, elf, executable, candidate]}}
    (BUILD / f"icon_directory_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "all three ICO wrappers verified" not in result.stdout:
        raise RuntimeError(f"ICO directory fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
