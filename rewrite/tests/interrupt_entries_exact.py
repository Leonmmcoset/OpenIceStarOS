"""Compile all interrupt-frame entry points and require original byte identity."""
from pathlib import Path
import hashlib
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from kernel_image import Fat16, ORIGINAL_SHA256


def main():
    build = ROOT / "build"
    build.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError("Unexpected original kernel")
    entries = re.findall(r"ENTRY\((\w+),\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+)\)",
                         (ROOT / "kernel/interrupt_entries.def").read_text())
    assert len(entries) == 14
    obj = build / "interrupt_entries_preview.o"
    subprocess.run(["gcc", "-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic",
                    "-fno-pie", "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
                    "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror",
                    "-c", "kernel/interrupt_entries.c", "-o", str(obj)], cwd=ROOT, check=True)
    installed = (build / "openicestaros_kernel.x").read_bytes() if "--installed" in sys.argv else None
    previous = 0x8371a0
    for name, start, end in entries:
        start, end = int(start, 16), int(end, 16)
        assert start == previous and end > start
        previous = end
        binary = build / ("interrupt_preview_" + name + ".bin")
        subprocess.run(["objcopy", "-O", "binary", "--only-section=.text.kernel_" + name,
                        str(obj), str(binary)], check=True)
        recovered = binary.read_bytes()
        assert recovered == original[start-0x800000:end-0x800000], name
        if installed is not None:
            assert recovered == installed[start-0x800000:end-0x800000], name + " installation"
        print(f"{name}: {len(recovered)} bytes, exact original match")
    assert previous == 0x8372e8
    print(f"PASS: {len(entries)} byte-exact interrupt entries ({previous-0x8371a0} bytes)"
          + (", installed image verified" if installed is not None else ""))


if __name__ == "__main__":
    main()
