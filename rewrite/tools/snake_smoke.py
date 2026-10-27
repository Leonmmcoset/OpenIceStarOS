"""Integration assertions for the real loader, interrupts, display, and PS/2 path."""

import json
from pathlib import Path
import struct
import time

from boot_smoke import png_from_ppm
from kernel_image import Fat16, require
from xformat import XImage


def check_snake(execute, build, image, cmd_base):
    evidence = {"image": str(image), "cmd_base": hex(cmd_base)}
    scratch = build / "snake_state.memory"

    def read_memory(address, size):
        execute("stop")
        try:
            scratch.unlink(missing_ok=True)
            execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
            data = scratch.read_bytes()
            require(len(data) == size, "incomplete Snake state read")
            return data
        finally:
            execute("cont")

    def capture(name):
        ppm = build / f"snake_{name}.ppm"
        execute("stop")
        try:
            execute(f'screendump "{ppm}"')
        finally:
            execute("cont")
        png_from_ppm(ppm, build / f"snake_{name}.png")

    def command(line):
        for character in line:
            key = {" ": "spc", ".": "dot"}.get(character, character)
            execute(f"sendkey {key} 30")
            time.sleep(0.08)
        time.sleep(0.2)
        require(read_memory(cmd_base + 0xb2da, len(line) + 1) == line.encode() + b"\0",
                "CMD input mismatch before executing " + line)
        execute("sendkey ret 30")
        time.sleep(0.7)

    command("cd icestar")
    command("cd games")
    directory = read_memory(cmd_base + 0xb1da, 255).split(b"\0")[0]
    evidence["working_directory"] = directory.decode("ascii")
    require(directory.upper() == b"C:\\ICESTAR\\GAMES", "CMD did not enter the games directory")
    command("snake.x")
    capture("launched")
    binary = Fat16(Path(image).read_bytes()).read_file(b"ICESTAR    ", b"GAMES      ", b"SNAKE   X  ")
    require(binary == (build / "snake.x").read_bytes(), "disk does not contain the compiled C Snake")
    parsed = XImage(binary)
    ram_start = 0x900000
    ram = read_memory(ram_start, 128 * 1024 * 1024 - ram_start)
    candidates = []
    offset = ram.find(binary[:0x1c])
    while offset >= 0:
        base = ram_start + offset
        if struct.unpack_from("<I", ram, offset + 0x1c)[0] == base:
            expected = parsed.load(base)
            require(ram[offset + 0x40:offset + 0x92c] == expected[0x40:0x92c],
                    "guest Snake code differs from ELF-derived relocated C code")
            require(ram[offset + 0x93c:offset + 0x97f] == expected[0x93c:0x97f],
                    "guest Snake strings differ")
            candidates.append(base)
        offset = ram.find(binary[:0x1c], offset + 1)
    del ram
    evidence["relocated_candidates"] = [hex(base) for base in candidates]
    require(len(candidates) == 1, "expected one running relocated C Snake")
    base = candidates[0]
    evidence["relocations_verified"] = len(parsed.relocations)

    def state():
        memory = read_memory(base + 0x92c, 0x3d4)

        def word(offset):
            return struct.unpack_from("<I", memory, offset - 0x92c)[0]

        return {name: word(offset) for name, offset in (
            ("length", 0x92c), ("surface", 0x980), ("window", 0x984),
            ("x", 0x9a0), ("y", 0xb40), ("food_x", 0xcd0), ("food_y", 0xcd4),
            ("direction", 0xcd8), ("game_over", 0xcdc), ("score", 0xce0),
            ("last_tick", 0xce4), ("current_tick", 0xce8))}

    def wait_for(predicate, timeout, description):
        deadline = time.monotonic() + timeout
        while True:
            observed = state()
            if predicate(observed):
                return observed
            if time.monotonic() > deadline:
                capture("failure")
                raise RuntimeError(f"{description}: {observed}")
            time.sleep(0.04)

    initial = state()
    evidence["initial"] = initial
    require(initial["surface"] > 0 and initial["window"] > 0, "Snake window creation failed")
    evidence["game_over"] = wait_for(lambda s: s["game_over"] == 1, 30, "Snake did not reach a wall")
    capture("game_over")
    execute("sendkey r 100")
    restarted = wait_for(lambda s: s["game_over"] == 0, 3, "R did not restart Snake")
    evidence["restarted"] = restarted
    require(restarted["length"] == 1 and restarted["score"] == 0 and
            restarted["direction"] == 0 and restarted["y"] == 10 and 15 <= restarted["x"] <= 17,
            "restart did not restore the recovered initial state")
    # The original translator selects keypad digits using the global Num Lock
    # flag, even for E0-prefixed arrows. Exercise Snake's scan-only path with
    # that original mode disabled, using a real PS/2 lock-key event.
    evidence["num_lock_initial"] = struct.unpack("<I", read_memory(0x1b018, 4))[0]
    if evidence["num_lock_initial"]:
        execute("sendkey num_lock 100")
        time.sleep(0.2)
    evidence["num_lock_for_arrow"] = struct.unpack("<I", read_memory(0x1b018, 4))[0]
    require(evidence["num_lock_for_arrow"] == 0, "Num Lock key did not select the scan-only path")
    time.sleep(0.15)
    execute("sendkey down 200")
    down = wait_for(lambda s: s["direction"] == 1, 3, "Down arrow was not handled")
    moved = wait_for(lambda s: s["y"] > down["y"] and s["game_over"] == 0,
                     3, "Snake timer did not move downward")
    evidence["down"] = down
    evidence["moved"] = moved
    require(moved["x"] == down["x"] and moved["last_tick"] != down["last_tick"],
            "downward movement or timer state mismatch")
    capture("moving")
    time.sleep(0.25)
    execute("sendkey a 200")
    left = wait_for(lambda s: s["direction"] == 2, 3, "A key was not handled")
    evidence["left"] = left
    evidence["moved_left"] = wait_for(lambda s: s["x"] < left["x"] and s["game_over"] == 0,
                                    3, "Snake did not move left")
    capture("turned")
    (build / "snake_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("C Snake loaded, relocated code verified, wall collision, restart, arrow and letter turns passed")
