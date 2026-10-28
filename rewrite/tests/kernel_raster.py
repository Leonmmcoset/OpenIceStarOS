"""Execute recovered layer sampling and fixed-point lines against original x86."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

from kernel_icon_pixels import FLAGS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [
    ("pixel_sample", "kernel_pixel_sample_below_81b514", 0x81b514, 0x81b714),
    ("raster_line8", "kernel_raster_line8_81b734", 0x81b734, 0x81b8dc),
    ("raster_line32", "kernel_raster_line32_81b8dc", 0x81b8dc, 0x81ba8a),
]


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    parser.add_argument("--component", choices=("all", "sample", "line", "chain"), default="all")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original kernel hash mismatch")
    original_path = BUILD / "raster_original.x"
    original_path.write_bytes(original)
    objects = []
    for name in ("pixel_sample", "raster_line"):
        obj = BUILD / f"kernel_{name}_preview.o"
        run(["gcc", *FLAGS, "-c", f"kernel/{name}.c", "-o", str(obj)])
        objects.append(str(obj))
    linker, elf = BUILD / "raster_preview.ld", BUILD / "raster_preview.elf"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_pixel_sample_below_81b514)\nSECTIONS {\n" +
        "\n".join(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                    f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
                    for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), *objects])
    preview, sizes = bytearray(original), []
    for name, _, start, end in SLOTS:
        path = BUILD / f"{name}_preview.bin"
        run(["objcopy", "-O", "binary", "-j", f".{name}", str(elf), str(path)])
        body = path.read_bytes()
        if not 0 < len(body) <= end-start:
            raise RuntimeError("invalid body size: " + name)
        preview[start-0x800000:end-0x800000] = body + b"\x90"*(end-start-len(body))
        sizes.append(dict(name=name, start=hex(start), end=hex(end),
                          compiled_bytes=len(body), slot_bytes=end-start))
        disassembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                           f"--start-address={start:#x}", f"--stop-address={end:#x}", str(original_path)],
                          capture_output=True, text=True).stdout
        if re.search(r"\bcall\s", disassembly):
            raise RuntimeError("unexpected original boundary call: " + name)
    preview_path = BUILD / "raster_preview.x"
    preview_path.write_bytes(preview)
    candidate = BUILD / "openicestaros_kernel.x" if args.installed else preview_path
    for _, _, start, end in SLOTS:
        if candidate.read_bytes()[start-0x800000:end-0x800000] != preview[start-0x800000:end-0x800000]:
            raise RuntimeError("installed slot differs from independently linked source")
    paths = [ROOT / p for p in ("kernel/pixel_sample.c", "kernel/pixel_sample.h",
                               "kernel/raster_line.c", "kernel/raster_line.h",
                               "tests/kernel_raster.py", "tests/raster_abi.S", "tests/raster_fixture.h",
                               "tests/kernel_icon_pixels.py", "tests/kernel_raster_line.c")]
    production = BUILD / "openicestaros_kernel.x"
    chain_candidate = bytearray(production.read_bytes())
    for _, _, start, end in SLOTS:
        chain_candidate[start-0x800000:end-0x800000] = preview[start-0x800000:end-0x800000]
    chain_path = BUILD / "raster_chain_candidate.x"
    chain_path.write_bytes(chain_candidate)
    label = "installed" if args.installed else "preview"
    for component, source, phrase in (
        ("sample", "kernel_pixel_sample.c", "sampler verified against 32 KiB memory/return/ABI oracle"),
        ("line", "kernel_raster_line.c", "lines verified against 32 KiB memory/return/live-argument/ABI oracle"),
        ("chain", "kernel_raster_chain.c", "real initializer/mode selection/forwarder/line chain verified"),
    ):
        if args.component not in ("all", component):
            continue
        executable = BUILD / f"kernel_raster_{component}_{label}_test"
        run(["gcc", *FLAGS, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
             "-Wl,-e,_start", "tests/"+source, "tests/raster_abi.S",
             "kernel/pixel_sample.c", "kernel/raster_line.c", "-o", str(executable)])
        start = time.monotonic()
        result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
        summary = dict(installed=args.installed, component=component, exit_code=result.returncode,
                       seconds=round(time.monotonic()-start, 3), slots=sizes, boundary_substitutions=0,
                       stdout=result.stdout, stderr=result.stderr,
                       sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in [*paths, ROOT / "tests" / source, elf, executable,
                                         *([production, chain_path] if component == "chain" else [candidate])]})
        (BUILD / f"raster_{component}_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
        print(json.dumps(summary, indent=2), flush=True)
        if result.returncode or phrase not in result.stdout:
            raise RuntimeError(f"raster {component} fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
