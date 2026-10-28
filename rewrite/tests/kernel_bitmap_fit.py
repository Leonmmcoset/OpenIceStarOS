"""Compare BMP fitting with real pixel/quantizer/initializer bodies."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from kernel_icon_pixels import FLAGS as SOURCE_FLAGS

FLAGS = [*SOURCE_FLAGS, "-DRASTER_INCOMING_EAX=0x6e25c813"]
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [("bitmap_fit", "kernel_bitmap_fit_8204ac", 0x8204ac, 0x820d66)]
BOUNDARIES = {"kernel_pixel_put8_81b26e": 0x81b26e,
              "kernel_pixel_put32_81b2f4": 0x81b2f4,
              "kernel_icon_quantize_81fd90": 0x81fd90,
              "kernel_bitmap_draw_81ffc4": 0x81ffc4}


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    (BUILD / "bitmap_fit_original.x").write_bytes(original)
    obj = BUILD / "kernel_bitmap_fit_preview.o"
    run(["gcc", *SOURCE_FLAGS, "-c", "kernel/bitmap_fit.c", "-o", str(obj)])
    linker, elf = BUILD / "bitmap_fit_preview.ld", BUILD / "bitmap_fit_preview.elf"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_bitmap_fit_8204ac)\n" +
        "".join(f"PROVIDE({symbol} = {address:#x});\n" for symbol, address in BOUNDARIES.items()) +
        "SECTIONS {\n" + "\n".join(
            f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
            f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
            for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    production = BUILD / "icestar_c.x"
    candidate, sizes = bytearray(production.read_bytes()), []
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
    candidate_path = BUILD / "bitmap_fit_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_bitmap_fit_{label}_test"
    run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start,--gc-sections",
         "tests/kernel_bitmap_fit.c", "tests/raster_abi.S", str(obj),
         *[f"-Wl,--defsym,{symbol}={address:#x}" for symbol, address in BOUNDARIES.items()],
         "-o", str(executable)])
    started = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
    paths = [ROOT / p for p in ("kernel/bitmap_fit.c", "kernel/bitmap.h",
        "tests/kernel_bitmap_fit.py", "tests/kernel_bitmap_fit.c", "tests/raster_abi.S",
        "tests/raster_fixture.h", "tests/kernel_icon_pixels.py", "tests/kernel_bitmap.c")]
    summary = dict(installed=args.installed, exit_code=result.returncode,
        seconds=round(time.monotonic()-started, 3), slots=sizes,
        helper_substitutions=0,
        limitation="A bounded bytewise oracle excludes unsafe addresses and excessive loops before execution. Forked probes check source/global faults and zero-ratio nontermination.",
        stdout=result.stdout, stderr=result.stderr,
        sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [*paths, elf, executable, candidate_path, production]})
    (BUILD / f"bitmap_fit_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2), flush=True)
    if result.returncode or "bitmap fit verified" not in result.stdout:
        raise RuntimeError(f"bitmap fit fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
