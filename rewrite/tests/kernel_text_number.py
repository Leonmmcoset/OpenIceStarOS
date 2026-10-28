"""Check unsigned radix rendering with real compiled glyphs and exception probes."""
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
    ("text_number8", "kernel_text_number8_81eaf2", 0x81eaf2, 0x81ec98),
    ("text_number32", "kernel_text_number32_81ec98", 0x81ec98, 0x81ee3e),
    ("number_digits", "kernel_number_digits_81ee3e", 0x81ee3e, 0x81ee7e),
]
BOUNDARIES = {"kernel_text_run8_81e7b2": 0x81e7b2,
              "kernel_text_run32_81e952": 0x81e952,
              "kernel_text_vertical8_81e87c": 0x81e87c,
              "kernel_text_vertical32_81ea1c": 0x81ea1c,
              "kernel_text_wide8_81e106": 0x81e106,
              "kernel_text_wide8_mask_81e2cc": 0x81e2cc,
              "kernel_text_wide32_81e45a": 0x81e45a,
              "kernel_text_wide32_mask_81e624": 0x81e624}


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    parser.add_argument("--component", choices=("all", "direct", "chain"), default="all")
    args = parser.parse_args()
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    (BUILD / "text_number_original.x").write_bytes(original)
    obj = BUILD / "kernel_text_number_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/text_number.c", "-o", str(obj)])
    linker, elf = BUILD / "text_number_preview.ld", BUILD / "text_number_preview.elf"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_text_number8_81eaf2)\n" +
        "".join(f"PROVIDE({symbol} = {address:#x});\n" for symbol, address in BOUNDARIES.items()) +
        "SECTIONS {\n" + "\n".join(
            f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
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
    candidate_path = BUILD / "text_number_candidate.x"
    candidate_path.write_bytes(candidate)
    label = "installed" if args.installed else "preview"
    paths = [ROOT / p for p in ("kernel/text_number.c", "kernel/text_number.h",
        "tests/kernel_text_number.py", "tests/kernel_text_number.c", "tests/kernel_text_number_chain.c",
        "tests/raster_abi.S", "tests/raster_fixture.h", "tests/kernel_icon_pixels.py",
        "tests/kernel_text_run.c", "tests/kernel_text_wide.c")]
    for component in ("direct", "chain"):
        if args.component not in ("all", component):
            continue
        source = "tests/kernel_text_number" + ("_chain" if component == "chain" else "") + ".c"
        executable = BUILD / f"kernel_text_number_{component}_{label}_test"
        run(["gcc", *FLAGS, "-DRASTER_ARGUMENT_WORDS=8", "-nostdlib", "-static",
             "-Wl,-e,_start,--gc-sections", source, "tests/raster_abi.S", str(obj),
             *[f"-Wl,--defsym,{symbol}={address:#x}" for symbol, address in BOUNDARIES.items()],
             "-o", str(executable)])
        started = time.monotonic()
        result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
        summary = dict(installed=args.installed, component=component, exit_code=result.returncode,
            seconds=round(time.monotonic()-started, 3), slots=sizes, boundary_substitutions=0,
            stdout=result.stdout, stderr=result.stderr,
            sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in [*paths, elf, executable, candidate_path, production]})
        (BUILD / f"text_number_{component}_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
        print(json.dumps(summary, indent=2), flush=True)
        if result.returncode or "numbers verified" not in result.stdout:
            raise RuntimeError(f"number {component} fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
