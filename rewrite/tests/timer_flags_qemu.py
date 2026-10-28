"""Execute real timer flag boundaries at CPL0, with only allocator hooks."""
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


def run(command):
    return subprocess.run(command, cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    candidate = BUILD / ("openicestaros_kernel.x" if args.installed else "timer_objects_preview.x")
    BUILD.mkdir(exist_ok=True)
    original = BUILD / "timer_objects_original.x"
    original_data = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    if hashlib.sha256(original_data).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("original timer kernel hash mismatch")
    original.write_bytes(original_data)
    # The original helper bytes are already installed in the production kernel.
    # A standalone preview supplies the same original helper bytes verbatim.
    data = candidate.read_bytes()
    for start, end in ((0x1a242, 0x1a244), (0x1a24a, 0x1a254)):
        if data[start:end] != original.read_bytes()[start:end]:
            raise RuntimeError("timer CPU flag helper bytes changed")
    (BUILD / "timer_flags_fixture_candidate.x").write_bytes(data)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables", "-ffunction-sections",
             "-fdata-sections", "-fno-optimize-sibling-calls", "-mpreferred-stack-boundary=2",
             "-Wall", "-Wextra", "-Werror"]
    objects = []
    for source in ("kernel/timer_objects.c", "kernel/cpu.c", "tests/timer_flags_qemu.c",
                   "tests/timer_flags_qemu_entry.S"):
        obj = BUILD / (Path(source).stem+"_timer_flags_fixture.o")
        run(["gcc", *flags, "-c", source, "-o", str(obj)])
        objects.append(str(obj))
    fixture = BUILD / "timer_flags_qemu.elf"
    run(["ld", "-m", "elf_i386", "--gc-sections", "-T", "tests/timer_flags_qemu.ld",
         "-o", str(fixture), *objects])
    label = "installed" if args.installed else "preview"
    log = BUILD / f"timer_flags_{label}_qemu.log"
    start = time.monotonic()
    result = subprocess.run(["qemu-system-i386", "-machine", "pc", "-cpu", "max", "-m", "64",
        "-kernel", str(fixture), "-display", "none", "-serial", "none", "-monitor", "none",
        "-no-reboot", "-no-shutdown", "-debugcon", "file:"+str(log),
        "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"], cwd=ROOT,
        timeout=30, capture_output=True, text=True)
    output = log.read_text()
    paths = [ROOT / p for p in ("tests/timer_flags_qemu.c", "tests/timer_flags_qemu_entry.S",
                               "tests/timer_flags_qemu.ld", "tests/timer_flags_qemu.py")]
    summary = {"installed": args.installed, "qemu_exit": result.returncode,
               "seconds": round(time.monotonic()-start, 3), "output": output,
               "stderr": result.stderr, "scenarios": 48, "executions": 144,
               "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, fixture, candidate, original]}}
    (BUILD / f"timer_flags_{label}_summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    print(json.dumps(summary, indent=2))
    if result.returncode != 33 or "PASS: 48 timer CPL0 scenarios, 144 executions" not in output:
        raise RuntimeError(f"timer flags fixture failed (QEMU exit {result.returncode})")


if __name__ == "__main__":
    main()
