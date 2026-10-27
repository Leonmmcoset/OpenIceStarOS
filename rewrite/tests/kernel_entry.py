"""Build and run bounded entry/bootstrap recovery tests without changing disks."""
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
    (BUILD / "entry_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    run(["gcc", *flags, "-c", "kernel/entry.c", "-o", "build/kernel_entry_preview.o"])
    boundaries = re.findall(r"ENTRY_BOUNDARY\(\w+,\s*(\w+),\s*(0x[0-9a-f]+)\)",
                            (ROOT / "kernel/entry_boundaries.def").read_text())
    linker = BUILD / "entry_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_entry)\n" +
        "\n".join(f"PROVIDE({name} = {address});" for name, address in
                    [*boundaries, ("kernel_write_port_byte", "0x81a261")]) + "\n" +
        "SECTIONS {\n"
        " .entry 0x800040 : { *(.text.kernel_entry) }\n"
        " ASSERT(kernel_entry == 0x800040 && SIZEOF(.entry) <= 48, \"entry overflow\")\n"
        " .bootstrap 0x800070 : { *(.text.kernel_bootstrap) }\n"
        " ASSERT(kernel_bootstrap == 0x800070 && SIZEOF(.bootstrap) <= 148, \"bootstrap overflow\")\n"
        " /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", "build/entry_preview.elf",
         "build/kernel_entry_preview.o"])
    preview = bytearray(original)
    sizes = {}
    for name, start, end in (("entry", 0x40, 0x70), ("bootstrap", 0x70, 0x104)):
        path = BUILD / f"entry_preview.{name}.bin"
        run(["objcopy", "-O", "binary", "-j", "." + name, "build/entry_preview.elf", str(path)])
        data = path.read_bytes()
        if not data or len(data) > end - start:
            raise RuntimeError(f"invalid {name} size")
        preview[start:end] = data + b"\x90" * (end - start - len(data))
        sizes[name] = {"bytes": len(data), "slot": end - start}
    (BUILD / "entry_preview.x").write_bytes(preview)
    symbols = {int(address, 16): name for name, address in boundaries}
    symbols[0x81a261] = "kernel_write_port_byte"
    candidate = "build/icestar_c.x" if args.installed else "build/entry_preview.x"
    declarations = []
    for name, path in (("original", "build/entry_original.x"), ("candidate", candidate)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x800040", "--stop-address=0x800104", path],
                       capture_output=True, text=True).stdout
        calls = []
        nested = 0
        for line in assembly.splitlines():
            match = re.match(r"\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", line)
            if not match:
                continue
            address, target = (int(value, 16) for value in match.groups())
            if target == 0x800070:
                nested += 1
                continue
            if target not in symbols:
                raise RuntimeError(f"unexpected {name} call: {line}")
            calls.append(f"    {{{address - 0x800000:#x}, (uint32_t){symbols[target]}}},")
        if len(calls) != 28 or nested != 1:
            raise RuntimeError(f"unexpected {name} external/nested call counts: {len(calls)}/{nested}")
        declarations.append(f"static const struct call_patch {name}_calls[] = {{\n" +
                            "\n".join(calls) + "\n};\n")
    (BUILD / "entry_calls.h").write_text("\n".join(declarations))
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_entry_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []),
         "-nostdlib", "-static", "-Wl,-e,_start", "tests/kernel_entry.c",
         "tests/kernel_entry_abi.S", "kernel/entry.c", "-o", str(executable)])
    paths = [ROOT / "kernel/entry.c", ROOT / "kernel/entry_boundaries.def",
             ROOT / "tests/kernel_entry.c", ROOT / "tests/kernel_entry_abi.S",
             ROOT / "tests/kernel_entry.py", BUILD / "entry_preview.elf", executable]
    if args.installed:
        paths.append(BUILD / "icestar_c.x")
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic() - start, 3), "slots": sizes,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in paths}}
    (BUILD / f"entry_{label}_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "cases: 864;" not in result.stdout:
        raise RuntimeError(f"entry fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
