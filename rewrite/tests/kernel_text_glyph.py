"""Execute 8x16 normal and rotated glyphs with byte-address and initialized-table oracles."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from kernel_icon_pixels import FLAGS as BASE_FLAGS

FLAGS = BASE_FLAGS
SOURCE_FLAGS = BASE_FLAGS
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [
    ("text_glyph8", "kernel_text_glyph8_81db5a", 0x81db5a, 0x81dcc0),
    ("text_rotated8", "kernel_text_rotated8_81dcc0", 0x81dcc0, 0x81de2a),
    ("text_glyph32", "kernel_text_glyph32_81de2a", 0x81de2a, 0x81df96),
    ("text_rotated32", "kernel_text_rotated32_81df96", 0x81df96, 0x81e106),
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
    (BUILD / "text_glyph_original.x").write_bytes(original)
    obj = BUILD / "kernel_text_glyph_preview.o"
    run(["gcc", *SOURCE_FLAGS, "-c", "kernel/text_glyph.c", "-o", str(obj)])
    linker, elf = BUILD / "text_glyph_preview.ld", BUILD / "text_glyph_preview.elf"
    sections = []
    for name, symbol, start, end in SLOTS:
        sections.append(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                        f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")")
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_text_glyph8_81db5a)\nSECTIONS {\n" +
        "\n".join(sections) + "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
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
    candidate_path = BUILD / "text_glyph_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    paths = [ROOT / p for p in ("kernel/text_glyph.c", "kernel/text_glyph.h",
        "tests/kernel_text_glyph.py", "tests/kernel_text_glyph.c",
        "tests/kernel_text_glyph_chain.c", "tests/raster_abi.S", "tests/raster_fixture.h",
        "tests/kernel_icon_pixels.py")]
    for component in ("direct", "chain"):
        if args.component not in ("all", component):
            continue
        source = "tests/kernel_text_glyph" + ("_chain" if component == "chain" else "") + ".c"
        executable = BUILD / f"kernel_text_glyph_{component}_{label}_test"
        run(["gcc", *FLAGS, "-nostdlib", "-static", "-Wl,-e,_start,--gc-sections",
             source, "tests/raster_abi.S", str(obj), "-o", str(executable)])
        start = time.monotonic()
        result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
        summary = dict(installed=args.installed, component=component, exit_code=result.returncode,
            seconds=round(time.monotonic()-start, 3), slots=sizes, boundary_substitutions=0,
            stdout=result.stdout, stderr=result.stderr,
            sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in [*paths, elf, executable, candidate_path, production]})
        (BUILD / f"text_glyph_{component}_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
        print(json.dumps(summary, indent=2), flush=True)
        if result.returncode or "text glyphs verified" not in result.stdout:
            raise RuntimeError(f"glyph {component} fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
