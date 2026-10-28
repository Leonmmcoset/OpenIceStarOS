"""Execute palette and brush primitives with real selected drawing callees."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from kernel_icon_pixels import FLAGS as BASE_FLAGS

FLAGS = [*BASE_FLAGS, "-DRASTER_ARGUMENT_WORDS=8", "-DRASTER_INCOMING_EAX=0x6e25c813"]
# The small square wrapper must use exactly the original outgoing frame: the
# 32-bit fill returns a pointer into it in EAX. Reserve callee-saved registers
# so GCC computes endpoints using only EAX/ECX/EDX and needs no save slots.
SOURCE_FLAGS = [*BASE_FLAGS, "-ffixed-ebx", "-ffixed-esi", "-ffixed-edi"]
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [
    ("palette_lookup", "kernel_palette_lookup_81b714", 0x81b714, 0x81b734),
    ("brush_square", "kernel_brush_square_81d032", 0x81d032, 0x81d070),
    ("brush_line", "kernel_brush_line_81d070", 0x81d070, 0x81d1fc),
    ("brush_circle", "kernel_brush_circle_81d1fc", 0x81d1fc, 0x81d4e6),
    ("circle_quadrants", "kernel_circle_quadrants_81d4e6", 0x81d4e6, 0x81d6c2),
]


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    parser.add_argument("--component", choices=("all", "direct", "chain"), default="all")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    (BUILD / "raster_brush_original.x").write_bytes(original)
    obj = BUILD / "kernel_raster_brush_preview.o"
    run(["gcc", *SOURCE_FLAGS, "-c", "kernel/raster_brush.c", "-o", str(obj)])
    linker, elf = BUILD / "raster_brush_preview.ld", BUILD / "raster_brush_preview.elf"
    sections = []
    for name, symbol, start, end in SLOTS:
        extra = " *(.text.kernel_brush_circle_body)" if name == "brush_circle" else ""
        sections.append(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}){extra} }}\n"
                        f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")")
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_palette_lookup_81b714)\nSECTIONS {\n" +
        "\n".join(sections) + "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
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
    candidate_path = BUILD / "raster_brush_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    paths = [ROOT / p for p in ("kernel/raster_brush.c", "kernel/raster_brush.h",
        "tests/kernel_raster_brush.py", "tests/kernel_raster_brush.c",
        "tests/kernel_raster_brush_chain.c", "tests/raster_abi.S", "tests/raster_fixture.h",
        "tests/kernel_icon_pixels.py")]
    for component in ("direct", "chain"):
        if args.component not in ("all", component):
            continue
        source = "tests/kernel_raster_brush" + ("_chain" if component == "chain" else "") + ".c"
        executable = BUILD / f"kernel_raster_brush_{component}_{label}_test"
        run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start",
             source, "tests/raster_abi.S", str(obj), "-o", str(executable)])
        start = time.monotonic()
        result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
        summary = dict(installed=args.installed, component=component, exit_code=result.returncode,
            seconds=round(time.monotonic()-start, 3), slots=sizes, boundary_substitutions=0,
            stdout=result.stdout, stderr=result.stderr,
            sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in [*paths, elf, executable, candidate_path, production]})
        (BUILD / f"raster_brush_{component}_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
        print(json.dumps(summary, indent=2), flush=True)
        if result.returncode or "brush primitives verified" not in result.stdout:
            raise RuntimeError(f"brush {component} fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
