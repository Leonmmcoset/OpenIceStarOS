"""Recover timer objects; compare original, ordinary C and fixed-address C."""
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
    ("timer_find_free", "kernel_timer_find_free", 0x82aa33, 0x82aaad),
    ("timer_create", "kernel_timer_create", 0x82aaad, 0x82abaf),
    ("timer_delete", "kernel_timer_delete", 0x82abaf, 0x82ac21),
    ("timer_rearm", "kernel_timer_rearm", 0x82ac21, 0x82ac53),
)
BOUNDARIES = {0x81a24a: "kernel_read_flags", 0x81a242: "kernel_disable_interrupts",
              0x815464: "kernel_allocate", 0x815978: "kernel_free_allocation",
              0x81a24d: "kernel_write_flags"}


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
    (BUILD / "timer_objects_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    obj = BUILD / "kernel_timer_objects_preview.o"
    run(["gcc", *flags, "-c", "kernel/timer_objects.c", "-o", str(obj)])
    linker = BUILD / "timer_objects_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_timer_find_free)\n" +
        "\n".join(f"PROVIDE({name} = {address:#x});" for address, name in BOUNDARIES.items()) +
        "\nSECTIONS {\n" +
        "\n".join(f" .{section} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                  f" ASSERT({symbol} == {start:#x} && SIZEOF(.{section}) <= {end-start}, \"{section} overflow\")"
                  for section, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    elf = BUILD / "timer_objects_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    preview = bytearray(original)
    sizes = {}
    for section, symbol, start, end in SLOTS:
        binary = BUILD / f"timer_objects_preview.{section}.bin"
        run(["objcopy", "-O", "binary", "-j", "." + section, str(elf), str(binary)])
        body = binary.read_bytes()
        if not 0 < len(body) <= end-start:
            raise RuntimeError("invalid " + section + " size")
        preview[start-0x800000:end-0x800000] = body + b"\x90" * (end-start-len(body))
        sizes[section] = {"compiled_bytes": len(body), "slot_bytes": end-start}
    (BUILD / "timer_objects_preview.x").write_bytes(preview)
    candidate = BUILD / "openicestaros_kernel.x" if args.installed else BUILD / "timer_objects_preview.x"
    patches = []
    for label, path, base in (("original", BUILD / "timer_objects_original.x", 0x900000),
                              ("candidate", candidate, 0x800000)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x82aa33", "--stop-address=0x82ac53", str(path)],
                       capture_output=True, text=True).stdout
        calls = [(int(a, 16), int(t, 16)) for a, t in re.findall(
            r"^\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", assembly, re.M)]
        if sorted(t for _, t in calls) != sorted([*BOUNDARIES, 0x81a24a, 0x81a242, 0x81a24d, 0x82aa33]):
            raise RuntimeError("unexpected timer external/internal calls: " + repr(calls))
        for address, target in calls:
            if target in BOUNDARIES:
                call = base + address - 0x800000
                patches.append(f"    DWORD({call+1:#x}) = (uint32_t){BOUNDARIES[target]} - {call+5:#x};")
    (BUILD / "timer_objects_calls.h").write_text("\n".join(patches) + "\n")
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_timer_objects_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_timer_objects.c", "tests/timer_objects_abi.S",
         "kernel/timer_objects.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=240)
    paths = [ROOT / p for p in ("kernel/timer_objects.c", "kernel/timer_objects.h",
             "tests/kernel_timer_objects.c", "tests/kernel_timer_objects.py", "tests/timer_objects_abi.S")]
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "slots": sizes,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, elf, executable, candidate]}}
    (BUILD / f"timer_objects_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "all four timer helpers verified" not in result.stdout:
        raise RuntimeError(f"timer object fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
