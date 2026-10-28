"""Compare the original float formatter and its complete math-service chain at original addresses and as ordinary C."""
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

SLOTS = [("round", "kernel_round_81a4fc", 0x81a4fc, 0x81a538),
         ("format_float", "kernel_format_float_81a634", 0x81a634, 0x81a9ee)]


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    (BUILD / "math_format_original.x").write_bytes(original)
    obj = BUILD / "kernel_math_format_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/math_format.c", "-o", str(obj)])
    extra_obj = BUILD / "kernel_math_format_round_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/math_extra.c", "-o", str(extra_obj)])
    linker, elf = BUILD / "math_format_preview.ld", BUILD / "math_format_preview.elf"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_format_float_81a634)\nPROVIDE(kernel_round_81a4fc = 0x81a4fc);\nPROVIDE(kernel_power_81a538 = 0x81a538);\n"
        "SECTIONS {\n" +
        "\n".join(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                    f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
                    for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "--gc-sections", "-T", str(linker), "-o", str(elf), str(obj), str(extra_obj)])
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
    candidate_path = BUILD / "math_format_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_math_format_{label}_test"
    run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start",
         "tests/kernel_math_format.c", "tests/math_format_abi.S",
         "kernel/math_format.c", "kernel/math_extra.c", "kernel/math_series.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
    summary = dict(installed=args.installed, exit_code=result.returncode,
        seconds=round(time.monotonic()-start,3), slots=sizes, boundary_substitutions=0,
        stdout=result.stdout,stderr=result.stderr,
        sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [ROOT/"kernel/math_format.c",ROOT/"kernel/math_format.h",
                          ROOT/"tests/kernel_math_format.c",ROOT/"tests/math_format_abi.S",elf,executable,candidate_path,production]})
    (BUILD/f"math_format_{label}_summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,indent=2),flush=True)
    if result.returncode or "float formatter verified" not in result.stdout:
        raise RuntimeError(f"math fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
