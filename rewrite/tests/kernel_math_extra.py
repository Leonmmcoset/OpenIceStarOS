"""Compare the remaining x87 scalar services at original addresses and as ordinary C."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from kernel_math_series import FLAGS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [
    ("acos", "kernel_acos_81a3f6", 0x81a3f6, 0x81a4fc),
    ("round", "kernel_round_81a4fc", 0x81a4fc, 0x81a538),
    ("absolute", "kernel_absolute_81a604", 0x81a604, 0x81a634),
    ("floor", "kernel_floor_81a9ee", 0x81a9ee, 0x81aa52),
    ("ceil", "kernel_ceil_81aa52", 0x81aa52, 0x81aab8),
    ("remainder", "kernel_remainder_81aab8", 0x81aab8, 0x81ab2a),
    ("sqrt", "kernel_sqrt_81ab2a", 0x81ab2a, 0x81ab34),
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
    (BUILD / "math_extra_original.x").write_bytes(original)
    obj = BUILD / "kernel_math_extra_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/math_extra.c", "-o", str(obj)])
    linker, elf = BUILD / "math_extra_preview.ld", BUILD / "math_extra_preview.elf"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_acos_81a3f6)\nPROVIDE(kernel_factorial_81a2aa = 0x81a2aa);\nPROVIDE(kernel_power_81a538 = 0x81a538);\n"
        "SECTIONS {\n" +
        "\n".join(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                    f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
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
        assert 0 < len(body) <= end-start
        replacement = body + b"\x90"*(end-start-len(body))
        if args.installed:
            assert candidate[start-0x800000:end-0x800000] == replacement
        candidate[start-0x800000:end-0x800000] = replacement
        sizes.append(dict(name=name, start=hex(start), end=hex(end),
                          compiled_bytes=len(body), slot_bytes=end-start))
    candidate_path = BUILD / "math_extra_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_math_extra_{label}_test"
    run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start",
         "tests/kernel_math_extra.c", "tests/math_series_abi.S",
         "kernel/math_extra.c", "kernel/math_series.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
    summary = dict(installed=args.installed, exit_code=result.returncode,
        seconds=round(time.monotonic()-start,3), slots=sizes, boundary_substitutions=0,
        stdout=result.stdout,stderr=result.stderr,
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [ROOT/"kernel/math_extra.c",ROOT/"kernel/math_extra.h",
                          ROOT/"tests/kernel_math_extra.c",ROOT/"tests/math_series_abi.S",elf,executable,candidate_path,production]})
    (BUILD/f"math_extra_{label}_summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,indent=2),flush=True)
    if result.returncode or "x87 extra math verified" not in result.stdout:
        raise RuntimeError(f"math fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
