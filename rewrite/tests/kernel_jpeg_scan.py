"""Execute three scan helpers against original code and complete JPEG decoding."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import time

from kernel_icon_pixels import FLAGS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = [
    ("jpeg_info", "kernel_jpeg_info_83a70f", 0x83a70f, 0x83a7a1),
    ("jpeg_decode", "kernel_jpeg_decode_83a7a1", 0x83a7a1, 0x83a835),
    ("jpeg_scan", "kernel_jpeg_scan_83af27", 0x83af27, 0x83b233),
    ("jpeg_prepare", "kernel_jpeg_prepare_83b233", 0x83b233, 0x83b351),
    ("jpeg_symbol", "kernel_jpeg_symbol_83b351", 0x83b351, 0x83b41b),
    ("jpeg_basis", "kernel_jpeg_basis_83b41b", 0x83b41b, 0x83b653),
    ("jpeg_transform", "kernel_jpeg_transform_83b653", 0x83b653, 0x83b73f),
    ("jpeg_magnitude", "kernel_jpeg_magnitude_83b73f", 0x83b73f, 0x83b7ab),
    ("jpeg_block", "kernel_jpeg_block_83b7ab", 0x83b7ab, 0x83b96d),
    ("jpeg_expand", "kernel_jpeg_expand_83b96d", 0x83b96d, 0x83ba4f),
    ("jpeg_mcu", "kernel_jpeg_mcu_83ba4f", 0x83ba4f, 0x83bc15),
    ("jpeg_color", "kernel_jpeg_color_83bc15", 0x83bc15, 0x83be59),
    ("jpeg_image", "kernel_jpeg_image_83be59", 0x83be59, 0x83c02f),
]


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
    original_path = BUILD / "jpeg_scan_original.x"
    original_path.write_bytes(original)
    obj, elf, linker = (BUILD / ("jpeg_scan_preview" + s) for s in (".o", ".elf", ".ld"))
    run(["gcc", *FLAGS, "-c", "kernel/jpeg_scan.c", "-o", str(obj)])
    basis_obj = BUILD / "jpeg_basis_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/jpeg_basis.c", "-o", str(basis_obj)])
    transform_obj = BUILD / "jpeg_transform_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/jpeg_transform.c", "-o", str(transform_obj)])
    block_obj = BUILD / "jpeg_block_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/jpeg_block.c", "-o", str(block_obj)])
    output_obj = BUILD / "jpeg_output_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/jpeg_output.c", "-o", str(output_obj)])
    decode_obj = BUILD / "jpeg_decode_preview.o"
    run(["gcc", *FLAGS, "-c", "kernel/jpeg_decode.c", "-o", str(decode_obj)])
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_jpeg_scan_83af27)\nSECTIONS {\n" +
        "\n".join(f" .{name} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) " +
                    ("*jpeg_basis_preview.o(.rodata*) " if name == "jpeg_basis" else
                     "*jpeg_decode_preview.o(.rodata*) " if name == "jpeg_image" else "") + "}\n" +
                    f" ASSERT({symbol} == {start:#x} && SIZEOF(.{name}) <= {end-start}, \"slot overflow\")"
                    for name, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj), str(basis_obj), str(transform_obj), str(block_obj), str(output_obj), str(decode_obj)])
    production = BUILD / "openicestaros_kernel.x"
    # Original neighbours isolate the new source; the installed check also
    # exercises all previously recovered JPEG helpers in the production image.
    candidate = bytearray(production.read_bytes() if args.installed else original)
    sizes = []
    for name, _, start, end in SLOTS:
        binary = BUILD / f"{name}_preview.bin"
        run(["objcopy", "-O", "binary", "-j", f".{name}", str(elf), str(binary)])
        body = binary.read_bytes()
        assert 0 < len(body) <= end-start
        replacement = body + b"\x90"*(end-start-len(body))
        if args.installed:
            assert candidate[start-0x800000:end-0x800000] == replacement
        candidate[start-0x800000:end-0x800000] = replacement
        sizes.append(dict(name=name, start=hex(start), compiled_bytes=len(body), slot_bytes=end-start))
    candidate_path = BUILD / "jpeg_scan_candidate.x"
    candidate_path.write_bytes(candidate)
    disassembly = run(["objdump", "-D", "-b", "binary", "-mi386", "-Mintel",
        "--adjust-vma=0x800000", "--start-address=0x83a70f", "--stop-address=0x83c02f",
        str(original_path)], capture_output=True, text=True).stdout
    (BUILD / "jpeg_scan_original.disasm").write_text(disassembly)
    jpeg = disk.read_file(b"ICESTAR    ", b"IMAGE      ", b"STAR24  JPG")
    fixture = BUILD / "jpeg_scan_fixture.jpg"
    fixture.write_bytes(jpeg)
    corpus_dir = BUILD / "jpeg_corpus"
    corpus_dir.mkdir(exist_ok=True)
    corpus = []
    paths = [(b"ICESTAR    ", b"IMAGE      ", f"{name:<8}JPG".encode())
             for name in ("STAR24", "BJTP", "BJTP1", "BJTP2", "BJTP3", "BJTP4", "BJTP5", "BJTP6")]
    paths += [(b"ICESTAR    ", b"GAMES      ", b"MOV        ", f"MOV{i:<5}JPG".encode()) for i in range(4)]
    for path in paths:
        data = disk.read_file(*path)
        cursor = 2
        while data[cursor+1] != 0xc0:
            assert data[cursor] == 0xff
            cursor += 2 + int.from_bytes(data[cursor+2:cursor+4], "big")
        w = int.from_bytes(data[cursor+7:cursor+9], "big")
        h = int.from_bytes(data[cursor+5:cursor+7], "big")
        file = corpus_dir / (path[-1][:8].strip().decode().lower()+".jpg")
        file.write_bytes(data)
        corpus.append(dict(path=str(file.relative_to(ROOT)), bytes=len(data), width=w, height=h,
                           sha256=hashlib.sha256(data).hexdigest()))
    (BUILD / "jpeg_corpus.h").write_text("static const struct { const char *path; unsigned bytes,width,height; } corpus[]={\n" +
        "\n".join(f' {{"{p["path"]}",{p["bytes"]},{p["width"]},{p["height"]}}},' for p in corpus) + "\n};\n")
    # Independent mathematical oracle: quantize cosines from libm, then build
    # every basis coefficient, without reading constants from the recovery.
    cosine = [round(32768*math.cos(math.pi*i/16)) for i in range(32)]
    basis = [cosine[((2*x+1)*u) % 32 if u else 4] *
             cosine[((2*y+1)*v) % 32 if v else 4] >> 15
             for u in range(8) for v in range(8) for x in range(8) for y in range(8)]
    (BUILD / "jpeg_basis_oracle.bin").write_bytes(struct.pack("<4096i", *basis))
    offset = 2
    while jpeg[offset+1] != 0xc0:
        assert jpeg[offset] == 0xff
        offset += 2 + int.from_bytes(jpeg[offset+2:offset+4], "big")
    height = int.from_bytes(jpeg[offset+5:offset+7], "big")
    width = int.from_bytes(jpeg[offset+7:offset+9], "big")
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_jpeg_scan_{label}_test"
    run(["gcc", *FLAGS, "-DRASTER_ARGUMENT_WORDS=8", f"-DJPEG_FILE_BYTES={len(jpeg)}", f"-DJPEG_WIDTH={width}",
        f"-DJPEG_HEIGHT={height}", "-nostdlib", "-static", "-Wl,-e,_start,--gc-sections",
        "tests/kernel_jpeg_scan.c", "tests/raster_abi.S", str(obj), str(basis_obj), str(transform_obj), str(block_obj), str(output_obj), str(decode_obj), "-o", str(executable)])
    started = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=240)
    paths = [ROOT / p for p in ("kernel/jpeg_scan.c", "kernel/jpeg_scan.h",
        "kernel/jpeg_basis.c", "kernel/jpeg_basis.h",
        "kernel/jpeg_transform.c", "kernel/jpeg_transform.h",
        "kernel/jpeg_block.c", "kernel/jpeg_block.h",
        "kernel/jpeg_output.c", "kernel/jpeg_output.h",
        "kernel/jpeg_decode.c", "kernel/jpeg_decode.h",
        "tests/kernel_jpeg_scan.c", "tests/kernel_jpeg_scan.py", "tests/raster_abi.S")]
    summary = dict(installed=args.installed, exit_code=result.returncode,
        seconds=round(time.monotonic()-started, 3), slots=sizes, helper_substitutions=0, corpus=corpus,
        limitation="Differential execution covers mapped state, caller words and ABI; fault probes compare termination. Malformed MCU paths using uninitialized local coefficients are not established equivalent. Other OS components remain original.",
        stdout=result.stdout, stderr=result.stderr,
        sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [*paths, elf, executable, candidate_path, production, fixture]})
    (BUILD / f"jpeg_scan_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2), flush=True)
    if result.returncode or "jpeg scan verified" not in result.stdout:
        raise RuntimeError(f"JPEG scan fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
