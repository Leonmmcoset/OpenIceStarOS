"""Compare x87 factorial/power/series at original addresses and as ordinary C."""
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
    ("factorial", "kernel_factorial_81a2aa", 0x81a2aa, 0x81a2dc),
    ("sine", "kernel_sine_81a2dc", 0x81a2dc, 0x81a366),
    ("cosine", "kernel_cosine_81a366", 0x81a366, 0x81a3f6),
    ("power", "kernel_power_81a538", 0x81a538, 0x81a604),
]
FLAGS = [*(f for f in FLAGS if not f.startswith("-mpreferred-stack-boundary")), "-fexcess-precision=fast", "-frounding-math", "-fno-math-errno"]


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    (BUILD / "math_series_original.x").write_bytes(original)
    obj = BUILD / "kernel_math_series_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/math_series.c", "-o", str(obj)])
    linker, elf = BUILD / "math_series_preview.ld", BUILD / "math_series_preview.elf"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_factorial_81a2aa)\n"
        "SECTIONS {\n" +
        "\n".join(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                    f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
                    for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    production = BUILD / "icestar_c.x"
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
    candidate_path = BUILD / "math_series_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_math_series_{label}_test"
    run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start",
         "tests/kernel_math_series.c", "tests/math_series_abi.S",
         "kernel/math_series.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
    summary = dict(installed=args.installed, exit_code=result.returncode,
        seconds=round(time.monotonic()-start,3), slots=sizes, boundary_substitutions=0,
        stdout=result.stdout,stderr=result.stderr,
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [ROOT/"kernel/math_series.c",ROOT/"kernel/math_series.h",
                          ROOT/"tests/kernel_math_series.c",ROOT/"tests/math_series_abi.S",elf,executable,candidate_path,production]})
    (BUILD/f"math_series_{label}_summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,indent=2),flush=True)
    if result.returncode or "x87 math series verified" not in result.stdout:
        raise RuntimeError(f"math fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
