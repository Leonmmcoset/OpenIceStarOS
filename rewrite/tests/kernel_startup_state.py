"""Build display/object-table recovery fixtures without changing boot images."""
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
    (BUILD / "startup_state_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    run(["gcc", *flags, "-c", "kernel/startup_state.c", "-o", "build/kernel_startup_state_preview.o"])
    linker = BUILD / "startup_state_preview.ld"
    linker.write_text("""OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_init_display_state)
PROVIDE(kernel_display_init_palette = 0x81ae14);
PROVIDE(kernel_display_select_depth = 0x81afbc);
SECTIONS {
 .display_state 0x81acc4 : { *(.text.kernel_init_display_state) }
 ASSERT(kernel_init_display_state == 0x81acc4 && SIZEOF(.display_state) <= 170, "display state overflow")
 .object_table_state 0x82aa0f : { *(.text.kernel_init_object_table_state) }
 ASSERT(kernel_init_object_table_state == 0x82aa0f && SIZEOF(.object_table_state) <= 36, "object state overflow")
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
""")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", "build/startup_state_preview.elf",
         "build/kernel_startup_state_preview.o"])
    preview, sizes = bytearray(original), {}
    for name, start, end in (("display_state", 0x1acc4, 0x1ad6e),
                              ("object_table_state", 0x2aa0f, 0x2aa33)):
        binary = BUILD / f"startup_state_preview.{name}.bin"
        run(["objcopy", "-O", "binary", "-j", "." + name, "build/startup_state_preview.elf", str(binary)])
        data = binary.read_bytes()
        if not data or len(data) > end - start:
            raise RuntimeError("invalid slot size: " + name)
        preview[start:end] = data + b"\x90" * (end - start - len(data))
        sizes[name] = {"bytes": len(data), "slot": end - start}
    (BUILD / "startup_state_preview.x").write_bytes(preview)
    candidate = "build/openicestaros_kernel.x" if args.installed else "build/startup_state_preview.x"
    symbols = {0x81ae14: "kernel_display_init_palette", 0x81afbc: "kernel_display_select_depth"}
    declarations = []
    for name, path in (("original", "build/startup_state_original.x"), ("candidate", candidate)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x81acc4", "--stop-address=0x81ad6e", path],
                       capture_output=True, text=True).stdout
        calls = []
        for line in assembly.splitlines():
            match = re.match(r"\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", line)
            if not match:
                continue
            address, target = (int(value, 16) for value in match.groups())
            if target not in symbols:
                raise RuntimeError("unexpected display initialization call: " + line)
            calls.append(f"    {{{address - 0x800000:#x}, (uint32_t){symbols[target]}}},")
        if len(calls) != 2:
            raise RuntimeError("expected two display boundary calls")
        declarations.append(f"static const struct call_patch {name}_calls[] = {{\n" +
                            "\n".join(calls) + "\n};\n")
    (BUILD / "startup_state_calls.h").write_text("\n".join(declarations))
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_startup_state_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []),
         "-nostdlib", "-static", "-Wl,-e,_start", "tests/kernel_startup_state.c",
         "tests/kernel_entry_abi.S", "kernel/startup_state.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
    paths = [ROOT / "kernel/startup_state.c", ROOT / "tests/kernel_startup_state.c",
             ROOT / "tests/kernel_startup_state.py", ROOT / "tests/kernel_entry_abi.S",
             BUILD / "startup_state_preview.elf", executable]
    if args.installed:
        paths.append(BUILD / "openicestaros_kernel.x")
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic() - start, 3), "slots": sizes,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in paths}}
    (BUILD / f"startup_state_{label}_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "cases: 6864;" not in result.stdout or "cases: 33;" not in result.stdout:
        raise RuntimeError(f"startup-state fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
