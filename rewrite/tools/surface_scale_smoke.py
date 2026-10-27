"""Check the test-only scaler application loaded by CMD through the real ABI."""

import hashlib
import json
from pathlib import Path
import struct
import time

from kernel_image import Fat16, require
from xformat import XImage


def expected_destination(source, depth, sw, sh, tw, th):
    """Bounded positive-dimension model, independent of compiled kernel code."""
    result = bytearray(b"\xa5" * 384)
    unit = depth // 8
    rx = (sw - tw) * 100 // tw if sw > tw else sw * 100 // (tw - sw) if sw < tw else 0
    ry = (sh - th) * 100 // th if sh > th else sh * 100 // (th - sh) if sh < th else 0
    sy, vc = 0, 1
    for row in range(th):
        sy = min(sy, sh - 1)
        col, sx, hc = 0, 0, 1
        while col < tw:
            sx = min(sx, sw - 1)

            def put(column):
                offset = ((row + 1) * 12 + column + 2) * unit
                pixel = source[(sy * sw + sx) * unit:(sy * sw + sx + 1) * unit]
                require(len(pixel) == unit, "oracle source access out of range")
                if depth == 32:
                    pixel = pixel[:3] + b"\0"
                require(0 <= offset <= len(result) - unit, "oracle destination access out of range")
                result[offset:offset + unit] = pixel

            put(col)
            if sw > tw:
                while hc * 100 < col * rx and sx < sw - 1:
                    sx += 1
                    hc += 1
            elif sw < tw:
                while (col + 1) * 100 >= hc * rx and col + 1 < tw:
                    put(col + 1)
                    col += 1
                    hc += 1
            col += 1
            sx += 1
        if sh > th:
            while vc * 100 < row * ry and sy < sh - 1:
                sy += 1
                vc += 1
        elif sh < th and (row + 1) * 100 >= vc * ry and row + 1 < th:
            sy -= 1
            vc += 1
        sy += 1
    return result


def check_surface_scale(execute, build, image, cmd_base):
    scratch = build / "surface_scale_state.memory"

    def read_memory(address, size):
        execute("stop")
        try:
            scratch.unlink(missing_ok=True)
            execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
            data = scratch.read_bytes()
            require(len(data) == size, "incomplete scaler diagnostic memory read")
            return data
        finally:
            execute("cont")

    def command(line):
        for char in line:
            key = {" ": "spc", ".": "dot"}.get(char, char)
            execute(f"sendkey {key} 30")
            time.sleep(0.08)
        time.sleep(0.2)
        require(read_memory(cmd_base + 0xb2da, len(line) + 1) == line.encode() + b"\0",
                "CMD diagnostic input mismatch")
        execute("sendkey ret 30")
        time.sleep(0.8)

    command("cd icestar")
    command("color.x")
    binary = Fat16(Path(image).read_bytes()).read_file(b"ICESTAR    ", b"COLOR   X  ")
    require(binary == (build / "surface_scale_guest.x").read_bytes(), "missing diagnostic X on disk")
    parsed = XImage(binary)
    metadata = json.loads((build / "surface_scale_guest.symbols.json").read_text())
    ram_start = 0x900000
    ram = read_memory(ram_start, 128 * 1024 * 1024 - ram_start)
    bases = []
    offset = ram.find(binary[:0x1c])
    while offset >= 0:
        base = ram_start + offset
        if struct.unpack_from("<I", ram, offset + 0x1c)[0] == base:
            require(ram[offset + 0x40:offset + parsed.bss_start] ==
                    parsed.load(base)[0x40:parsed.bss_start], "guest diagnostic relocation mismatch")
            bases.append(base)
        offset = ram.find(binary[:0x1c], offset + 1)
    del ram
    require(len(bases) == 1, f"expected one relocated scaler diagnostic, found {bases}")
    symbol = metadata["symbols"]["scale_report"]
    report_address = bases[0] + symbol["address"]
    report = read_memory(report_address, symbol["size"])
    require(len(report) == 4148, "unexpected scaler report size")
    words = struct.unpack_from("<13I", report)
    magic, status, entry, first, second, old0, old1, high0, high1, capacity, flags0, flags1, count = words
    require((magic, status, entry, count) == (0x53434c32, 1, 0x8194e4, 6),
            f"scaler diagnostic did not complete: {words}")
    require(high0 == high1 and first == high0 and second == first + 1 and second < capacity,
            "diagnostic changed the layer high-water count or borrowed invalid slots")
    require((old0, old1) == (0, 0) and read_memory(0x500000 + first * 4, 8) == bytes(8),
            "diagnostic did not restore borrowed zero slots")
    require(flags0 & 0x200 and flags0 & 0x200 == flags1 & 0x200,
            "diagnostic did not restore enabled interrupts")
    evidence = {"image": str(image), "image_sha256": hashlib.sha256(Path(image).read_bytes()).hexdigest(),
                "diagnostic_only": True, "relocated_base": hex(bases[0]),
                "relocations_verified": len(parsed.relocations), "report_address": hex(report_address),
                "entry": hex(entry), "borrowed_slots": [first, second], "slots_restored": True,
                "high_water_before_after": [high0, high1], "flags_before_after": [hex(flags0), hex(flags1)],
                "cases": []}
    for index in range(6):
        offset = 164 + index * 664
        depth, sw, sh, tw, th, returned = struct.unpack_from("<6I", report, offset)
        expected_geometry = [(8, 6, 4, 6, 4), (8, 7, 5, 4, 3), (8, 3, 2, 6, 5),
                             (32, 6, 4, 6, 4), (32, 7, 5, 4, 3), (32, 3, 2, 6, 5)][index]
        require((depth, sw, sh, tw, th) == expected_geometry, "guest case geometry mismatch")
        source = report[offset + 24:offset + 280]
        require(source == struct.pack("<64I", *(0x9b000000 + index * 0x01010101 + j * 0x00030507
                                              for j in range(64))), "guest source was modified")
        observed = report[offset + 280:offset + 664]
        expected = expected_destination(source, depth, sw, sh, tw, th)
        require(returned == th, f"guest scaler return mismatch for case {index}")
        require(observed == expected, f"guest scaler output/guard mismatch for case {index}")
        evidence["cases"].append({"depth": depth, "source_size": [sw, sh], "target_size": [tw, th],
                                  "returned": returned, "destination_bytes_checked": len(observed),
                                  "destination_sha256": hashlib.sha256(observed).hexdigest()})
    (build / "surface_scale_guest_report.memory").write_bytes(report)
    (build / "surface_scale_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("Test-only C X application: 6 real service-slot-163 calls, 2304 output/guard bytes, "
          "relocations, source buffers, layer restoration and interrupt restoration verified")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Build a diagnostic-only VHD with COLOR.X replaced")
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
        require(updated[cursor:start] == original[cursor:start], "unexpected non-COLOR image change")
        cursor = start + length
    require(updated[cursor:] == original[cursor:], "unexpected image tail change")
    require(Fat16(updated).read_kernel() == fs.read_kernel(), "diagnostic changed kernel")
    args.output.write_bytes(updated)
    print(f"{args.output}: only COLOR.X file extents replaced with the test-only scaler diagnostic")
