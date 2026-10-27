"""Observe compiled STARSCR using real PS/2 inputs and read-only guest memory."""
import hashlib
import json
from pathlib import Path
import struct
import time

from boot_smoke import png_from_ppm
from kernel_image import Fat16, require
from xformat import XImage

PHASES = (
    ((0, -100, 1), (-100, 100, 2), (100, 100, 3)),
    ((-100, -100, 3), (100, -100, 1), (0, 100, 2)),
    ((0, -100, 2), (-100, 100, 3), (100, 100, 1)),
    ((-100, -100, 1), (100, -100, 2), (0, 100, 3)),
)


def check_starscr(execute, build, image, cmd_base, reference=False):
    prefix = "starscr_reference" if reference else "starscr"
    evidence = {"image": str(image), "cmd_base": hex(cmd_base), "reference": reference}
    scratch = build / f"{prefix}_state.memory"

    def read_paused(address, size):
        scratch.unlink(missing_ok=True)
        execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
        data = scratch.read_bytes()
        require(len(data) == size, "incomplete STARSCR state read")
        return data

    def read_memory(address, size):
        execute("stop")
        try:
            return read_paused(address, size)
        finally:
            execute("cont")

    def dword(address):
        return struct.unpack("<I", read_memory(address, 4))[0]

    def capture_paused(name):
        ppm = build / f"{prefix}_{name}.ppm"
        execute(f'screendump "{ppm}"')
        _, dimensions, _, pixels = ppm.read_bytes().split(b"\n", 3)
        width, height = map(int, dimensions.split())
        require(len(pixels) == width * height * 3, "incomplete STARSCR screenshot")
        return ppm, width, height, pixels

    def capture(name):
        execute("stop")
        try:
            ppm, width, height, pixels = capture_paused(name)
        finally:
            execute("cont")
        png_from_ppm(ppm, build / f"{prefix}_{name}.png")
        return width, height, pixels

    def command(line):
        for character in line:
            execute(f"sendkey { {' ': 'spc', '.': 'dot'}.get(character, character)} 30")
            time.sleep(0.08)
        time.sleep(0.2)
        require(read_memory(cmd_base + 0xb2da, len(line) + 1) == line.encode() + b"\0",
                "CMD input mismatch before executing " + line)
        execute("sendkey ret 30")
        time.sleep(0.8)

    def write_evidence():
        (build / f"{prefix}_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")

    command("cd icestar")
    directory = read_memory(cmd_base + 0xb1da, 255).split(b"\0")[0]
    evidence["working_directory"] = directory.decode("ascii")
    require(directory.upper() == b"C:\\ICESTAR", "CMD did not enter the STARSCR directory")
    command("starscr.x")
    capture("launched")
    binary = Fat16(Path(image).read_bytes()).read_file(b"ICESTAR    ", b"STARSCR X  ")
    if reference:
        require(hashlib.sha256(binary).hexdigest() ==
                "6d2f3ebfe9a45dcba055ddfe01b7fb8f384e0003a9a9484c9585d912c7f0473c",
                "reference disk does not contain the original STARSCR")
        key_offset, close_offset = 0x160, 0x180
    else:
        require(binary == (build / "starscr.x").read_bytes(), "disk does not contain compiled C STARSCR")
        metadata = json.loads((build / "starscr.symbols.json").read_text())["symbols"]
        key_offset, close_offset = metadata["starscr_key"]["address"], metadata["starscr_close"]["address"]
    parsed = XImage(binary)
    start = 0x900000
    ram = read_memory(start, 128 * 1024 * 1024 - start)
    candidates = []
    offset = ram.find(binary[:0x1c])
    while offset >= 0:
        base = start + offset
        if struct.unpack_from("<I", ram, offset + 0x1c)[0] == base:
            expected = parsed.load(base)
            require(ram[offset + 0x40:offset + parsed.bss_start] == expected[0x40:parsed.bss_start],
                    "guest STARSCR instructions/data differ from relocated disk application")
            candidates.append(base)
        offset = ram.find(binary[:0x1c], offset + 1)
    del ram
    evidence["relocated_candidates"] = [hex(base) for base in candidates]
    require(len(candidates) == 1, "expected one running relocated STARSCR")
    base = candidates[0]
    record, window, surface, unused = struct.unpack("<4I", read_memory(base + parsed.bss_start, 16))
    require(window > 0 and dword(0x6a0000 + window * 4) == record, "STARSCR window pool mismatch")
    require(dword(record + 0xc) == surface and unused == 0, "STARSCR BSS contract mismatch")
    key_callback, close_callback = dword(record + 0x593), dword(record + 0x5b7)
    require(key_callback == base + key_offset, "STARSCR keyboard callback mismatch")
    require(close_callback == base + close_offset, "STARSCR close callback mismatch")
    require(dword(record + 0x11f) == 4, "STARSCR did not set the original window mode")
    task = dword(record + 4)
    task_address = dword(0x1b0c8) + task * 108
    require(dword(task_address) != 0, "STARSCR task is not active")
    wx, wy, ww, wh = struct.unpack("<4I", read_memory(record + 0x12b, 16))
    cw, ch = struct.unpack("<2I", read_memory(record + 0x143, 8))
    display = dword(0x1b034)
    sw, sh = struct.unpack("<2I", read_memory(display + 0xc, 8))
    require((wx, wy, ww, wh) == (0, 0, sw, sh), "STARSCR window is not full screen")
    palette = struct.unpack("<4I", read_memory(dword(0x1b0e0), 16))
    colors = [bytes((c >> 16 & 255, c >> 8 & 255, c & 255)) for c in palette]
    require(len(set(colors)) == 4, "STARSCR phase colors are not distinct")
    evidence.update(window=window, window_record=hex(record), surface=surface, task=task,
                    key_callback=hex(key_callback), close_callback=hex(close_callback),
                    window_geometry=[wx, wy, ww, wh], client_dimensions=[cw, ch],
                    palette=[hex(c) for c in palette], relocations_verified=len(parsed.relocations),
                    task_active_before_close=dword(task_address))
    # Probe the whole cleared square on a four-pixel grid. Avoid a 3-pixel
    # band around all circle boundaries: the original kernel's floating-point
    # rasterizer has its own edge rounding, not specified by this application.
    points = []
    expected_signatures = [bytearray() for _ in PHASES]
    for y in range(-220, 221, 4):
        for x in range(-220, 221, 4):
            if any(117**2 <= (x - dx)**2 + (y - dy)**2 <= 123**2
                   for circles in PHASES for dx, dy, _ in circles):
                continue
            px, py = wx + cw // 2 + x, wy + ch // 2 + y
            require(0 <= px < sw and 0 <= py < sh, "STARSCR probe is outside the screen")
            points.append((px, py))
            for phase, circles in enumerate(PHASES):
                color = 0
                for dx, dy, index in circles:
                    if (x - dx)**2 + (y - dy)**2 <= 120**2:
                        color = index
                expected_signatures[phase] += colors[color]
    expected_signatures = list(map(bytes, expected_signatures))
    require(len(set(expected_signatures)) == 4, "STARSCR phase signatures are not distinct")
    evidence["pixel_probes_per_phase"] = len(points)
    observed, transitions, samples = set(), [], []
    deadline = time.monotonic() + 25
    while len(observed) < 4 and time.monotonic() < deadline:
        execute("stop")
        try:
            tick = struct.unpack("<I", read_paused(0x1b084, 4))[0]
            ppm, width, height, pixels = capture_paused("sample")
        finally:
            execute("cont")
        require((width, height) == (sw, sh), "STARSCR display mode changed")
        actual = b"".join(pixels[(y * sw + x) * 3:(y * sw + x + 1) * 3] for x, y in points)
        match = next((i for i, signature in enumerate(expected_signatures) if signature == actual), None)
        samples.append({"tick": tick, "phase": match, "pixel_signature_sha256": hashlib.sha256(actual).hexdigest()})
        if match is not None:
            if not transitions or transitions[-1] != match:
                transitions.append(match)
            if match not in observed:
                target = build / f"{prefix}_phase{match}.ppm"
                target.write_bytes(ppm.read_bytes())
                png_from_ppm(target, build / f"{prefix}_phase{match}.png")
                observed.add(match)
        time.sleep(0.04)
    evidence.update(phase_samples=samples, phase_transitions=transitions, phases_verified=sorted(observed))
    write_evidence()
    require(observed == {0, 1, 2, 3}, f"STARSCR did not render all four expected phases: {transitions}")
    execute("sendkey esc 50")
    time.sleep(1)
    capture("closed")
    evidence.update(window_pool_after_close=hex(dword(0x6a0000 + window * 4)),
                    task_active_after_close=dword(task_address))
    write_evidence()
    require(evidence["window_pool_after_close"] == "0x0", "Esc did not remove the STARSCR window")
    require(evidence["task_active_after_close"] == 0, "Esc did not terminate the STARSCR task")
    # Test, but do not silently claim, desktop usability after close. The
    # untouched original VHD also leaves pointer input queued after Esc.
    def pointer():
        return list(struct.unpack("<ii", read_memory(dword(0x1b048) + 12, 8)))

    evidence["tick_after_close"] = dword(0x1b084)
    evidence["pointer_before_post_exit_probe"] = pointer()
    for _ in range(3):
        execute("mouse_move 20 10")
        time.sleep(0.15)
    evidence["pointer_after_post_exit_probe"] = pointer()
    evidence["post_exit_mouse_responsive"] = (evidence["pointer_before_post_exit_probe"] !=
                                             evidence["pointer_after_post_exit_probe"])
    evidence["mouse_queue_after_close"] = list(struct.unpack("<4I", read_memory(dword(0x1b044), 16)))
    evidence["tick_after_post_exit_probe"] = dword(0x1b084)
    (build / f"{prefix}_post_exit_registers.txt").write_bytes(execute("info registers"))
    capture("post_exit")
    write_evidence()
    kind = "Original" if reference else "C"
    print(f"{kind} STARSCR loaded; {len(parsed.relocations)} relocations, four phases with {len(points)} pixel probes each, Esc exit and task cleanup verified")
    print("Post-exit mouse input: " + ("responsive" if evidence["post_exit_mouse_responsive"] else
          "STALLED; desktop reuse is not verified (compare smoke-starscr-reference)"))
