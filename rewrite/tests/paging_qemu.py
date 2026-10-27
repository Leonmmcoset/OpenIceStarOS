"""Recover and test identity paging at CPL0, independently of production images."""
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


def controls(data, base):
    path = BUILD / "paging_control_disassembly.bin"
    path.write_bytes(data)
    assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386",
                    f"--adjust-vma={base:#x}", str(path)], capture_output=True, text=True).stdout
    patches = []
    for line in assembly.splitlines():
        match = re.match(r"\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)mov\s+%eax,%cr([03])\b", line)
        if match:
            address = int(match.group(1), 16)
            raw = bytes.fromhex(match.group(2))
            if raw not in (b"\x0f\x22\xd8", b"\x0f\x22\xc0"):
                raise RuntimeError("unexpected control-write instruction: " + line)
            patches.append((address-base, raw, match.group(3)))
    if len(patches) != 2 or [item[2] for item in patches] != ["3", "0"]:
        raise RuntimeError("expected CR3 then CR0 writes")
    return patches


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original kernel hash mismatch")
    (BUILD / "paging_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    obj = BUILD / "kernel_paging_preview.o"
    run(["gcc", *flags, "-c", "kernel/paging.c", "-o", str(obj)])
    linker = BUILD / "paging_preview.ld"
    linker.write_text("""OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_init_identity_paging)
SECTIONS {
 .identity_paging 0x837448 : SUBALIGN(1) {
  *(.text.kernel_init_identity_paging) *(.text.build_identity_tables)
 }
 ASSERT(kernel_init_identity_paging == 0x837448 && SIZEOF(.identity_paging) <= 106, "paging overflow")
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
 .unexpected : { *(*) }
 ASSERT(SIZEOF(.unexpected) == 0, "unexpected paging sections")
}
""")
    elf = BUILD / "paging_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    binary = BUILD / "paging_preview.bin"
    run(["objcopy", "-O", "binary", "-j", ".identity_paging", str(elf), str(binary)])
    body = binary.read_bytes()
    if not body or len(body)>106:
        raise RuntimeError("invalid paging slot size")
    preview = bytearray(original)
    preview[0x37448:0x374b2] = body + b"\x90"*(106-len(body))
    (BUILD / "paging_preview.x").write_bytes(preview)
    candidate = BUILD / "icestar_c.x" if args.installed else BUILD / "paging_preview.x"
    candidate_data = candidate.read_bytes()
    (BUILD / "paging_fixture_candidate.x").write_bytes(candidate_data)
    wrapper = BUILD / "paging_standalone_wrapper.bin"
    run(["objcopy", "-O", "binary", "-j", ".text.kernel_init_identity_paging", str(obj), str(wrapper)])
    arrays = []
    for name, data, base in (("original", original[0x37448:0x374b2], 0x837448),
                              ("candidate", candidate_data[0x37448:0x374b2], 0x837448),
                              ("standalone", wrapper.read_bytes(), 0)):
        entries = [f"    {{{offset:#x}, {{{', '.join(hex(b) for b in raw)}}}}},"
                   for offset, raw, _ in controls(data, base)]
        arrays.append(f"static const struct cr_patch {name}_cr_patches[] = {{\n"+
                      "\n".join(entries)+"\n};\n")
    (BUILD / "paging_cr_patches.h").write_text("\n".join(arrays))
    objects = [str(obj)]
    for source in ("tests/paging_qemu.c", "tests/paging_qemu_entry.S"):
        fixture_obj = BUILD / (Path(source).stem+"_fixture.o")
        run(["gcc", *flags, "-c", source, "-o", str(fixture_obj)])
        objects.append(str(fixture_obj))
    fixture = BUILD / "paging_qemu.elf"
    run(["ld", "-m", "elf_i386", "-T", "tests/paging_qemu.ld", "-o", str(fixture), *objects])
    label = "installed" if args.installed else "preview"
    log = BUILD / f"paging_{label}_qemu.log"
    start = time.monotonic()
    result = subprocess.run(["qemu-system-i386", "-machine", "pc", "-cpu", "max", "-m", "64",
                             "-kernel", str(fixture), "-display", "none", "-serial", "none",
                             "-monitor", "none", "-no-reboot", "-no-shutdown",
                             "-debugcon", "file:"+str(log),
                             "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"],
                            cwd=ROOT, timeout=300, capture_output=True, text=True)
    output = log.read_text()
    sources = [ROOT / p for p in ("kernel/paging.c", "kernel/paging.h", "tests/paging_qemu.c",
                "tests/paging_qemu_entry.S", "tests/paging_qemu.ld", "tests/paging_qemu.py")]
    summary = {"installed": args.installed, "qemu_exit": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "compiled_bytes": len(body),
               "slot_bytes": 106, "output": output, "stderr": result.stderr,
               "unhooked_scenarios": 96, "construction_only_scenarios": 96,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*sources, elf, fixture, candidate, BUILD / "paging_original.x"]}}
    (BUILD / f"paging_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode != 33 or "PASS: 192 paging scenarios, 576" not in output:
        raise RuntimeError(f"paging fixture failed (QEMU exit {result.returncode})")


if __name__ == "__main__":
    main()
