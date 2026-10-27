"""Compare complete hardware interrupt/dispatcher chains at CPL0 and CPL3."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--installed", action="store_true")
    args = parser.parse_args()
    subprocess.run(["make", "build/icestar_c.x"], cwd=ROOT, check=True)
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables", "-ffunction-sections",
             "-fdata-sections", "-Wall", "-Wextra", "-Werror"]
    if args.installed:
        flags.append("-DKERNEL_LINKED_TEST")
    objects = []
    for source, name, defines in (
        ("tests/syscalls_qemu.c", "syscalls_qemu", []),
        ("kernel/syscalls.c", "syscalls_qemu_dispatch", []),
        ("tests/interrupt_entries_qemu_entry.S", "syscalls_qemu_entry",
         ["-Dinterrupt_entries_qemu_main=syscalls_qemu_main"]),
    ):
        obj = BUILD / (name + ".o")
        subprocess.run(["gcc", *flags, *defines, "-c", source, "-o", str(obj)], cwd=ROOT, check=True)
        objects.append(str(obj))
    elf = BUILD / "syscalls_qemu.elf"
    subprocess.run(["ld", "-m", "elf_i386", "-T", "tests/interrupts_qemu.ld", "-o", str(elf),
                    *objects], cwd=ROOT, check=True)
    log = BUILD / ("syscalls_qemu_installed.log" if args.installed else "syscalls_qemu_preview.log")
    result = subprocess.run(["qemu-system-i386", "-machine", "pc", "-cpu", "max", "-m", "64",
                             "-kernel", str(elf), "-display", "none", "-serial", "none", "-monitor", "none",
                             "-icount", "shift=0,align=off,sleep=off", "-no-reboot", "-no-shutdown",
                             "-debugcon", "file:" + str(log),
                             "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"],
                            cwd=ROOT, timeout=45, capture_output=True, text=True)
    output = log.read_text()
    print(output, end="")
    if result.returncode != 33 or "PASS: 3456 complete syscall-chain comparisons" not in output:
        raise RuntimeError(f"Syscall fixture failed (QEMU exit {result.returncode}): {result.stderr}")


if __name__ == "__main__":
    main()
