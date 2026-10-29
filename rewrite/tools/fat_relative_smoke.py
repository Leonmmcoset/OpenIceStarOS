"""Check real FAT service calls from a test-only X application."""
import hashlib
import json
from pathlib import Path
import struct
import time

from kernel_image import Fat16, require, word
from xformat import XImage


def check_fat_relative(execute, build, image, cmd_base):
    scratch = build / "fat_relative_state.memory"

    def read_memory(address, size):
        execute("stop")
        try:
            scratch.unlink(missing_ok=True)
            execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
            data = scratch.read_bytes()
            require(len(data) == size, "incomplete FAT diagnostic memory read")
            return data
        finally:
            execute("cont")

    for line in ("cd icestar", "color.x"):
        for char in line:
            execute(f"sendkey { {' ': 'spc', '.': 'dot'}.get(char, char)} 30")
            time.sleep(0.08)
        time.sleep(0.2)
        require(read_memory(cmd_base + 0xb2da, len(line) + 1) == line.encode() + b"\0",
                "CMD FAT diagnostic input mismatch")
        execute("sendkey ret 30")
        time.sleep(0.8)
    disk = Fat16(Path(image).read_bytes())
    binary = disk.read_file(b"ICESTAR    ", b"COLOR   X  ")
    require(binary == (build / "fat_relative_guest.x").read_bytes(), "missing FAT diagnostic on disk")
    parsed = XImage(binary)
    metadata = json.loads((build / "fat_relative_guest.symbols.json").read_text())
    ram_start = 0x900000
    ram = read_memory(ram_start, 128 * 1024 * 1024 - ram_start)
    bases = []
    offset = ram.find(binary[:0x1c])
    while offset >= 0:
        base = ram_start + offset
        if struct.unpack_from("<I", ram, offset + 0x1c)[0] == base:
            require(ram[offset + 0x40:offset + parsed.bss_start] ==
                    parsed.load(base)[0x40:parsed.bss_start], "FAT guest relocation mismatch")
            bases.append(base)
        offset = ram.find(binary[:0x1c], offset + 1)
    del ram
    require(len(bases) == 1, f"expected one relocated FAT diagnostic: {bases}")
    symbol = metadata["symbols"]["fat_report"]
    address = bases[0] + symbol["address"]
    report = read_memory(address, symbol["size"])
    require(len(report) == 525040, "unexpected FAT report size")
    header = struct.unpack_from("<12I", report)
    entries = [0x826ade, 0x826c3a, 0x826c6e, 0x826dd0, 0x826f38, 0x827032, 0x82711e, 0x8272f4]
    require(header[:4] == (0x46415432, 1, 1, 16) and list(header[4:]) == entries,
            f"FAT guest did not complete: {header}")
    init = disk.find(disk.root, b"INIT    X  ", False)
    offsets = list(disk.chain(word(init, 26)))
    require(disk.cluster_bytes == 8192 and len(offsets) == 4, "INIT fixture geometry changed")
    raw = b"".join(disk.data[o:o+8192] for o in offsets)
    evidence = dict(diagnostic_only=True, image_sha256=hashlib.sha256(Path(image).read_bytes()).hexdigest(),
                    relocated_base=hex(bases[0]), relocations_verified=len(parsed.relocations),
                    report_address=hex(address), input="emulated PS/2 only",
                    file="/INIT.X", cluster_bytes=8192, chain_clusters=4, cases=[])
    for index in range(16):
        start = 48 + index * 32812
        entry, result, freed = struct.unpack_from("<3I", report, start)
        expected_entry = entries[index if index < 6 else index % 2 + 2 if index < 10 else 6 if index < 13 else 7]
        require(entry == expected_entry, "unexpected FAT service entry")
        expected = bytearray(b"\xa5" * 32800)
        if index in (0, 1, 4, 5, 10, 11, 15):
            expected[16:32784] = raw
            require(result != 0 if index in (4, 5, 10, 11, 15) else result == 4, "whole-file return mismatch")
        elif index in (2, 3, 7, 9):
            cluster = 2 if index in (2, 3) else 3 if index == 7 else 0
            begin = cluster * 8192
            expected[16+begin:16+begin+8192] = raw[begin:begin+8192]
            require(result == cluster, "single-cluster return mismatch")
        elif index in (6, 8, 12):
            require(result == (3 if index == 6 else 0), "no-read return mismatch")
        else:
            length = 8192 if index == 13 else 16384
            expected[16:16+length] = raw[:length]
            require(result != 0, "bounded allocation failed")
        require(freed == int(index in (4, 5, 10, 11, 13, 14, 15)), "diagnostic allocation cleanup mismatch")
        observed = report[start+12:start+32812]
        require(observed == expected, f"FAT case {index} data/guard mismatch")
        evidence["cases"].append(dict(index=index, entry=hex(entry), result=hex(result),
            allocation_freed=bool(freed), bytes_checked=len(observed),
            buffer_sha256=hashlib.sha256(observed).hexdigest()))
    (build / "fat_relative_guest_report.memory").write_bytes(report)
    (build / "fat_relative_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("Test-only X: all eight real FAT service entries, 16 calls, 524800 data/guard bytes, "
          "arbitrary directory base, bounded clusters, past-EOF/negative offsets, allocations/free and relocations verified")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Install a test-only FAT X in a separate diagnostic image")
    parser.add_argument("source", type=Path)
    parser.add_argument("diagnostic", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    require(args.output.resolve() != args.source.resolve(), "diagnostic must use a separate image")
    original = args.source.read_bytes()
    fs = Fat16(original)
    names = (b"ICESTAR    ", b"COLOR   X  ")
    updated = fs.replace_file(args.diagnostic.read_bytes(), *names)
    cursor = 0
    for start, length in sorted(fs.file_extents(*names)):
        require(updated[cursor:start] == original[cursor:start], "unexpected non-COLOR disk change")
        cursor = start + length
    require(updated[cursor:] == original[cursor:], "unexpected image tail change")
    require(Fat16(updated).read_kernel() == fs.read_kernel(), "diagnostic changed kernel")
    args.output.write_bytes(updated)
    print(f"{args.output}: only COLOR.X extents replaced by the test-only FAT diagnostic")
