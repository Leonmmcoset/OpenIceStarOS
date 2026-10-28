"""Check exact CMOS hardware helper encodings and exhaustive BCD arithmetic."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = tuple(("cmos_" + name, "kernel_cmos_" + name, address, end)
              for name, address, end in (
    ("seconds", 0x82adab, 0x82adc5), ("minutes", 0x82adc5, 0x82addf),
    ("hours", 0x82addf, 0x82adf9), ("weekday", 0x82adf9, 0x82ae13),
    ("day", 0x82ae13, 0x82ae2d), ("month", 0x82ae2d, 0x82ae47),
    ("year", 0x82ae47, 0x82ae84)))


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
    (BUILD / "cmos_fields_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables", "-ffunction-sections",
             "-fdata-sections", "-fno-optimize-sibling-calls", "-mpreferred-stack-boundary=2",
             "-Wall", "-Wextra", "-Werror"]
    obj = BUILD / "kernel_cmos_fields_preview.o"
    run(["gcc", *flags, "-c", "kernel/cmos_fields.c", "-o", str(obj)])
    linker = BUILD / "cmos_fields_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_cmos_seconds)\nSECTIONS {\n" +
        "\n".join(f" .{section} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                  f" ASSERT({symbol} == {start:#x} && SIZEOF(.{section}) == {end-start}, \"{section} layout\")"
                  for section, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    elf = BUILD / "cmos_fields_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    preview = bytearray(original)
    sizes = {}
    for section, symbol, start, end in SLOTS:
        binary = BUILD / f"cmos_fields_preview.{section}.bin"
        run(["objcopy", "-O", "binary", "-j", "."+section, str(elf), str(binary)])
        body = binary.read_bytes()
        if body != original[start-0x800000:end-0x800000]:
            raise RuntimeError("hardware register ABI encoding differs: " + section)
        preview[start-0x800000:end-0x800000] = body
        sizes[section] = {"bytes": len(body), "exact_original": True}
    (BUILD / "cmos_fields_preview.x").write_bytes(preview)
    candidate = BUILD / ("openicestaros_kernel.x" if args.installed else "cmos_fields_preview.x")
    candidate_bytes = candidate.read_bytes()
    for section, symbol, start, end in SLOTS:
        if candidate_bytes[start-0x800000:end-0x800000] != original[start-0x800000:end-0x800000]:
            raise RuntimeError("candidate hardware helper differs: " + section)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_cmos_fields_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_cmos_fields.c", "tests/cmos_fields_abi.S",
         "kernel/cmos_fields.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
    paths = [ROOT / p for p in ("kernel/cmos_fields.c", "kernel/cmos_fields.h",
             "tests/kernel_cmos_fields.c", "tests/kernel_cmos_fields.py", "tests/cmos_fields_abi.S")]
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "slots": sizes,
               "scenarios": 67072, "executions": 201216,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, elf, executable, candidate]}}
    (BUILD / f"cmos_fields_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "PASS: 67072 CMOS field scenarios" not in result.stdout:
        raise RuntimeError("CMOS field fixture failed: " + str(result.returncode))


if __name__ == "__main__":
    main()
