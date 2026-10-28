"""Test the complete recovered DIB decoder with its real pixel and color helpers."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from kernel_icon_dib import FLAGS, resource_fixtures
from kernel_icon_pixels import SLOTS as PIXEL_SLOTS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [*PIXEL_SLOTS, ("icon_dib", "kernel_icon_dib_82b20c", 0x82b20c, 0x82b678)]


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    disk = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes())
    original = disk.read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original kernel hash mismatch")
    original_path = BUILD / "icon_render_original.x"
    original_path.write_bytes(original)
    objects = []
    for name in ("icon_dib", "icon_pixels"):
        obj = BUILD / f"kernel_{name}_render.o"
        run(["gcc", *FLAGS, "-c", f"kernel/{name}.c", "-o", str(obj)])
        objects.append(str(obj))
    linker, elf = BUILD / "icon_render_preview.ld", BUILD / "icon_render_preview.elf"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_icon_dib_82b20c)\nSECTIONS {\n" +
        "\n".join(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                    f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
                    for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), *objects])
    preview, sizes = bytearray(original), []
    for name, _, start, end in SLOTS:
        path = BUILD / f"{name}_render.bin"
        run(["objcopy", "-O", "binary", "-j", f".{name}", str(elf), str(path)])
        body = path.read_bytes()
        if not 0 < len(body) <= end-start:
            raise RuntimeError("invalid body size: " + name)
        preview[start-0x800000:end-0x800000] = body + b"\x90"*(end-start-len(body))
        sizes.append(dict(name=name, compiled_bytes=len(body), slot_bytes=end-start))
    preview_path = BUILD / "icon_render_preview.x"
    preview_path.write_bytes(preview)
    candidate = BUILD / "openicestaros_kernel.x" if args.installed else preview_path
    for _, _, start, end in SLOTS:
        if candidate.read_bytes()[start-0x800000:end-0x800000] != preview[start-0x800000:end-0x800000]:
            raise RuntimeError("installed chain slot differs from independently linked source")
    resources = resource_fixtures(disk)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_icon_render_{label}_test"
    run(["gcc", *FLAGS, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_icon_render.c", "tests/icon_dib_abi.S",
         "kernel/icon_dib.c", "kernel/icon_pixels.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=360)
    paths = [ROOT / p for p in ("kernel/icon_pixels.c", "kernel/icon_pixels.h", "kernel/icon_dib.c",
                               "kernel/icon_dib.h", "tests/kernel_icon_render.c", "tests/kernel_icon_render.py",
                               "tests/icon_dib_abi.S", "tests/kernel_icon_dib.py", "tests/kernel_icon_pixels.py")]
    summary = dict(installed=args.installed, exit_code=result.returncode,
                   seconds=round(time.monotonic()-start, 3), slots=sizes,
                   boundary_substitutions=0, resources=resources, stdout=result.stdout, stderr=result.stderr,
                   sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in [*paths, elf, executable, candidate]})
    (BUILD / f"icon_render_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "chain verified against 64 KiB memory/ABI oracle" not in result.stdout:
        raise RuntimeError(f"complete render fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
