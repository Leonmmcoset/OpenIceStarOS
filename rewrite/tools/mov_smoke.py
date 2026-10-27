"""Run recovered MOV with real PS/2 input and read-only QEMU observations."""

import hashlib
import json
from pathlib import Path
import struct
import time

from boot_smoke import png_from_ppm
from kernel_image import Fat16, require
from xformat import XImage


def check_mov(execute, build, image, cmd_base):
    evidence = {"image": str(image), "cmd_base": hex(cmd_base)}
    scratch = build / "mov_state.memory"

    def read_paused(address, size):
        scratch.unlink(missing_ok=True)
        execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
        data = scratch.read_bytes()
        require(len(data) == size, "incomplete MOV state read")
        return data

    def read_memory(address, size):
        execute("stop")
        try:
            return read_paused(address, size)
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

    def capture_paused(name):
        ppm = build / f"mov_{name}.ppm"
        execute(f'screendump "{ppm}"')
        data = ppm.read_bytes()
        _, dimensions, _, pixels = data.split(b"\n", 3)
        width, height = map(int, dimensions.split())
        return ppm, width, height, pixels

    def capture(name):
        execute("stop")
        try:
            ppm, width, height, pixels = capture_paused(name)
        finally:
            execute("cont")
        png_from_ppm(ppm, build / f"mov_{name}.png")
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

    for line in ("cd icestar", "cd games", "cd mov"):
        command(line)
    directory = read_memory(cmd_base + 0xb1da, 255).split(b"\0")[0]
    evidence["working_directory"] = directory.decode("ascii")
    require(directory.upper() == b"C:\\ICESTAR\\GAMES\\MOV", "CMD did not enter the MOV directory")
    command("mov.x")
    capture("launched")
    disk = Fat16(Path(image).read_bytes())
    binary = disk.read_file(b"ICESTAR    ", b"GAMES      ", b"MOV        ", b"MOV     X  ")
    require(binary == (build / "mov.x").read_bytes(), "disk does not contain the compiled C MOV")
    parsed = XImage(binary)
    metadata = json.loads((build / "mov.symbols.json").read_text())
    ram_start = 0x900000
    ram = read_memory(ram_start, 128 * 1024 * 1024 - ram_start)
    candidates = []
    offset = ram.find(binary[:0x1c])
    while offset >= 0:
        base = ram_start + offset
        if struct.unpack_from("<I", ram, offset + 0x1c)[0] == base:
            expected = parsed.load(base)
            require(ram[offset + 0x40:offset + parsed.bss_start] == expected[0x40:parsed.bss_start],
                    "guest MOV instructions/data differ from relocated C code")
            candidates.append(base)
        offset = ram.find(binary[:0x1c], offset + 1)
    del ram
    evidence["relocated_candidates"] = [hex(base) for base in candidates]
    require(len(candidates) == 1, "expected one running relocated C MOV")
    base = candidates[0]
    evidence["relocations_verified"] = len(parsed.relocations)
    handles = struct.unpack("<4I", read_memory(base + parsed.bss_start, 16))
    # The kernel accepts nonnegative image slots, including slot zero.
    require(len(set(handles)) == 4 and all(handle < 65536 for handle in handles),
            "MOV did not create four distinct image controls")
    image_pool = dword(0x1b124)
    images = []
    expected_frames = []
    window = record = 0
    for index, handle in enumerate(handles):
        image_record = dword(image_pool + handle * 4)
        require(image_record != 0, "MOV image is absent from its pool")
        data = read_memory(image_record, 0x15f)
        owner = struct.unpack_from("<I", data, 4)[0]
        if not index:
            window = owner
            record = dword(0x6a0000 + window * 4)
        require(owner == window, "MOV images belong to different windows")
        path = data[0x54:0x153].split(b"\0")[0]
        require(path == f"C:\\ICESTAR\\GAMES\\MOV\\MOV{index}.JPG".encode(), "MOV resource path mismatch")
        x, y, w, h, source_w, source_h = struct.unpack_from("<6I", data, 0x20)
        pixels_address = struct.unpack_from("<I", data, 0x1c)[0]
        depth = struct.unpack_from("<I", data, 0x40)[0]
        require((x, y, w, h, source_w, source_h, depth) == (10, 30, 380, 240, 380, 240, 32),
                "MOV image geometry/depth mismatch")
        raw = read_memory(pixels_address, w * h * 4)
        rgb = bytearray(w * h * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = raw[2::4], raw[1::4], raw[0::4]
        expected_frames.append(bytes(rgb))
        images.append({"handle": handle, "record": hex(image_record), "path": path.decode(),
                       "pixels": hex(pixels_address), "geometry": [x, y, w, h],
                       "decoded_rgb_sha256": hashlib.sha256(rgb).hexdigest()})
    require(len(set(expected_frames)) == 4, "MOV decoded frames are not distinct")
    evidence["images"] = images
    require(record > 0, "MOV window creation failed")
    task = dword(record + 4)
    task_address = dword(0x1b0c8) + task * 108
    callback = dword(record + 0x5b7)
    require(callback == base + metadata["symbols"]["mov_close"]["address"],
            "MOV close callback is not the compiled routine")
    evidence.update(window=window, window_record=hex(record), task=task,
                    close_callback=hex(callback), task_active_before_close=dword(task_address))
    require(evidence["task_active_before_close"] != 0, "MOV task is not active")
    wx, wy, ww, wh = struct.unpack("<4I", read_memory(record + 0x12b, 16))
    evidence["window_geometry"] = [wx, wy, ww, wh]
    require((wx, wy, ww, wh) == (330, 184, 400, 280), "unexpected MOV window geometry")
    observed, transitions = set(), []
    samples = []
    deadline = time.monotonic() + 20
    while len(observed) != 4 and time.monotonic() < deadline:
        execute("stop")
        try:
            tick = struct.unpack("<I", read_paused(0x1b084, 4))[0]
            ppm, sw, sh, pixels = capture_paused("sample")
        finally:
            execute("cont")
        x, y, w, h = wx + 10, wy + 30, 380, 240
        require(x + w <= sw and y + h <= sh, "MOV frame is outside the screen")
        crop = b"".join(pixels[((y + row) * sw + x) * 3:((y + row) * sw + x + w) * 3]
                        for row in range(h))
        match = next((i for i, frame in enumerate(expected_frames) if crop == frame), None)
        samples.append({"tick": tick, "frame": match, "crop_sha256": hashlib.sha256(crop).hexdigest()})
        if match is not None:
            if not transitions or transitions[-1] != match:
                transitions.append(match)
            if match not in observed:
                (build / f"mov_frame{match}.ppm").write_bytes(ppm.read_bytes())
                png_from_ppm(build / f"mov_frame{match}.ppm", build / f"mov_frame{match}.png")
                observed.add(match)
        time.sleep(0.04)
    evidence["frame_samples"] = samples
    evidence["frame_transitions"] = transitions
    evidence["frames_verified"] = sorted(observed)
    (build / "mov_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    require(observed == {0, 1, 2, 3}, f"MOV did not display all four exact decoded frames: {transitions}")
    # Normal window geometry puts the close button thirteen pixels from the
    # right border and twelve from the top, as for the tested Color window.
    evidence["pointer_close_button"] = move_to(wx + ww - 13, wy + 12)
    execute("mouse_button 1")
    time.sleep(0.1)
    execute("mouse_button 0")
    time.sleep(1)
    capture("closed")
    evidence["window_pool_after_close"] = hex(dword(0x6a0000 + window * 4))
    evidence["task_active_after_close"] = dword(task_address)
    evidence["image_pool_after_close"] = [hex(dword(image_pool + handle * 4)) for handle in handles]
    (build / "mov_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    require(evidence["window_pool_after_close"] == "0x0", "MOV window was not removed")
    require(evidence["task_active_after_close"] == 0, "MOV task was not terminated")
    require(evidence["image_pool_after_close"] == ["0x0"] * 4, "MOV images were not released")
    evidence["pointer_cmd_taskbar"] = move_to(125, 754)
    execute("mouse_button 1")
    time.sleep(0.1)
    execute("mouse_button 0")
    time.sleep(0.5)
    before = list(struct.unpack("<2I", read_memory(cmd_base + 0xf46c, 8)))
    command("cpuid")
    after = list(struct.unpack("<2I", read_memory(cmd_base + 0xf46c, 8)))
    evidence["cmd_cursor_after_close"] = [before, after]
    require(after == [before[0], before[1] + 2], "CMD did not return to its prompt after MOV exit")
    require(read_memory(cmd_base + 0xb2da, 6) == bytes(6), "CMD input was not cleared after MOV exit")
    capture("cmd_after_close")
    (build / "mov_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("C MOV loaded; 16 relocations, all four exact animation frames, PS/2 close, image/task cleanup, and CMD reuse verified")
