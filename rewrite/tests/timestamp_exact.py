"""Independently compare the low-TSC primitive with the original VHD."""
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
    obj, binary = build / "timestamp_preview.o", build / "timestamp_preview.bin"
    subprocess.run(["gcc", "-m32", "-march=i386", "-Os", "-ffreestanding", "-fno-pic", "-fno-pie",
                    "-Wall", "-Wextra", "-Werror", "-c", "kernel/timestamp.c", "-o", str(obj)],
                   cwd=ROOT, check=True)
    subprocess.run(["objcopy", "-O", "binary", "--only-section=.text.kernel_timestamp_low", str(obj), str(binary)], check=True)
    recovered = binary.read_bytes()
    assert len(recovered) == 5 and recovered == original[0x3734a:0x3734f]
    if "--installed" in sys.argv:
        assert recovered == (build / "icestar_c.x").read_bytes()[0x3734a:0x3734f]
    print("PASS: low-TSC primitive, 5 exact original bytes" + (", installed image verified" if "--installed" in sys.argv else ""))


if __name__ == "__main__":
    main()
