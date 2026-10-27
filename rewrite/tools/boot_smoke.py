"""Boot a snapshot, capture its screen, and check the loaded C kernel slots."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import socket
import struct
import subprocess
import tempfile
import time
import zlib

from kernel_image import Fat16


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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", nargs="?", type=Path)
    parser.add_argument("--input-check", action="store_true",
                        help="exercise the desktop pointer and CMD keyboard input")
    parser.add_argument("--surface-move-check", action="store_true",
                        help="trace recovered moved-layer execution during the PS/2 input check")
    parser.add_argument("--surface-scale-check", action="store_true",
                        help="load the test-only scaler X application and trace six service calls")
    parser.add_argument("--surface-lifecycle-check", action="store_true",
                        help="trace recovered surface initialization/search/create/delete during normal boot and input")
    parser.add_argument("--surface-order-check", action="store_true",
                        help="trace recovered surface remove/append/front/move during normal boot and input")
    parser.add_argument("--task-check", action="store_true",
                        help="observe timer preemption and suspended C scheduler TSS/stack state")
    parser.add_argument("--memory-check", action="store_true",
                        help="verify detected RAM, page metadata, and the live heap chain")
    parser.add_argument("--snake-check", action="store_true",
                        help="load the C Snake through CMD and exercise movement/restart")
    parser.add_argument("--color-check", action="store_true",
                        help="load the C Color, verify its palette, and close its window")
    parser.add_argument("--mov-check", action="store_true",
                        help="load the C MOV, verify its four animation frames, and close it")
    parser.add_argument("--starscr-check", action="store_true",
                        help="load the C screen saver, verify four phases, and exit with Esc")
    parser.add_argument("--starscr-reference", action="store_true",
                        help="run the same screen-saver check on the untouched original VHD")
    arguments = parser.parse_args()
    if arguments.starscr_reference and not arguments.starscr_check:
        parser.error("--starscr-reference requires --starscr-check")
    if arguments.surface_move_check:
        if arguments.starscr_reference:
            parser.error("--surface-move-check requires the compiled kernel image")
        arguments.input_check = True
    if arguments.surface_scale_check and (arguments.surface_move_check or arguments.starscr_reference):
        parser.error("--surface-scale-check uses its own compiled-kernel diagnostic image and trace")
    if arguments.surface_lifecycle_check:
        if (arguments.starscr_reference or arguments.surface_move_check or arguments.surface_scale_check or
                arguments.surface_order_check):
            parser.error("--surface-lifecycle-check requires its own compiled-kernel execution trace")
        arguments.input_check = True
    if arguments.surface_order_check:
        if arguments.starscr_reference or arguments.surface_move_check or arguments.surface_scale_check:
            parser.error("--surface-order-check requires its own compiled-kernel execution trace")
        arguments.input_check = True
    if sum((arguments.snake_check, arguments.color_check, arguments.mov_check, arguments.starscr_check,
            arguments.surface_scale_check)) > 1:
        parser.error("select one application check per boot")
    root = Path(__file__).resolve().parents[1]
    build = root / "build"
    default_image = ("IceStarOS_scale_diagnostic.vhd" if arguments.surface_scale_check else
                     "IceStarOS_c_apps.vhd" if arguments.color_check or arguments.mov_check or arguments.starscr_check else
                     "IceStarOS_c_snake.vhd" if arguments.snake_check else "IceStarOS_c_init.vhd")
    image = (arguments.image.resolve() if arguments.image else
             root.parent / "IceStarOS_x86_32M.vhd" if arguments.starscr_reference else build / default_image)
    capture_prefix = "boot_starscr_reference" if arguments.starscr_reference else "boot_c_init"
    screen = build / f"{capture_prefix}.ppm"
    memory = build / ("boot_starscr_reference.memory" if arguments.starscr_reference else "boot_kernel.memory")
    with tempfile.TemporaryDirectory(prefix="icestar-qemu-") as temporary:
        monitor = Path(temporary) / "monitor.sock"
        command = ["qemu-system-i386", "-machine", "pc", "-m", "128", "-drive",
                   f"file={image},format=raw,snapshot=on", "-boot", "c", "-display", "none",
                   "-monitor", f"unix:{monitor},server=on,wait=off"]
        if arguments.surface_move_check:
            trace = build / "surface_move_guest.trace"
            trace.unlink(missing_ok=True)
            command.extend(["-dfilter", "0x8191d8..0x8194e3", "-D", str(trace)])
        if arguments.surface_scale_check:
            trace = build / "surface_scale_guest.trace"
            trace.unlink(missing_ok=True)
            command.extend(["-dfilter", "0x8194e4..0x819c3f", "-D", str(trace)])
        if arguments.surface_lifecycle_check:
            trace = build / "surface_lifecycle_guest.trace"
            trace.unlink(missing_ok=True)
            # Keep normal TB chaining during RAM probing. Logging starts at
            # boot so the one-time table initializer is included as well.
            command.extend(["-dfilter", "0x8175c0..0x8178ef", "-D", str(trace), "-d", "exec"])
        if arguments.surface_order_check:
            trace = build / "surface_order_guest.trace"
            trace.unlink(missing_ok=True)
            command.extend(["-dfilter", "0x8178f0..0x817ccf", "-D", str(trace), "-d", "exec"])
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
                    if arguments.memory_check:
                        from memory_smoke import check_memory
                        check_memory(execute, build)
                    if arguments.task_check:
                        if arguments.starscr_reference:
                            parser.error("--task-check requires the compiled kernel image")
                        from task_smoke import check_task_switch
                        check_task_switch(execute, build)
                    if (arguments.input_check or arguments.snake_check or arguments.color_check or
                            arguments.mov_check or arguments.starscr_check or arguments.surface_scale_check):
                        if (arguments.surface_move_check or arguments.surface_scale_check or
                                arguments.surface_lifecycle_check or arguments.surface_order_check):
                            # Trace after boot/RAM probing; disabling TB chaining
                            # during early memory initialization distorts timing.
                            execute("log exec,nochain")
                        cmd_base = check_inputs(execute, build, image)
                        if arguments.snake_check:
                            from snake_smoke import check_snake
                            check_snake(execute, build, image, cmd_base)
                        if arguments.color_check:
                            from color_smoke import check_color
                            check_color(execute, build, image, cmd_base)
                        if arguments.mov_check:
                            from mov_smoke import check_mov
                            check_mov(execute, build, image, cmd_base)
                        if arguments.starscr_check:
                            from starscr_smoke import check_starscr
                            check_starscr(execute, build, image, cmd_base, arguments.starscr_reference)
                        if arguments.surface_scale_check:
                            from surface_scale_smoke import check_surface_scale
                            check_surface_scale(execute, build, image, cmd_base)
                    execute("stop")
                    execute(f'screendump "{screen}"')
                    execute(f'pmemsave 0x800000 0x40000 "{memory}"')
                    registers = "boot_starscr_reference_registers.txt" if arguments.starscr_reference else "boot_registers.txt"
                    (build / registers).write_bytes(execute("info registers"))
                    connection.sendall(b"quit\n")
                    while connection.recv(65536):
                        pass
                process.wait(timeout=10)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
    loaded = memory.read_bytes()
    kernel = (Fat16(image.read_bytes()).read_kernel() if arguments.starscr_reference else
              (build / "icestar_c.x").read_bytes())
    for patch in json.loads((root / "kernel/patches.json").read_text()):
        start, end = int(patch["start"], 0) - 0x800000, int(patch["end"], 0) - 0x800000
        if loaded[start:end] != kernel[start:end]:
            raise RuntimeError("loaded kernel patch differs: " + patch["name"])
    if arguments.surface_move_check:
        pcs = [int(match.group(1), 16) for match in
               re.finditer(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/", trace.read_text())]
        entry_hits = pcs.count(0x8191d8)
        body_hits = sum(0x8191d8 < pc < 0x8194e4 for pc in pcs)
        if not entry_hits or not body_hits:
            raise RuntimeError("recovered moved-layer entry and C body did not both execute")
        evidence = {"image": str(image),
                    "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
                    "trace_sha256": hashlib.sha256(trace.read_bytes()).hexdigest(),
                    "entry": "0x8191d8", "entry_execution_events": entry_hits,
                    "body_execution_events": body_hits,
                    "executed_block_addresses": [hex(pc) for pc in sorted(set(pcs))]}
        (build / "surface_move_execution.json").write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"Recovered moved-layer entry executed {entry_hits} times; "
              f"{body_hits} C-body/return block execution events")
    if arguments.surface_scale_check:
        pcs = [int(match.group(1), 16) for match in
               re.finditer(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/", trace.read_text())]
        entry_hits = pcs.count(0x8194e4)
        body_hits = sum(0x8194e4 < pc < 0x819c40 for pc in pcs)
        if entry_hits != 6 or not body_hits:
            raise RuntimeError("expected six recovered scaler entries and executed C body blocks")
        evidence = {"image": str(image), "diagnostic_only": True,
                    "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
                    "trace_sha256": hashlib.sha256(trace.read_bytes()).hexdigest(),
                    "entry": "0x8194e4", "entry_execution_events": entry_hits,
                    "body_execution_events": body_hits,
                    "installed_intervals_verified": len(json.loads((root / "kernel/patches.json").read_text())),
                    "executed_block_addresses": [hex(pc) for pc in sorted(set(pcs))]}
        (build / "surface_scale_execution.json").write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"Recovered scaler entry executed {entry_hits} times; "
              f"{body_hits} C-body/return block execution events")
    if arguments.surface_lifecycle_check:
        pcs = [int(match.group(1), 16) for match in
               re.finditer(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/", trace.read_text())]
        functions = {}
        for name, start, end in (("init", 0x8175c0, 0x817626),
                                 ("create", 0x817626, 0x8177c4),
                                 ("find_free", 0x8177c4, 0x81783e),
                                 ("delete", 0x81783e, 0x8178f0)):
            entries = pcs.count(start)
            if not entries:
                raise RuntimeError("recovered surface routine was not observed: " + name)
            functions[name] = {"entry": hex(start), "entry_execution_events": entries,
                               "body_execution_events": sum(start < pc < end for pc in pcs),
                               "executed_block_addresses": [hex(pc) for pc in sorted(set(pcs))
                                                            if start <= pc < end]}
        evidence = {"image": str(image), "diagnostic_only": False,
                    "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
                    "trace_sha256": hashlib.sha256(trace.read_bytes()).hexdigest(),
                    "installed_intervals_verified": len(json.loads((root / "kernel/patches.json").read_text())),
                    "functions": functions}
        (build / "surface_lifecycle_execution.json").write_text(json.dumps(evidence, indent=2) + "\n")
        print("Recovered surface lifecycle entries observed: " +
              ", ".join(f"{name}={data['entry_execution_events']}" for name, data in functions.items()))
    if arguments.surface_order_check:
        pcs = [int(match.group(1), 16) for match in
               re.finditer(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/", trace.read_text())]
        functions = {}
        for name, start, end in (("remove", 0x8178f0, 0x8179d6),
                                 ("append", 0x8179d6, 0x817a3c),
                                 ("front", 0x817a3c, 0x817b32),
                                 ("move", 0x817b32, 0x817cd0)):
            entries = pcs.count(start)
            body_events = sum(start < pc < end for pc in pcs)
            if not entries or not body_events:
                raise RuntimeError("recovered surface order entry/body was not observed: " + name)
            functions[name] = {"entry": hex(start), "entry_execution_events": entries,
                               "body_execution_events": body_events,
                               "executed_block_addresses": [hex(pc) for pc in sorted(set(pcs))
                                                            if start <= pc < end]}
        evidence = {"image": str(image), "diagnostic_only": False,
                    "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
                    "trace_sha256": hashlib.sha256(trace.read_bytes()).hexdigest(),
                    "installed_intervals_verified": len(json.loads((root / "kernel/patches.json").read_text())),
                    "functions": functions}
        (build / "surface_order_execution.json").write_text(json.dumps(evidence, indent=2) + "\n")
        print("Recovered surface order entries observed: " +
              ", ".join(f"{name}={data['entry_execution_events']}" for name, data in functions.items()))
    png_from_ppm(screen, build / f"{capture_prefix}.png")
    print("QEMU snapshot boot captured; " + ("reference kernel intervals" if arguments.starscr_reference else
          "all fixed-address C patches") + " match guest memory")
    print("Screenshot: " + str(build / f"{capture_prefix}.png"))
    print("Desktop appearance still requires visual inspection; this is not a full OS test")


def check_inputs(execute, build, image):
    """Use only emulated PS/2 events; inspect state through the monitor."""
    evidence = {}
    scratch = build / "input_state.memory"

    def read_memory(address, size):
        execute("stop")
        try:
            scratch.unlink(missing_ok=True)
            execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
            data = scratch.read_bytes()
            if len(data) != size:
                raise RuntimeError("incomplete guest state read")
            return data
        finally:
            execute("cont")

    def dword(address):
        return struct.unpack("<I", read_memory(address, 4))[0]

    def position():
        pointer = dword(0x1b048)
        return struct.unpack("<ii", read_memory(pointer + 12, 8))

    def capture(name):
        ppm = build / f"input_{name}.ppm"
        execute("stop")
        try:
            execute(f'screendump "{ppm}"')
        finally:
            execute("cont")
        png_from_ppm(ppm, build / f"input_{name}.png")

    def move_to(x, y):
        for _ in range(30):
            px, py = position()
            dx, dy = x - px, y - py
            if abs(dx) <= 2 and abs(dy) <= 2:
                return px, py
            dx, dy = max(-100, min(100, dx)), max(-100, min(100, dy))
            execute(f"mouse_move {dx} {dy}")
            time.sleep(0.12)
        raise RuntimeError(f"pointer did not reach {(x, y)}; observed {position()}")

    evidence["pointer_initial"] = position()
    capture("desktop")
    evidence["pointer_cmd_icon"] = move_to(45, 282)
    for _ in range(2):
        execute("mouse_button 1")
        time.sleep(0.07)
        execute("mouse_button 0")
        time.sleep(0.07)
    time.sleep(3)
    capture("cmd")
    # Match immutable instructions, then validate mutable CMD state. Cached
    # file copies can contain the same instructions but never consume input.
    command_file = Fat16(image.read_bytes()).read_file(b"ICESTAR    ", b"CMD     X  ")
    ram_start = 0x900000
    ram = read_memory(ram_start, 128 * 1024 * 1024 - ram_start)
    # The operand at file offset 0x6f is relocated, as are later absolute
    # references. Match the first instructions preceding that operand.
    signature = command_file[0x40:0x6b]
    candidates = []
    offset = ram.find(signature)
    while offset >= 0:
        base = ram_start + offset - 0x40
        # The store operand at 0xc8 must reference this loaded copy's BSS,
        # distinguishing the running relocated program from its file cache.
        if struct.unpack_from("<I", ram, offset - 0x40 + 0xc8)[0] == base + 0xf490:
            candidates.append(base)
        offset = ram.find(signature, offset + 1)
    del ram
    evidence["cmd_code_candidates"] = [hex(base) for base in candidates]
    if len(candidates) != 1:
        raise RuntimeError("expected one relocated CMD instance after clicking its icon")
    cmd_base = candidates[0]
    queue = dword(0x1b038)
    evidence["keyboard_queue_before"] = list(struct.unpack("<4I", read_memory(queue, 16)))
    for key in ("c", "p", "u", "i", "d"):
        execute(f"sendkey {key} 50")
        time.sleep(0.15)
    time.sleep(1)
    capture("typed")
    evidence["keyboard_queue_after"] = list(struct.unpack("<4I", read_memory(queue, 16)))
    before, after = evidence["keyboard_queue_before"], evidence["keyboard_queue_after"]
    if before[2:] != [0, 2048] or after[2:] != [0, 2048]:
        raise RuntimeError("keyboard queue was not drained or changed capacity")
    if [(after[i] - before[i]) % 2048 for i in range(2)] != [10, 10]:
        raise RuntimeError("five key make/break pairs did not traverse the queue")
    typed = read_memory(cmd_base + 0xb2da, 6)
    evidence["cmd_input"] = typed.rstrip(b"\0").decode("ascii")
    if typed != b"cpuid\0":
        raise RuntimeError("CMD did not receive the expected text")
    evidence["cmd_cursor_before_return"] = list(struct.unpack("<2I", read_memory(cmd_base + 0xf46c, 8)))
    execute("sendkey ret 50")
    time.sleep(2)
    capture("cpuid")
    evidence["cmd_cursor_after_return"] = list(struct.unpack("<2I", read_memory(cmd_base + 0xf46c, 8)))
    column, row = evidence["cmd_cursor_after_return"]
    if column != 4 or row != evidence["cmd_cursor_before_return"][1] + 2:
        raise RuntimeError("CMD did not print a result and return to its prompt")
    if read_memory(cmd_base + 0xb2da, 6) != b"\0" * 6:
        raise RuntimeError("CMD did not clear its command buffer")
    (build / "input_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("Input state: " + json.dumps(evidence))
    print("Mouse position, relocated CMD, input text, queue consumption, and prompt return verified")
    print("Inspect input_cpuid.png for the command result; its text is not OCR-checked")
    return cmd_base


if __name__ == "__main__":
    main()
