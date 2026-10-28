"""Compare original, ordinary C and fixed-address C JPEG tables parsers."""
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


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    disk = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes())
    original = disk.read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    original_path = BUILD / "jpeg_tables_original.x"
    original_path.write_bytes(original)
    obj, elf, linker = (BUILD / ("jpeg_tables_preview" + s) for s in (".o", ".elf", ".ld"))
    run(["gcc", *FLAGS, "-c", "kernel/jpeg_tables.c", "-o", str(obj)])
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_jpeg_tables_83aae7)\nSECTIONS {\n"
        " .jpeg_tables 0x83aae7 : SUBALIGN(1) { *(.text.kernel_jpeg_tables_83aae7) }\n"
        " ASSERT(kernel_jpeg_tables_83aae7 == 0x83aae7 && SIZEOF(.jpeg_tables) <= 404, \"slot overflow\")\n"
        " /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    binary = BUILD / "jpeg_tables_preview.bin"
    run(["objcopy", "-O", "binary", "-j", ".jpeg_tables", str(elf), str(binary)])
    body = binary.read_bytes()
    assert 0 < len(body) <= 404
    production = BUILD / "openicestaros_kernel.x"
    candidate = bytearray(production.read_bytes())
    replacement = body + b"\x90" * (404-len(body))
    if args.installed:
        assert candidate[0x3aae7:0x3ac7b] == replacement
    candidate[0x3aae7:0x3ac7b] = replacement
    candidate_path = BUILD / "jpeg_tables_candidate.x"
    candidate_path.write_bytes(candidate)
    disassembly = run(["objdump", "-D", "-b", "binary", "-mi386", "-Mintel",
        "--adjust-vma=0x800000", "--start-address=0x83aae7", "--stop-address=0x83ac7b",
        str(original_path)], capture_output=True, text=True).stdout
    (BUILD / "jpeg_tables_original.disasm").write_text(disassembly)
    jpeg = disk.read_file(b"ICESTAR    ", b"IMAGE      ", b"STAR24  JPG")
    fixture = BUILD / "jpeg_tables_fixture.jpg"
    fixture.write_bytes(jpeg)
    offset = 2
    while jpeg[offset+1] != 0xc0:
        assert jpeg[offset] == 0xff
        offset += 2 + int.from_bytes(jpeg[offset+2:offset+4], "big")
    height = int.from_bytes(jpeg[offset+5:offset+7], "big")
    width = int.from_bytes(jpeg[offset+7:offset+9], "big")
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_jpeg_tables_{label}_test"
    run(["gcc", *FLAGS, f"-DJPEG_FILE_BYTES={len(jpeg)}", f"-DJPEG_WIDTH={width}",
        f"-DJPEG_HEIGHT={height}", "-nostdlib", "-static", "-Wl,-e,_start,--gc-sections",
        "tests/kernel_jpeg_tables.c", "tests/raster_abi.S", str(obj), "-o", str(executable)])
    started = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=900)
    paths = [ROOT / p for p in ("kernel/jpeg_tables.c", "kernel/jpeg_tables.h",
        "tests/kernel_jpeg_tables.c", "tests/kernel_jpeg_tables.py", "tests/raster_abi.S",
        "tests/kernel_icon_pixels.py")]
    summary = dict(installed=args.installed, exit_code=result.returncode,
        seconds=round(time.monotonic()-started, 3), compiled_bytes=len(body), slot_bytes=404,
        helper_substitutions=0, limitation="Bounded bytewise oracle excludes unsafe memory. Only the quantization parser is recovered; real info/decode composition retains unrecovered JPEG routines.",
        stdout=result.stdout, stderr=result.stderr,
        sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [*paths, elf, executable, candidate_path, production, fixture]})
    (BUILD / f"jpeg_tables_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2), flush=True)
    if result.returncode or "jpeg tables verified" not in result.stdout:
        raise RuntimeError(f"JPEG tables fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
