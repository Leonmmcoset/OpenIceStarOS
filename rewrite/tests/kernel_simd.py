"""Verify vector leaves against original bytes, a lane oracle and exact CPU state."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from kernel_icon_pixels import FLAGS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [
    ("simd_add", "kernel_simd_add_81ab34", 0x81ab34, 0x81ab58),
    ("simd_subtract", "kernel_simd_subtract_81ab58", 0x81ab58, 0x81ab7c),
    ("simd_multiply", "kernel_simd_multiply_81ab7c", 0x81ab7c, 0x81abc6),
    ("simd_blend", "kernel_simd_blend_81abc6", 0x81abc6, 0x81acc4),
]


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    (BUILD / "simd_original.x").write_bytes(original)
    obj, elf = BUILD / "kernel_simd_preview.o", BUILD / "simd_preview.elf"
    run(["gcc", *FLAGS, "-c", "kernel/simd.c", "-o", str(obj)])
    linker = BUILD / "simd_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_simd_add_81ab34)\nSECTIONS {\n" +
        "\n".join(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                    f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) == {end-start}, \"layout mismatch\")"
                    for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    production = BUILD / "openicestaros_kernel.x"
    candidate = bytearray(production.read_bytes())
    sizes = []
    for name, _, start, end in SLOTS:
        path = BUILD / f"{name}_preview.bin"
        run(["objcopy", "-O", "binary", "-j", f".{name}", str(elf), str(path)])
        body = path.read_bytes()
        assert len(body) == end-start
        assert body == original[start-0x800000:end-0x800000], f"original instruction mismatch: {name}"
        if args.installed:
            manifest = json.loads((ROOT / "kernel/patches.json").read_text())
            entry = next(p for p in manifest if p["binary"] == f"kernel.{name}.bin")
            assert entry["exact"] and (int(entry["start"], 0), int(entry["end"], 0)) == (start, end)
            assert (BUILD / entry["binary"]).read_bytes() == body
            assert candidate[start-0x800000:end-0x800000] == body
        candidate[start-0x800000:end-0x800000] = body
        sizes.append(dict(name=name, start=hex(start), end=hex(end),
                          compiled_bytes=len(body), original_bytes_identical=True))
    candidate_path = BUILD / "simd_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_simd_{label}_test"
    run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start",
         "tests/kernel_simd.c", "tests/simd_abi.S", "kernel/simd.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=180)
    paths = [ROOT / p for p in ("kernel/simd.c", "kernel/simd.h", "recovery/SIMD.md",
                               "tests/kernel_simd.c", "tests/kernel_simd.py", "tests/simd_abi.S")]
    summary = dict(installed=args.installed, exit_code=result.returncode,
                   seconds=round(time.monotonic()-start, 3), slots=sizes,
                   boundary_substitutions=0, stdout=result.stdout, stderr=result.stderr,
                   sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in [*paths, elf, executable, candidate_path, production]})
    (BUILD / f"simd_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2), flush=True)
    if result.returncode or "SIMD byte oracle, exact GPR/flags/XMM/x87/MXCSR and guarded boundaries verified" not in result.stdout:
        raise RuntimeError(f"SIMD fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
