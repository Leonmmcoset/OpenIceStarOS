"""Differential tests for startup UI, independent of production builds."""
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
    (BUILD / "startup_ui_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    run(["gcc", *flags, "-c", "kernel/startup_ui.c", "-o", "build/kernel_startup_ui_preview.o"])
    linker = BUILD / "startup_ui_preview.ld"
    linker.write_text("""OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_startup_ui)
PROVIDE(kernel_startup_logo_821624 = 0x821624);
PROVIDE(kernel_startup_panel_81d6c2 = 0x81d6c2);
PROVIDE(kernel_startup_fill_837545 = 0x837545);
PROVIDE(kernel_startup_outline_837589 = 0x837589);
PROVIDE(kernel_startup_text_8376d3 = 0x8376d3);
PROVIDE(kernel_startup_lookup_8263c0 = 0x8263c0);
PROVIDE(kernel_startup_read_alloc_827454 = 0x827454);
PROVIDE(kernel_startup_read_into_82760c = 0x82760c);
PROVIDE(kernel_startup_load_image_80214c = 0x80214c);
PROVIDE(kernel_wait_ticks = 0x82ad81);
SECTIONS {
 .startup_ui 0x81067a : SUBALIGN(1) {
  *(.text.kernel_startup_ui) *(.text.relative_rectangle) *(.rodata*)
 }
 ASSERT(kernel_startup_ui == 0x81067a && SIZEOF(.startup_ui) <= 1744, "UI overflow")
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
 .unexpected : { *(*) }
 ASSERT(SIZEOF(.unexpected) == 0, "unexpected UI sections")
}
""")
    elf = BUILD / "startup_ui_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), "build/kernel_startup_ui_preview.o"])
    binary = BUILD / "startup_ui_preview.bin"
    run(["objcopy", "-O", "binary", "-j", ".startup_ui", str(elf), str(binary)])
    body = binary.read_bytes()
    if not body or len(body) > 1744:
        raise RuntimeError("invalid UI slot size")
    preview = bytearray(original)
    preview[0x1067a:0x10d4a] = body + b"\x90" * (1744-len(body))
    (BUILD / "startup_ui_preview.x").write_bytes(preview)
    candidate = BUILD / "icestar_c.x" if args.installed else BUILD / "startup_ui_preview.x"
    symbols = {0x821624: "kernel_startup_logo_821624", 0x81d6c2: "kernel_startup_panel_81d6c2",
               0x837545: "kernel_startup_fill_837545", 0x837589: "kernel_startup_outline_837589",
               0x8376d3: "kernel_startup_text_8376d3", 0x8263c0: "kernel_startup_lookup_8263c0",
               0x827454: "kernel_startup_read_alloc_827454", 0x82760c: "kernel_startup_read_into_82760c",
               0x80214c: "kernel_startup_load_image_80214c", 0x82ad81: "kernel_wait_ticks"}
    declarations = []
    for name, path in (("original", BUILD / "startup_ui_original.x"), ("candidate", candidate)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x81067a", "--stop-address=0x810d4a", str(path)],
                       capture_output=True, text=True).stdout
        calls, counts = [], {key: 0 for key in symbols}
        for line in assembly.splitlines():
            match = re.match(r"\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", line)
            if not match:
                continue
            address, target = (int(value, 16) for value in match.groups())
            if 0x81067a <= target < 0x810d4a:
                continue
            if target not in symbols:
                raise RuntimeError("unexpected UI CALL: " + line)
            counts[target] += 1
            calls.append(f"    {{{address - 0x800000:#x}, (uint32_t){symbols[target]}}},")
        if not all(counts.values()):
            raise RuntimeError(f"missing UI boundary CALL: {counts}")
        declarations.append(f"static const struct call_patch {name}_calls[] = {{\n"+
                            "\n".join(calls)+"\n};\n")
    (BUILD / "startup_ui_calls.h").write_text("\n".join(declarations))
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_startup_ui_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_startup_ui.c", "tests/kernel_entry_abi.S",
         "kernel/startup_ui.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=180)
    paths = [ROOT / p for p in ("kernel/startup_ui.c", "kernel/startup_ui.h",
             "tests/kernel_startup_ui.c", "tests/kernel_startup_ui.py", "tests/kernel_entry_abi.S")]
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic()-start,3), "compiled_bytes": len(body), "slot_bytes": 1744,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, elf, executable, candidate]}}
    (BUILD / f"startup_ui_{label}_summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,indent=2))
    if result.returncode or "cases: 36864;" not in result.stdout:
        raise RuntimeError(f"startup UI fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
