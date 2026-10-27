"""Run the original/rebuilt raw CMOS sampler with real privileged I/O."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def main():
    subprocess.run(["make", "build/icestar_c.x"], cwd=ROOT, check=True)
    subprocess.run(["python3", "tests/cmos_clock_exact.py", "--installed"], cwd=ROOT, check=True)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables", "-Wall", "-Wextra", "-Werror"]
    for source in ("cmos_clock_qemu.c", "cmos_clock_qemu_entry.S"):
        subprocess.run(["gcc", *flags, "-c", "tests/" + source, "-o",
                        str(BUILD / (Path(source).stem + ".o"))], cwd=ROOT, check=True)
    elf = BUILD / "cmos_clock_qemu.elf"
    subprocess.run(["ld", "-m", "elf_i386", "-T", "tests/interrupts_qemu.ld", "-o", str(elf),
                    str(BUILD / "cmos_clock_qemu_entry.o"), str(BUILD / "cmos_clock_qemu.o")],
                   cwd=ROOT, check=True)
    log = BUILD / "cmos_clock_qemu.log"
    result = subprocess.run(["qemu-system-i386", "-machine", "pc", "-cpu", "max", "-m", "64",
                             "-kernel", str(elf), "-display", "none", "-serial", "none", "-monitor", "none",
                             "-icount", "shift=0,align=off,sleep=off", "-rtc", "base=2026-10-27T00:00:00,clock=vm",
                             "-no-reboot", "-no-shutdown", "-debugcon", "file:" + str(log),
                             "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"],
                            cwd=ROOT, timeout=30, capture_output=True, text=True)
    output = log.read_text()
    print(output, end="")
    if result.returncode != 33 or "PASS: 256 CPL0 CMOS" not in output:
        raise RuntimeError(f"CMOS fixture failed (QEMU exit {result.returncode}): {result.stderr}")


if __name__ == "__main__":
    main()
