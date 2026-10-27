"""Run original/rebuilt IDT, PIC, PIT and RTC routines at CPL 0 in QEMU."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def main():
    subprocess.run(["make", "build/icestar_c.x"], cwd=ROOT, check=True)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic",
             "-fno-pie", "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-Wall", "-Wextra", "-Werror"]
    for source in ("interrupts_qemu.c", "interrupts_qemu_entry.S"):
        subprocess.run(["gcc", *flags, "-c", "tests/" + source, "-o",
                        str(BUILD / (Path(source).stem + ".o"))], cwd=ROOT, check=True)
    elf = BUILD / "interrupts_qemu.elf"
    subprocess.run(["ld", "-m", "elf_i386", "-T", "tests/interrupts_qemu.ld", "-o", str(elf),
                    str(BUILD / "interrupts_qemu_entry.o"), str(BUILD / "interrupts_qemu.o")],
                   cwd=ROOT, check=True)
    log = BUILD / "interrupts_qemu.log"
    result = subprocess.run(["qemu-system-i386", "-machine", "pc", "-cpu", "max", "-m", "64",
                             "-kernel", str(elf), "-display", "none", "-serial", "none", "-monitor", "none",
                             "-icount", "shift=0,align=off,sleep=off",
                             "-rtc", "base=2026-10-27T00:00:00,clock=vm",
                             "-no-reboot", "-no-shutdown", "-debugcon", "file:" + str(log),
                             "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"],
                            cwd=ROOT, timeout=30, capture_output=True, text=True)
    output = log.read_text()
    print(output, end="")
    if result.returncode != 33 or "PASS: 521 CPL0 interrupt comparisons" not in output:
        raise RuntimeError(f"Interrupt fixture failed (QEMU exit {result.returncode}): {result.stderr}")


if __name__ == "__main__":
    main()
