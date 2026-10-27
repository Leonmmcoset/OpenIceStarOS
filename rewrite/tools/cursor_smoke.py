"""Exercise cursor shape/position changes through real window-border input."""

import json
import struct
import time

from boot_smoke import png_from_ppm
from kernel_image import require


def check_cursor(execute, build, cmd_base):
    scratch = build / "cursor_state.memory"
    evidence = {"cmd_base": hex(cmd_base), "input": "emulated PS/2 only",
                "window_application": "original DEVMGR.X", "completed": False}

    def read_memory(address, size):
        execute("stop")
        try:
            scratch.unlink(missing_ok=True)
            execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
            data = scratch.read_bytes()
            require(len(data) == size, "incomplete cursor state read")
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
            if abs(dx) <= 1 and abs(dy) <= 1:
                time.sleep(0.2)
                return position()
            execute(f"mouse_move {max(-100, min(100, dx))} {max(-100, min(100, dy))}")
            time.sleep(0.12)
        raise RuntimeError(f"cursor did not reach {(x, y)}; observed {position()}")

    def capture(name):
        ppm = build / f"cursor_{name}.ppm"
        execute("stop")
        try:
            execute(f'screendump "{ppm}"')
        finally:
            execute("cont")
        png_from_ppm(ppm, build / f"cursor_{name}.png")

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

    def windows():
        count = dword(0x1b098)
        require(0 < count <= 4096, f"unexpected window high-water count: {count}")
        return struct.unpack(f"<{count}I", read_memory(0x6a0000, count * 4))

    # CMD uses style 2. Original DEVMGR.X passes style 3 at file offset 0x6c;
    # 0x807bca..0x807bd0 stores the seventh creation argument at record+0x11b.
    # Launch that existing application, leaving production binaries untouched.
    before = set(windows())
    command("cd icestar")
    command("devmgr.x")
    candidates = [(handle, record) for handle, record in enumerate(windows())
                  if record and record not in before and dword(record + 0x11b) == 3]
    require(len(candidates) == 1, f"expected one new resizable DEVMGR window: {candidates}")
    window, record = candidates[0]
    surface = dword(record + 12)
    descriptor = dword(0x500000 + 4 * surface)
    x, y, width, height = struct.unpack("<4i", read_memory(descriptor + 4, 16))
    evidence.update(window=window, record=hex(record), surface=surface,
                    geometry=[x, y, width, height], resize_mode=dword(record + 0x11b))
    (build / "cursor_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    require(evidence["resize_mode"] == 3, "DEVMGR window is not in the original resizable mode")
    require(width > 100 and height > 100, "DEVMGR geometry is too small for cursor probes")
    # Hovering the top-left five-pixel border calls 0x81325c with shape 2
    # and an eight-pixel hotspot offset. No mouse button or guest write is used.
    px, py = move_to(x + 2, y + 2)
    cursor = dword(0x1b04c)
    state = list(struct.unpack("<3I", read_memory(cursor, 12)))
    shape = dword(0x1b120)
    evidence["border"] = {"pointer": [px, py], "shape": shape, "descriptor": state}
    (build / "cursor_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    require(shape == 2, "top-left window border did not select cursor shape 2")
    require(state == [dword(0x1b140), px - 8, py - 8],
            "positioned cursor did not retain the original resize hotspot/pixels")
    require(dword(cursor + 44) != 0, "resize cursor is hidden")
    capture("border")
    px, py = move_to(x + 80, y + 80)
    shape = dword(0x1b120)
    state = list(struct.unpack("<3I", read_memory(cursor, 12)))
    evidence["interior"] = {"pointer": [px, py], "shape": shape, "descriptor": state}
    (build / "cursor_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    require(shape == 0 and state == [dword(0x1b03c), px, py],
            "cursor did not restore its original shape and hotspot inside DEVMGR")
    require(dword(cursor + 44) != 0, "restored cursor is hidden")
    capture("interior")
    move_to(x + width - 13, y + 12)
    execute("mouse_button 1")
    time.sleep(0.1)
    execute("mouse_button 0")
    time.sleep(0.8)
    require(dword(0x6a0000 + 4 * window) == 0, "DEVMGR window was not deleted after close")
    evidence["window_deleted"] = True
    move_to(125, 754)
    execute("mouse_button 1")
    time.sleep(0.1)
    execute("mouse_button 0")
    time.sleep(0.5)
    command("cd ..")
    require(read_memory(cmd_base + 0xb1da, 255).split(b"\0")[0].upper() == b"C:\\",
            "CMD did not return to the root directory")
    evidence["completed"] = True
    (build / "cursor_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("PS/2 border hover verified positioned resize cursor and normal-shape restoration")
