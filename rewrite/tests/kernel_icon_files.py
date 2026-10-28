"""Build three recovered file-icon APIs and compare source/fixed C to x86."""
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

SLOTS = (
    ("x_icon_pointer", "kernel_x_icon_pointer", 0x812c0e, 0x812c3e),
    ("icon_draw_file", "kernel_icon_draw_file", 0x82b0a0, 0x82b112),
    ("icon_draw_file_auto", "kernel_icon_draw_file_auto", 0x82b112, 0x82b20c),
)
BOUNDARIES = {
    0x812c0e: "icon_mock_x_pointer", 0x815978: "kernel_free_allocation",
    0x826f38: "kernel_icon_read_relative_826f38",
    0x827798: "kernel_icon_read_absolute_827798",
    0x82a45d: "kernel_string_length", 0x82af08: "kernel_icon_draw_id",
}


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
    (BUILD / "icon_files_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    objects = []
    for name in ("icon_files", "x_icon"):
        obj = BUILD / f"kernel_{name}_preview.o"
        run(["gcc", *flags, "-c", f"kernel/{name}.c", "-o", str(obj)])
        objects.append(str(obj))
    linker = BUILD / "icon_files_preview.ld"
    provides = {**BOUNDARIES}
    del provides[0x812c0e]  # Real fixed-address getter, separately tested.
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_x_icon_pointer)\n" +
        "\n".join(f"PROVIDE({name} = {address:#x});" for address, name in provides.items()) +
        "\nSECTIONS {\n" +
        "\n".join(f" .{section} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                  f" ASSERT({symbol} == {start:#x} && SIZEOF(.{section}) <= {end-start}, \"{section} overflow\")"
                  for section, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    elf = BUILD / "icon_files_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), *objects])
    preview, sizes = bytearray(original), {}
    candidate = BUILD / ("icestar_c.x" if args.installed else "icon_files_preview.x")
    installed = candidate.read_bytes() if args.installed else None
    for section, symbol, start, end in SLOTS:
        binary = BUILD / f"icon_files_preview.{section}.bin"
        run(["objcopy", "-O", "binary", "-j", "."+section, str(elf), str(binary)])
        body = binary.read_bytes()
        if not 0 < len(body) <= end-start:
            raise RuntimeError("invalid " + section + " size")
        slot = body + b"\x90" * (end-start-len(body))
        preview[start-0x800000:end-0x800000] = slot
        if installed is not None and installed[start-0x800000:end-0x800000] != slot:
            raise RuntimeError("installed bytes differ from compiled source: " + section)
        sizes[section] = {"compiled_bytes": len(body), "slot_bytes": end-start}
    (BUILD / "icon_files_preview.x").write_bytes(preview)
    patches = []
    for path, base in ((BUILD / "icon_files_original.x", 0x900000), (candidate, 0x800000)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x82b0a0", "--stop-address=0x82b20c", str(path)],
                       capture_output=True, text=True).stdout
        calls = [(int(a, 16), int(t, 16)) for a, t in re.findall(
            r"^\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", assembly, re.M)]
        expected = [0x812c0e, 0x815978, 0x815978, 0x826f38, 0x826f38,
                    0x827798, 0x827798, 0x82a45d, 0x82af08, 0x82af08]
        if sorted(t for _, t in calls) != expected:
            raise RuntimeError("unexpected file icon calls: " + repr(calls))
        for address, target in calls:
            call = base + address - 0x800000
            patches.append(f"    *(volatile uint32_t *){call+1:#x} = (uint32_t){BOUNDARIES[target]} - {call+5:#x};")
    (BUILD / "icon_files_calls.h").write_text("\n".join(patches)+"\n")
    headers, resources = [], []
    applications = json.loads((ROOT / "apps/images.json").read_text())
    applications += [{"name": name, "path": ["ICESTAR    ", name.ljust(8)+"X  "]}
                     for name in ("CMD", "TASKMGR", "PICVIEW", "DESKTOP")]
    for app in applications:
        data = disk.read_file(*(name.encode() for name in app["path"]))
        if "original_sha256" in app and hashlib.sha256(data).hexdigest() != app["original_sha256"]:
            raise RuntimeError("original application hash mismatch")
        offset = struct.unpack_from("<I", data, 40)[0]
        if offset and (offset+6 > len(data) or data[offset:offset+4] != b"\0\0\1\0"):
            raise RuntimeError("original X icon is not an ICO directory")
        headers.append("{"+",".join(str(b) for b in data[:64])+"}")
        resources.append({"name": app["name"], "icon_offset": offset,
                          "file_size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    (BUILD / "icon_files_resources.h").write_text(
        "static const uint8_t resource_headers[][64] = {\n"+",\n".join(headers)+"\n};\n")
    mock_obj = BUILD / "kernel_icon_files_mock.o"
    run(["gcc", *flags, "-Dkernel_x_icon_pointer=icon_mock_x_pointer", "-c",
         "kernel/icon_files.c", "-o", str(mock_obj)])
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_icon_files_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_icon_files.c", "tests/icon_files_abi.S",
         str(mock_obj), "kernel/x_icon.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=240)
    paths = [ROOT / p for p in ("kernel/icon_files.c", "kernel/icon_files.h", "kernel/x_icon.c",
             "kernel/x_icon.h", "tests/kernel_icon_files.c", "tests/kernel_icon_files.py", "tests/icon_files_abi.S")]
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "slots": sizes, "resources": resources,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, elf, executable, candidate]}}
    (BUILD / f"icon_files_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "all three file icon APIs verified" not in result.stdout:
        raise RuntimeError(f"file icon fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
