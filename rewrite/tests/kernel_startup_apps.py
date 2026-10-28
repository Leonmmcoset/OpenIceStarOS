"""Verify the startup image caller in isolation without changing boot disks."""
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
    (BUILD / "startup_apps_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    run(["gcc", *flags, "-c", "kernel/startup_apps.c", "-o", "build/kernel_startup_apps_preview.o"])
    linker = BUILD / "startup_apps_preview.ld"
    linker.write_text("""OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_startup_applications)
PROVIDE(kernel_startup_launch_image = 0x802282);
PROVIDE(kernel_wait_ticks = 0x82ad81);
SECTIONS {
 .startup_apps 0x810f38 : { *(.text.kernel_startup_applications) }
 ASSERT(kernel_startup_applications == 0x810f38 && SIZEOF(.startup_apps) <= 64, "startup apps overflow")
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
""")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", "build/startup_apps_preview.elf",
         "build/kernel_startup_apps_preview.o"])
    binary = BUILD / "startup_apps_preview.bin"
    run(["objcopy", "-O", "binary", "-j", ".startup_apps", "build/startup_apps_preview.elf", str(binary)])
    data = binary.read_bytes()
    if not data or len(data) > 64:
        raise RuntimeError("invalid startup apps slot size")
    preview = bytearray(original)
    preview[0x10f38:0x10f78] = data + b"\x90" * (64 - len(data))
    (BUILD / "startup_apps_preview.x").write_bytes(preview)
    candidate = "build/openicestaros_kernel.x" if args.installed else "build/startup_apps_preview.x"
    symbols = {0x802282: "kernel_startup_launch_image", 0x82ad81: "kernel_wait_ticks"}
    declarations = []
    for name, path in (("original", "build/startup_apps_original.x"), ("candidate", candidate)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x810f38", "--stop-address=0x810f78", path],
                       capture_output=True, text=True).stdout
        calls, targets = [], []
        for line in assembly.splitlines():
            match = re.match(r"\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", line)
            if not match:
                continue
            address, target = (int(value, 16) for value in match.groups())
            if target not in symbols:
                raise RuntimeError("unexpected startup call: " + line)
            targets.append(target)
            calls.append(f"    {{{address - 0x800000:#x}, (uint32_t){symbols[target]}}},")
        if targets != [0x802282, 0x82ad81, 0x802282]:
            raise RuntimeError("unexpected launch/wait order")
        declarations.append(f"static const struct call_patch {name}_calls[] = {{\n" +
                            "\n".join(calls) + "\n};\n")
    (BUILD / "startup_apps_calls.h").write_text("\n".join(declarations))
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_startup_apps_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []),
         "-nostdlib", "-static", "-Wl,-e,_start", "tests/kernel_startup_apps.c",
         "tests/kernel_entry_abi.S", "kernel/startup_apps.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
    paths = [ROOT / "kernel/startup_apps.c", ROOT / "tests/kernel_startup_apps.c",
             ROOT / "tests/kernel_startup_apps.py", ROOT / "tests/kernel_entry_abi.S",
             BUILD / "startup_apps_preview.elf", executable]
    if args.installed:
        paths.append(BUILD / "openicestaros_kernel.x")
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic() - start, 3),
               "slots": {"startup_apps": {"bytes": len(data), "slot": 64}},
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in paths}}
    (BUILD / f"startup_apps_{label}_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "cases: 480;" not in result.stdout:
        raise RuntimeError(f"startup-applications fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
