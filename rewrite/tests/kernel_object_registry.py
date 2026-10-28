"""Original/C/fixed-address object registry recovery, without changing disks."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402

SLOTS = (
    ("object_find_name", "kernel_object_find_name", 0x8104a0, 0x810520),
    ("object_set_state", "kernel_object_set_state", 0x810520, 0x81054e),
    ("object_registry_add", "kernel_object_registry_add", 0x81054e, 0x8105dc),
    ("object_registry_remove", "kernel_object_registry_remove", 0x8105dc, 0x81067a),
)


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
    (BUILD / "object_registry_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-fno-optimize-sibling-calls",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    obj = BUILD / "kernel_object_registry_preview.o"
    run(["gcc", *flags, "-c", "kernel/object_registry.c", "-o", str(obj)])
    linker = BUILD / "object_registry_preview.ld"
    linker.write_text("OUTPUT_FORMAT(elf32-i386)\nENTRY(kernel_object_find_name)\n"
                      "PROVIDE(kernel_strings_equal = 0x82a481);\nSECTIONS {\n" +
        "\n".join(f" .{section} {start:#x} : SUBALIGN(1) {{ *(.text.{symbol}) }}\n"
                  f" ASSERT({symbol} == {start:#x} && SIZEOF(.{section}) <= {end-start}, \"{section} overflow\")"
                  for section, symbol, start, end in SLOTS) +
        "\n /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }\n"
        " .unexpected : { *(*) }\n ASSERT(SIZEOF(.unexpected) == 0, \"unexpected sections\")\n}\n")
    elf = BUILD / "object_registry_preview.elf"
    run(["ld", "-m", "elf_i386", "-T", str(linker), "-o", str(elf), str(obj)])
    preview = bytearray(original)
    sizes = {}
    for section, symbol, start, end in SLOTS:
        binary = BUILD / f"object_registry_preview.{section}.bin"
        run(["objcopy", "-O", "binary", "-j", "." + section, str(elf), str(binary)])
        body = binary.read_bytes()
        if not 0 < len(body) <= end-start:
            raise RuntimeError("invalid " + section + " size")
        preview[start-0x800000:end-0x800000] = body + b"\x90" * (end-start-len(body))
        sizes[section] = {"compiled_bytes": len(body), "slot_bytes": end-start}
    (BUILD / "object_registry_preview.x").write_bytes(preview)
    candidate = BUILD / "icestar_c.x" if args.installed else BUILD / "object_registry_preview.x"
    patches = []
    for name, path in (("original", BUILD / "object_registry_original.x"), ("candidate", candidate)):
        assembly = run(["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
                        "--start-address=0x8104a0", "--stop-address=0x81067a", str(path)],
                       capture_output=True, text=True).stdout
        calls = re.findall(r"^\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", assembly, re.M)
        if len(calls) != 1 or int(calls[0][1], 16) != 0x82a481:
            raise RuntimeError("unexpected object registry external calls")
        patches.append(f"#define {name.upper()}_COMPARE_CALL {int(calls[0][0],16)-0x800000:#x}u")
    (BUILD / "object_registry_calls.h").write_text("\n".join(patches) + "\n")
    label = "installed" if args.installed else "preview"
    executable = BUILD / f"kernel_object_registry_{label}_test"
    run(["gcc", *flags, *(["-DTEST_INSTALLED"] if args.installed else []), "-nostdlib", "-static",
         "-Wl,-e,_start", "tests/kernel_object_registry.c", "tests/object_registry_abi.S",
         "kernel/object_registry.c", "-o", str(executable)])
    start = time.monotonic()
    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=180)
    paths = [ROOT / p for p in ("kernel/object_registry.c", "kernel/object_registry.h",
             "tests/kernel_object_registry.c", "tests/kernel_object_registry.py", "tests/object_registry_abi.S")]
    summary = {"installed": args.installed, "exit_code": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "slots": sizes,
               "stdout": result.stdout, "stderr": result.stderr,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, elf, executable, candidate]}}
    (BUILD / f"object_registry_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode or "all four object helpers verified" not in result.stdout:
        raise RuntimeError(f"object registry fixture failed: {result.returncode}")


if __name__ == "__main__":
    main()
