"""Compare surface-ID pixel services against original x86 and a memory oracle."""
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
    ("pixel_put8", "kernel_pixel_put8_81b26e", 0x81b26e, 0x81b2f4),
    ("pixel_put32", "kernel_pixel_put32_81b2f4", 0x81b2f4, 0x81b37c),
    ("pixel_get8", "kernel_pixel_get8_81b406", 0x81b406, 0x81b486),
    ("pixel_get32", "kernel_pixel_get32_81b486", 0x81b486, 0x81b514),
]


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original kernel hash mismatch")
    original_path = BUILD / "pixel_access_original.x"
    original_path.write_bytes(original)
    obj, elf = BUILD / "kernel_pixel_access_preview.o", BUILD / "pixel_access_preview.elf"
    run(["gcc", *FLAGS, "-c", "kernel/pixel_access.c", "-o", str(obj)])
    linker = BUILD / "pixel_access_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_pixel_put8_81b26e)\nSECTIONS {\n" +
        "\n".join(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                    f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
                    for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
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
    preview_path = BUILD / "pixel_access_preview.x"
    preview_path.write_bytes(preview)
    candidate = BUILD / "icestar_c.x" if args.installed else preview_path
    for _, _, start, end in SLOTS:
        if candidate.read_bytes()[start-0x800000:end-0x800000] != preview[start-0x800000:end-0x800000]:
            raise RuntimeError("installed slot differs from independently linked source")
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_pixel_access_{label}_test"
    run(["gcc", *FLAGS, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_pixel_access.c", "tests/icon_pixels_abi.S",
         "kernel/pixel_access.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
    paths = [ROOT / p for p in ("kernel/pixel_access.c", "kernel/pixel_access.h",
                               "tests/kernel_pixel_access.c", "tests/kernel_pixel_access.py",
                               "tests/icon_pixels_abi.S", "tests/kernel_icon_pixels.py")]
    summary = dict(installed=args.installed, exit_code=result.returncode,
                   seconds=round(time.monotonic()-start, 3), slots=sizes,
                   boundary_substitutions=0, stdout=result.stdout, stderr=result.stderr,
                   sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in [*paths, elf, executable, candidate]})
    (BUILD / f"pixel_access_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "surface-ID services verified against 12 KiB memory/ABI oracle" not in result.stdout:
        raise RuntimeError(f"surface-ID pixel fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
