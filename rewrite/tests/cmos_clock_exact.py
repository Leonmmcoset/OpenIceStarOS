"""Compile the CMOS hardware boundary and verify independently extracted bytes."""
from pathlib import Path
import hashlib
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from kernel_image import Fat16, ORIGINAL_SHA256


def main():
    build = ROOT / "build"
    build.mkdir(exist_ok=True)
    original = Fat16((ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()).read_kernel()
    assert hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256
    obj, binary = build / "cmos_clock_preview.o", build / "cmos_clock_preview.bin"
    flags = ["-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
             "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
             "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror"]
    subprocess.run(["gcc", *flags, "-c", "kernel/cmos_clock.c", "-o", str(obj)],
                   cwd=ROOT, check=True)
    subprocess.run(["objcopy", "-O", "binary", "--only-section=.text.kernel_read_cmos_clock",
                    str(obj), str(binary)], check=True)
    recovered = binary.read_bytes()
    assert len(recovered) == 132
    assert recovered == original[0x2ae84:0x2af08]
    if "--installed" in sys.argv:
        assert recovered == (build / "icestar_c.x").read_bytes()[0x2ae84:0x2af08]
    print("PASS: CMOS sampling boundary, 132 exact original bytes"
          + (", installed image verified" if "--installed" in sys.argv else ""))


if __name__ == "__main__":
    main()
