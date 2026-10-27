"""Boot a snapshot, capture its screen, and check the loaded C kernel slots."""

import json
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time
import zlib


def png_from_ppm(source, destination):
    data = source.read_bytes()
    magic, dimensions, maximum, pixels = data.split(b"\n", 3)
    if magic != b"P6" or maximum != b"255":
        raise ValueError("unexpected QEMU PPM header")
    width, height = map(int, dimensions.split())
    if len(pixels) != width * height * 3:
        raise ValueError("incomplete screenshot")

    def chunk(kind, content):
        return (struct.pack("!I", len(content)) + kind + content +
                struct.pack("!I", zlib.crc32(kind + content)))

    scanlines = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
                         for y in range(height))
    destination.write_bytes(b"\x89PNG\r\n\x1a\n" +
                            chunk(b"IHDR", struct.pack("!IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                            chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))


def main():
    root = Path(__file__).resolve().parents[1]
    build = root / "build"
    image = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else build / "IceStarOS_c_init.vhd"
    screen = build / "boot_c_init.ppm"
    memory = build / "boot_kernel.memory"
    with tempfile.TemporaryDirectory(prefix="icestar-qemu-") as temporary:
        monitor = Path(temporary) / "monitor.sock"
        command = ["qemu-system-i386", "-machine", "pc", "-m", "128", "-drive",
                   f"file={image},format=raw,snapshot=on", "-boot", "c", "-display", "none",
                   "-monitor", f"unix:{monitor},server=on,wait=off"]
        with (build / "boot_qemu.log").open("wb") as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 10
                while not monitor.exists():
                    if process.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("QEMU monitor unavailable; see build/boot_qemu.log")
                    time.sleep(0.05)
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
                    connection.settimeout(10)
                    connection.connect(str(monitor))

                    def read_prompt():
                        output = b""
                        while not output.endswith(b"(qemu) "):
                            block = connection.recv(65536)
                            if not block:
                                raise RuntimeError("QEMU monitor closed")
                            output += block
                        return output

                    def execute(line):
                        connection.sendall(line.encode() + b"\n")
                        return read_prompt()

                    read_prompt()
                    time.sleep(12)
                    execute("stop")
                    execute(f'screendump "{screen}"')
                    execute(f'pmemsave 0x800000 0x40000 "{memory}"')
                    (build / "boot_registers.txt").write_bytes(execute("info registers"))
                    connection.sendall(b"quit\n")
                    while connection.recv(65536):
                        pass
                process.wait(timeout=10)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
    loaded = memory.read_bytes()
    kernel = (build / "icestar_c.x").read_bytes()
    for patch in json.loads((root / "kernel/patches.json").read_text()):
        start, end = int(patch["start"], 0) - 0x800000, int(patch["end"], 0) - 0x800000
        if loaded[start:end] != kernel[start:end]:
            raise RuntimeError("loaded kernel patch differs: " + patch["name"])
    png_from_ppm(screen, build / "boot_c_init.png")
    print("QEMU snapshot boot captured; all fixed-address C patches match guest memory")
    print("Screenshot: " + str(build / "boot_c_init.png"))
    print("Desktop appearance still requires visual inspection; this is not a full OS test")


if __name__ == "__main__":
    main()
