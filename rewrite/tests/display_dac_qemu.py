"""CPL0 differential checks for the recovered VGA DAC writer; preview or installed."""
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
    (BUILD / "display_dac_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    obj = BUILD / "kernel_display_dac_preview.o"
    run(["gcc", *flags, "-c", "kernel/display_dac.c", "-o", str(obj)])
    linker = BUILD / "display_dac_preview.ld"
    linker.write_text("""OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_display_write_palette)
PROVIDE(kernel_read_flags = 0x81a24a);
PROVIDE(kernel_disable_interrupts = 0x81a242);
PROVIDE(kernel_write_flags = 0x81a24d);
PROVIDE(kernel_write_port_byte = 0x81a261);
SECTIONS {
 .display_dac 0x81ad6e : SUBALIGN(1) {
  *(.text.kernel_display_write_palette) *(.text.display_dac_body)
 }
 ASSERT(kernel_display_write_palette == 0x81ad6e && SIZEOF(.display_dac) <= 166, "DAC overflow")
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
 .unexpected : { *(*) }
 ASSERT(SIZEOF(.unexpected) == 0, "unexpected DAC sections")
}
""")
    elf = BUILD / "display_dac_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    binary = BUILD / "display_dac_preview.bin"
    run(["objcopy", "-O", "binary", "-j", ".display_dac", str(elf), str(binary)])
    body = binary.read_bytes()
    if not body or len(body) > 166:
        raise RuntimeError("invalid DAC slot size")
    preview = bytearray(original)
    preview[0x1ad6e:0x1ae14] = body + b"\x90" * (166 - len(body))
    (BUILD / "display_dac_preview.x").write_bytes(preview)
    candidate = BUILD / "openicestaros_kernel.x" if args.installed else BUILD / "display_dac_preview.x"
    (BUILD / "display_dac_fixture_candidate.x").write_bytes(candidate.read_bytes())
    symbols = {0x81a24a: "kernel_read_flags", 0x81a242: "kernel_disable_interrupts",
               0x81a24d: "kernel_write_flags", 0x81a261: "kernel_write_port_byte"}
    declarations = []
    for name, path in (("original", BUILD / "display_dac_original.x"), ("candidate", candidate)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x81ad6e", "--stop-address=0x81ae14", str(path)],
                       capture_output=True, text=True).stdout
        calls, counts = [], {key: 0 for key in symbols}
        for line in assembly.splitlines():
            match = re.match(r"\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", line)
            if not match:
                continue
            address, target = (int(value, 16) for value in match.groups())
            if 0x81ad6e <= target < 0x81ae14:
                continue
            if target not in symbols:
                raise RuntimeError("unexpected DAC call: " + line)
            counts[target] += 1
            calls.append(f"    {{{address - 0x800000:#x}, (uint32_t){symbols[target]}}},")
        if counts != {0x81a24a: 1, 0x81a242: 1, 0x81a24d: 1, 0x81a261: 4}:
            raise RuntimeError(f"unexpected DAC boundary CALL counts: {counts}")
        declarations.append(f"static const struct call_patch {name}_calls[] = {{\n" +
                            "\n".join(calls) + "\n};\n")
    (BUILD / "display_dac_calls.h").write_text("\n".join(declarations))
    objects = []
    for source in ("tests/display_dac_qemu.c", "tests/display_dac_qemu_entry.S", "kernel/display_dac.c"):
        fixture_obj = BUILD / (Path(source).stem + "_fixture.o")
        run(["gcc", *flags, "-c", source, "-o", str(fixture_obj)])
        objects.append(str(fixture_obj))
    fixture = BUILD / "display_dac_qemu.elf"
    run(["ld", "-m", "elf_i386", "-T", "tests/display_dac_qemu.ld", "-o", str(fixture), *objects])
    label = "installed" if args.installed else "preview"
    log = BUILD / f"display_dac_{label}_qemu.log"
    start = time.monotonic()
    result = subprocess.run(["qemu-system-i386", "-machine", "pc", "-cpu", "max", "-m", "64",
                             "-kernel", str(fixture), "-display", "none", "-serial", "none",
                             "-monitor", "none", "-no-reboot", "-no-shutdown",
                             "-debugcon", "file:" + str(log),
                             "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"],
                            cwd=ROOT, timeout=300, capture_output=True, text=True)
    output = log.read_text()
    sources = [ROOT / p for p in ("kernel/display_dac.c", "kernel/display_dac.h",
                "tests/display_dac_qemu.c", "tests/display_dac_qemu_entry.S",
                "tests/display_dac_qemu.ld", "tests/display_dac_qemu.py")]
    summary = {"installed": args.installed, "qemu_exit": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "compiled_bytes": len(body),
               "slot_bytes": 166, "output": output, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*sources, elf, fixture, candidate, BUILD / "display_dac_original.x"]}}
    (BUILD / f"display_dac_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode != 33 or "PASS: 6912 DAC scenarios, 20736" not in output:
        raise RuntimeError(f"DAC fixture failed (QEMU exit {result.returncode})")


if __name__ == "__main__":
    main()
