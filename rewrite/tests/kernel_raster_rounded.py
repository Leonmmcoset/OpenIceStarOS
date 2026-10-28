"""Execute rounded and chamfered rectangles with real selected drawing callees."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from kernel_icon_pixels import FLAGS as BASE_FLAGS

FLAGS = [*BASE_FLAGS, "-DRASTER_ARGUMENT_WORDS=8", "-DRASTER_INCOMING_EAX=0x6e25c813"]
# The filled wrappers require exactly forty bytes from EBP to the outgoing
# six-word call. Do not defer popping previous call arguments into that frame.
SOURCE_FLAGS = [*BASE_FLAGS, "-fno-defer-pop"]
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [
    ("rounded_outline", "kernel_rounded_outline_81d6c2", 0x81d6c2, 0x81d8ca),
    ("rounded_fill", "kernel_rounded_fill_81d8ca", 0x81d8ca, 0x81da68),
    ("chamfer_fill", "kernel_chamfer_fill_81da68", 0x81da68, 0x81db5a),
]
BOUNDARIES = {"kernel_circle_quadrants_81d4e6": 0x81d4e6,
              "kernel_service_00": 0x8374cf, "kernel_service_01": 0x837501,
              "kernel_service_02": 0x837545,
              # Unused native-array references in the included leaf fixture.
              "kernel_palette_color": 0x81b714, "kernel_brush_square_81d032": 0x81d032,
              "kernel_brush_line_81d070": 0x81d070, "kernel_brush_circle_81d1fc": 0x81d1fc}



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
    (BUILD / "raster_rounded_original.x").write_bytes(original)
    obj = BUILD / "kernel_raster_rounded_preview.o"
    run(["gcc", *SOURCE_FLAGS, "-c", "kernel/raster_rounded.c", "-o", str(obj)])
    linker, elf = BUILD / "raster_rounded_preview.ld", BUILD / "raster_rounded_preview.elf"
    sections = []
    for name, symbol, start, end in SLOTS:
        extra = ""
        sections.append(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}){extra} }}\n"
                        f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")")
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_rounded_outline_81d6c2)\n" +
        "".join(f"PROVIDE({symbol} = {address:#x});\n" for symbol, address in BOUNDARIES.items()) + "SECTIONS {\n" +
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
    candidate_path = BUILD / "raster_rounded_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    paths = [ROOT / p for p in ("kernel/raster_rounded.c", "kernel/raster_rounded.h",
        "tests/kernel_raster_rounded.py", "tests/kernel_raster_rounded.c",
        "tests/kernel_raster_rounded_chain.c", "tests/raster_abi.S", "tests/raster_fixture.h",
        "tests/kernel_icon_pixels.py", "tests/kernel_raster_brush.c", "kernel/service_forward.h", "kernel/raster_brush.h")]
    for component in ("direct", "chain"):
        if args.component not in ("all", component):
            continue
        source = "tests/kernel_raster_rounded" + ("_chain" if component == "chain" else "") + ".c"
        executable = BUILD / f"kernel_raster_rounded_{component}_{label}_test"
        run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start,--gc-sections",
             source, "tests/raster_abi.S", str(obj),
             *[f"-Wl,--defsym,{symbol}={address:#x}" for symbol, address in BOUNDARIES.items()],
             "-o", str(executable)])
        start = time.monotonic()
        result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
        summary = dict(installed=args.installed, component=component, exit_code=result.returncode,
            seconds=round(time.monotonic()-start, 3), slots=sizes, boundary_substitutions=0,
            stdout=result.stdout, stderr=result.stderr,
            sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in [*paths, elf, executable, candidate_path, production]})
        (BUILD / f"raster_rounded_{component}_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
        print(json.dumps(summary, indent=2), flush=True)
        if result.returncode or "rounded rectangles verified" not in result.stdout:
            raise RuntimeError(f"rounded {component} fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
