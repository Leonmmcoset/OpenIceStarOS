"""Execute DIB rendering source against original x86 and a bytewise oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

START, END = 0x82b20c, 0x82b678
BOUNDARIES = {
    0x81b37c: "kernel_icon_put8_81b37c",
    0x81b3c0: "kernel_icon_put32_81b3c0",
    0x81fd90: "kernel_icon_quantize_81fd90",
}
FLAGS = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
         "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
         "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
         "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)


def resource_fixtures(disk):
    packages = []
    for name in ("QQ", "STAR", "DISK", "TASKBAR"):
        data = disk.read_file(b"ICESTAR    ", b"ICO        ", (name.ljust(8)+"ICO").encode())
        packages.append((name+".ICO", data))
    for name in ("CMD", "TASKMGR", "PICVIEW", "DESKTOP"):
        data = disk.read_file(b"ICESTAR    ", (name.ljust(8)+"X  ").encode())
        offset = struct.unpack_from("<I", data, 40)[0]
        if not 0 < offset < len(data):
            raise RuntimeError("invalid original X icon offset")
        packages.append((name+".X", data[offset:]))
    fixtures, resources = [], []
    for name, data in packages:
        zero, kind, count = struct.unpack_from("<HHH", data)
        if zero or kind != 1 or not 0 < count <= 32:
            raise RuntimeError("invalid original ICO header")
        for frame in range(count):
            size, offset = struct.unpack_from("<II", data, 6+16*frame+8)
            payload = data[offset:offset+size]
            if len(payload) != size:
                raise RuntimeError("truncated original ICO payload")
            if payload.startswith(b"\x89PNG\r\n\x1a\n"):
                continue
            width, height = struct.unpack_from("<II", payload, 4)
            depth = struct.unpack_from("<H", payload, 14)[0]
            if len(payload) < 40 or width > 65 or height > 128:
                raise RuntimeError("resource exceeds mapped fixture bounds")
            fixtures.append("{"+str(size)+", {"+",".join(str(b) for b in payload)+"}}")
            resources.append(dict(name=name, frame=frame, bytes=size, width=width,
                                  doubled_height=height, depth=depth,
                                  sha256=hashlib.sha256(payload).hexdigest()))
    largest = max(p["bytes"] for p in resources)
    (BUILD / "icon_dib_resources.h").write_text(
        "static const struct { uint32_t size; uint8_t bytes["+str(largest)+
        "]; } resource_fixtures[] = {\n"+",\n".join(fixtures)+"\n};\n")
    return resources


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    disk = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes())
    original = disk.read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original kernel hash mismatch")
    original_path = BUILD / "icon_dib_original.x"
    original_path.write_bytes(original)
    obj, elf = BUILD / "kernel_icon_dib_preview.o", BUILD / "icon_dib_preview.elf"
    run(["gcc", *FLAGS, "-c", "kernel/icon_dib.c", "-o", str(obj)])
    linker = BUILD / "icon_dib_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_icon_dib_82b20c)\n"+
        "\n".join(f"PROVIDE({symbol} = {address:#x});" for address, symbol in BOUNDARIES.items())+
        f"\nSECTIONS {{\n .icon_dib {START:#x} : SUBALIGN(1) {{ *(.text.kernel_icon_dib_82b20c) }}\n"
        f" ASSERT(kernel_icon_dib_82b20c == {START:#x} && SIZEOF(.icon_dib) <= {END-START}, \"DIB overflow\")\n"
        " /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    binary = BUILD / "icon_dib_preview.bin"
    run(["objcopy", "-O", "binary", "-j", ".icon_dib", str(elf), str(binary)])
    body = binary.read_bytes()
    if not 0 < len(body) <= END-START:
        raise RuntimeError("invalid DIB body size")
    slot = body + b"\x90" * (END-START-len(body))
    preview = bytearray(original)
    preview[START-0x800000:END-0x800000] = slot
    preview_path = BUILD / "icon_dib_preview.x"
    preview_path.write_bytes(preview)
    candidate = BUILD / "openicestaros_kernel.x" if args.installed else preview_path
    if candidate.read_bytes()[START-0x800000:END-0x800000] != slot:
        raise RuntimeError("installed DIB slot does not match independently linked source")
    patches = []
    call_counts = {}
    for label, path, base in (("original", original_path, 0x900000), ("candidate", candidate, 0x800000)):
        disassembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                           f"--start-address={START:#x}", f"--stop-address={END:#x}", str(path)],
                          capture_output=True, text=True).stdout
        calls = [(int(a, 16), int(t, 16)) for a, t in re.findall(
            r"^\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", disassembly, re.M)]
        if set(t for _, t in calls) != set(BOUNDARIES):
            raise RuntimeError("unexpected DIB calls: "+repr(calls))
        if label == "original" and sorted(t for _, t in calls) != sorted(list(BOUNDARIES)*2):
            raise RuntimeError("unexpected original DIB call multiplicity")
        call_counts[label] = len(calls)
        for address, target in calls:
            call = base + address - 0x800000
            patches.append(f"    DWORD({call+1:#x}) = (uint32_t){BOUNDARIES[target]} - {call+5:#x};")
    (BUILD / "icon_dib_calls.h").write_text("\n".join(patches)+"\n")
    resources = resource_fixtures(disk)
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_icon_dib_{label}_test"
    run(["gcc", *FLAGS, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_icon_dib.c", "tests/icon_dib_abi.S", "kernel/icon_dib.c",
         "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=360)
    paths = [ROOT / p for p in ("kernel/icon_dib.c", "kernel/icon_dib.h", "tests/kernel_icon_dib.c",
                               "tests/kernel_icon_dib.py", "tests/icon_dib_abi.S")]
    summary = dict(installed=args.installed, exit_code=result.returncode,
                   seconds=round(time.monotonic()-start, 3), compiled_bytes=len(body), slot_bytes=END-START,
                   boundary_calls=call_counts, resources=resources, stdout=result.stdout, stderr=result.stderr,
                   sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in [*paths, elf, executable, candidate]})
    (BUILD / f"icon_dib_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "DIB decoder verified" not in result.stdout:
        raise RuntimeError(f"DIB fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
