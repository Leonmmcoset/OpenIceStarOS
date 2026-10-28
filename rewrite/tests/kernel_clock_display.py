"""Recover packed clock display wrappers and check their variadic boundary."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = (
    ("clock_draw_time", "kernel_draw_clock_time", 0x82ac53, 0x82acdd),
    ("clock_draw_date", "kernel_draw_clock_date", 0x82acdd, 0x82ad81),
)
BOUNDARIES = {0x81f88a: "kernel_format_text"}


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original kernel hash mismatch")
    (BUILD / "clock_display_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    obj = BUILD / "kernel_clock_display_preview.o"
    run(["gcc", *flags, "-c", "kernel/clock_display.c", "-o", str(obj)])
    linker = BUILD / "clock_display_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_draw_clock_time)\n" +
        "\n".join(f"PROVIDE({name} = {address:#x});" for address, name in BOUNDARIES.items()) +
        "\nSECTIONS {\n" +
        "\n".join(f" .{section} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                  f" ASSERT({symbol} == {start:#x} && SIZEOF(.{section}) <= {end-start}, \"{section} overflow\")"
                  for section, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    elf = BUILD / "clock_display_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    preview = bytearray(original)
    sizes = {}
    for section, symbol, start, end in SLOTS:
        binary = BUILD / f"clock_display_preview.{section}.bin"
        run(["objcopy", "-O", "binary", "-j", "." + section, str(elf), str(binary)])
        body = binary.read_bytes()
        if not 0 < len(body) <= end-start:
            raise RuntimeError("invalid " + section + " size")
        preview[start-0x800000:end-0x800000] = body + b"\x90" * (end-start-len(body))
        sizes[section] = {"compiled_bytes": len(body), "slot_bytes": end-start}
    (BUILD / "clock_display_preview.x").write_bytes(preview)
    candidate = BUILD / "icestar_c.x" if args.installed else BUILD / "clock_display_preview.x"
    patches = []
    for label, path, base in (("original", BUILD / "clock_display_original.x", 0x900000),
                              ("candidate", candidate, 0x800000)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x82ac53", "--stop-address=0x82ad81", str(path)],
                       capture_output=True, text=True).stdout
        calls = [(int(a, 16), int(t, 16)) for a, t in re.findall(
            r"^\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", assembly, re.M)]
        if sorted(t for _, t in calls) != [0x81f88a, 0x81f88a]:
            raise RuntimeError("unexpected clock display external calls: " + repr(calls))
        for address, target in calls:
            if target in BOUNDARIES:
                call = base + address - 0x800000
                patches.append(f"    DWORD({call+1:#x}) = (uint32_t){BOUNDARIES[target]} - {call+5:#x};")
    (BUILD / "clock_display_calls.h").write_text("\n".join(patches) + "\n")
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_clock_display_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_clock_display.c", "tests/clock_display_abi.S",
         "kernel/clock_display.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=240)
    paths = [ROOT / p for p in ("kernel/clock_display.c", "kernel/clock_display.h",
             "tests/kernel_clock_display.c", "tests/kernel_clock_display.py", "tests/clock_display_abi.S")]
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "slots": sizes,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, elf, executable, candidate]}}
    (BUILD / f"clock_display_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "both clock wrappers verified" not in result.stdout:
        raise RuntimeError(f"clock display fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
