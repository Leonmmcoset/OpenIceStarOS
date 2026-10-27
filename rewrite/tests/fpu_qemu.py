"""Compile and run the isolated CPL0 original/C FPU fixture in QEMU."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def main():
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic",
             "-fno-pie", "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-Wall", "-Wextra", "-Werror"]
    subprocess.run(["make", "build/icestar_c.x"], cwd=ROOT, check=True)
    for source in ("fpu_qemu.c", "fpu_qemu_entry.S"):
        output = BUILD / (source.replace(".S", ".o").replace(".c", ".o"))
        subprocess.run(["gcc", *flags, "-c", "tests/" + source, "-o", str(output)], cwd=ROOT, check=True)
    elf = BUILD / "fpu_qemu.elf"
    subprocess.run(["ld", "-m", "elf_i386", "-T", "tests/fpu_qemu.ld", "-o", str(elf),
                    str(BUILD / "fpu_qemu_entry.o"), str(BUILD / "fpu_qemu.o")], cwd=ROOT, check=True)
    log = BUILD / "fpu_qemu.log"
    command = ["qemu-system-i386", "-machine", "pc", "-cpu", "max", "-m", "64",
               "-kernel", str(elf), "-display", "none", "-serial", "none", "-monitor", "none",
               "-no-reboot", "-no-shutdown", "-debugcon", "file:" + str(log),
               "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"]
    result = subprocess.run(command, cwd=ROOT, timeout=30, capture_output=True, text=True)
    output = log.read_text()
    print(output, end="")
    if result.returncode != 33 or "PASS: 800 real-hardware-boundary comparisons" not in output:
        raise RuntimeError(f"FPU fixture failed (QEMU exit {result.returncode}): {result.stderr}")


if __name__ == "__main__":
    main()
