"""Compare the original firmware scans, standalone C and optional installed C."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
sys.path.insert(0, str(ROOT))
from tools.kernel_image import Fat16, ORIGINAL_SHA256  # noqa: E402


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    subprocess.run(["make", "build/openicestaros_kernel.x"], cwd=ROOT, check=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original kernel hash mismatch")
    (BUILD / "firmware_scan_original.x").write_bytes(original)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables", "-fno-delete-null-pointer-checks",
             "-mpreferred-stack-boundary=2", "-Wall", "-Wextra", "-Werror"]
    if args.installed:
        flags.append("-DTEST_INSTALLED")
    objects = []
    for source in ("tests/firmware_scan_qemu.c", "tests/firmware_scan_qemu_entry.S",
                   "kernel/firmware_scan.c"):
        obj = BUILD / (Path(source).stem + "_fixture.o")
        subprocess.run(["gcc", *flags, "-c", source, "-o", str(obj)], cwd=ROOT, check=True)
        objects.append(str(obj))
    elf = BUILD / "firmware_scan_qemu.elf"
    subprocess.run(["ld", "-m", "elf_i386", "-T", "tests/firmware_scan_qemu.ld", "-o", str(elf),
                    *objects], cwd=ROOT, check=True)
    label = "installed" if args.installed else "preview"
    log = BUILD / f"firmware_scan_{label}_qemu.log"
    start = time.monotonic()
    result = subprocess.run(["qemu-system-i386", "-machine", "pc", "-cpu", "max", "-m", "64",
                             "-kernel", str(elf), "-display", "none", "-serial", "none", "-monitor", "none",
                             "-no-reboot", "-no-shutdown", "-debugcon", "file:" + str(log),
                             "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"],
                            cwd=ROOT, timeout=120, capture_output=True, text=True)
    output = log.read_text()
    summary = {"installed": args.installed, "qemu_exit": result.returncode,
               "seconds": round(time.monotonic() - start, 3), "output": output, "stderr": result.stderr}
    summary["sha256"] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (ROOT / "kernel/firmware_scan.c", elf,
                                   BUILD / "firmware_scan_original.x", BUILD / "openicestaros_kernel.x")}
    (BUILD / f"firmware_scan_{label}_summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(output, end="")
    if result.returncode != 33 or "PASS: " not in output:
        raise RuntimeError(f"firmware fixture failed (QEMU exit {result.returncode}): {result.stderr}")


if __name__ == "__main__":
    main()
