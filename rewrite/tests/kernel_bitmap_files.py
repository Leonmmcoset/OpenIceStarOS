"""Compare BMP file wrappers using explicit filesystem/draw/free entry detours."""
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

SLOTS = [("bitmap_draw_file", "kernel_bitmap_draw_file_81fec2", 0x81fec2, 0x81ffc4),
         ("bitmap_fit_file", "kernel_bitmap_fit_file_82040c", 0x82040c, 0x8204ac)]
BOUNDARIES = {"kernel_startup_lookup_8263c0": 0x8263c0,
              "kernel_startup_read_alloc_827454": 0x827454,
              "kernel_bitmap_lookup_relative_825c8e": 0x825c8e,
              "kernel_bitmap_read_relative_827032": 0x827032,
              "kernel_bitmap_draw_81ffc4": 0x81ffc4,
              "kernel_bitmap_fit_8204ac": 0x8204ac,
              "kernel_bitmap_free_815978": 0x815978}


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    (BUILD / "bitmap_files_original.x").write_bytes(original)
    obj = BUILD / "kernel_bitmap_files_preview.o"
    run(["gcc", *SOURCE_FLAGS, "-c", "kernel/bitmap_files.c", "-o", str(obj)])
    linker, elf = BUILD / "bitmap_files_preview.ld", BUILD / "bitmap_files_preview.elf"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_bitmap_draw_file_81fec2)\n" +
        "".join(f"PROVIDE({symbol} = {address:#x});\n" for symbol, address in BOUNDARIES.items()) +
        "SECTIONS {\n" + "\n".join(
            f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
            f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
            for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    production = BUILD / "openicestaros_kernel.x"
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
    candidate_path = BUILD / "bitmap_files_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_bitmap_files_{label}_test"
    run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start,--gc-sections",
         "tests/kernel_bitmap_files.c", "tests/raster_abi.S", str(obj),
         *[f"-Wl,--defsym,{symbol}={address:#x}" for symbol, address in BOUNDARIES.items()],
         "-o", str(executable)])
    started = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
    paths = [ROOT / p for p in ("kernel/bitmap_files.c", "kernel/bitmap_files.h",
        "tests/kernel_bitmap_files.py", "tests/kernel_bitmap_files.c", "tests/raster_abi.S",
        "tests/raster_fixture.h", "tests/kernel_icon_pixels.py")]
    summary = dict(installed=args.installed, exit_code=result.returncode,
        seconds=round(time.monotonic()-started, 3), slots=sizes,
        helper_entry_detours={symbol: hex(address) for symbol, address in BOUNDARIES.items()},
        limitation="Seven explicit helper entry detours. 96 drawing and 48 fitting composition cases restore and execute the real BMP bodies. Filesystem/free remain substituted. This is not real filesystem integration.",
        stdout=result.stdout, stderr=result.stderr,
        sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [*paths, elf, executable, candidate_path, production]})
    (BUILD / f"bitmap_files_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2), flush=True)
    if result.returncode or "bitmap file wrappers verified" not in result.stdout:
        raise RuntimeError(f"bitmap file fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
