"""Exercise the recovered type-0x0a Color through the real loader and GUI."""

import json
from pathlib import Path
import struct
import time

from boot_smoke import png_from_ppm
from kernel_image import Fat16, require
from xformat import XImage


def check_color(execute, build, image, cmd_base):
    evidence = {"image": str(image), "cmd_base": hex(cmd_base)}
    scratch = build / "color_state.memory"

    def read_memory(address, size):
        execute("stop")
        try:
            scratch.unlink(missing_ok=True)
            execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
            data = scratch.read_bytes()
            require(len(data) == size, "incomplete Color state read")
            return data
        finally:
            execute("cont")

    def dword(address):
        return struct.unpack("<I", read_memory(address, 4))[0]

    def position():
        return struct.unpack("<ii", read_memory(dword(0x1b048) + 12, 8))

    def move_to(x, y):
        for _ in range(30):
            px, py = position()
            dx, dy = x - px, y - py
            if abs(dx) <= 2 and abs(dy) <= 2:
                return px, py
            execute(f"mouse_move {max(-100, min(100, dx))} {max(-100, min(100, dy))}")
            time.sleep(0.12)
        raise RuntimeError(f"pointer did not reach {(x, y)}; observed {position()}")

    def capture(name):
        ppm = build / f"color_{name}.ppm"
        execute("stop")
        try:
            execute(f'screendump "{ppm}"')
        finally:
            execute("cont")
        png_from_ppm(ppm, build / f"color_{name}.png")
        data = ppm.read_bytes()
        _, dimensions, _, pixels = data.split(b"\n", 3)
        width, height = map(int, dimensions.split())
        return width, height, pixels

    def command(line):
        for character in line:
            key = {" ": "spc", ".": "dot"}.get(character, character)
            execute(f"sendkey {key} 30")
            time.sleep(0.08)
        time.sleep(0.2)
        require(read_memory(cmd_base + 0xb2da, len(line) + 1) == line.encode() + b"\0",
                "CMD input mismatch before executing " + line)
        execute("sendkey ret 30")
        time.sleep(0.8)

    command("cd icestar")
    directory = read_memory(cmd_base + 0xb1da, 255).split(b"\0")[0]
    evidence["working_directory"] = directory.decode("ascii")
    require(directory.upper() == b"C:\\ICESTAR", "CMD did not enter the application directory")
    command("color.x")
    width, height, pixels = capture("launched")
    binary = Fat16(Path(image).read_bytes()).read_file(b"ICESTAR    ", b"COLOR   X  ")
    require(binary == (build / "color.x").read_bytes(), "disk does not contain the compiled C Color")
    parsed = XImage(binary)
    metadata = json.loads((build / "color.symbols.json").read_text())
    ram_start = 0x900000
    ram = read_memory(ram_start, 128 * 1024 * 1024 - ram_start)
    candidates = []
    offset = ram.find(binary[:0x1c])
    while offset >= 0:
        base = ram_start + offset
        if struct.unpack_from("<I", ram, offset + 0x1c)[0] == base:
            expected = parsed.load(base)
            require(ram[offset + 0x40:offset + parsed.bss_start] == expected[0x40:parsed.bss_start],
                    "guest Color instructions/data differ from ELF-derived relocated C code")
            candidates.append(base)
        offset = ram.find(binary[:0x1c], offset + 1)
    del ram
    evidence["relocated_candidates"] = [hex(base) for base in candidates]
    require(len(candidates) == 1, "expected one running relocated C Color")
    base = candidates[0]
    evidence["relocations_verified"] = len(parsed.relocations)
    record, unused0, surface, unused1 = struct.unpack("<4I", read_memory(base + parsed.bss_start, 16))
    callback = dword(record + 0x5b7)
    evidence["state"] = {"window_record": hex(record), "surface": hex(surface),
                         "unused": [unused0, unused1], "close_callback": hex(callback)}
    require(record > 0 and surface > 0, "Color window creation failed")
    require(callback == base + metadata["symbols"]["color_close"]["address"],
            "window close callback is not the compiled Color routine")
    window, task = struct.unpack("<2I", read_memory(record, 8))
    task_address = dword(0x1b0c8) + task * 108
    evidence["window"] = window
    evidence["task"] = task
    evidence["task_active_before_close"] = dword(task_address)
    require(dword(0x6a0000 + window * 4) == record, "Color window pool does not point to its record")
    require(evidence["task_active_before_close"] != 0, "Color task is not active")
    palette_address = dword(0x1b0e0)
    palette = struct.unpack("<600I", read_memory(palette_address, 600 * 4))
    evidence["palette_address"] = hex(palette_address)
    evidence["screen_size"] = [width, height]
    mismatches = []
    for index, value in enumerate(palette):
        row, column = divmod(index, 30)
        x, y = 170 + 20 * column, 140 + 26 * row
        require(x < width and y < height, "Color rectangle lies outside the screenshot")
        start = (y * width + x) * 3
        observed = tuple(pixels[start:start + 3])
        expected = ((value >> 16) & 255, (value >> 8) & 255, value & 255)
        if observed != expected:
            mismatches.append({"index": index, "pixel": [x, y],
                               "expected": expected, "observed": observed})
    evidence["palette_checks"] = 600
    evidence["palette_mismatches"] = mismatches
    (build / "color_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    require(not mismatches, f"Color display mismatch: {mismatches[:3]}")
    # Button center was checked against the actual rendered window. Move only
    # through PS/2 events and confirm the guest cursor reaches the button.
    evidence["pointer_close_button"] = move_to(887, 92)
    execute("mouse_button 1")
    time.sleep(0.1)
    execute("mouse_button 0")
    time.sleep(1)
    capture("closed")
    evidence["window_pool_after_close"] = hex(dword(0x6a0000 + window * 4))
    evidence["task_active_after_close"] = dword(task_address)
    (build / "color_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    require(evidence["window_pool_after_close"] == "0x0", "Color window was not removed")
    require(evidence["task_active_after_close"] == 0, "Color close callback did not terminate its task")
    # Activate the still-running CMD through its taskbar entry, then verify a
    # second real command traverses the input and render path after task exit.
    evidence["pointer_cmd_taskbar"] = move_to(125, 754)
    execute("mouse_button 1")
    time.sleep(0.1)
    execute("mouse_button 0")
    time.sleep(0.5)
    before = list(struct.unpack("<2I", read_memory(cmd_base + 0xf46c, 8)))
    command("cpuid")
    after = list(struct.unpack("<2I", read_memory(cmd_base + 0xf46c, 8)))
    evidence["cmd_cursor_after_close"] = [before, after]
    require(after == [before[0], before[1] + 2], "CMD did not return to its prompt after Color exit")
    require(read_memory(cmd_base + 0xb2da, 6) == bytes(6), "CMD input was not cleared after Color exit")
    capture("cmd_after_close")
    (build / "color_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("C Color loaded; relocated code, 600 palette pixels, PS/2 close, task exit, and CMD reuse verified")
