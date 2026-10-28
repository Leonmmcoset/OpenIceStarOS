"""Observe actual C timer preemption through saved TSS/stack state in QEMU."""

import json
import struct
import time

from kernel_image import require
from xformat import Elf32


def check_task_switch(execute, build):
    scratch = build / "task_state.memory"
    elf = Elf32((build / "kernel_patch.elf").read_bytes())
    symbols = {}
    for index, section in enumerate(elf.sections):
        if section[1] != 2:
            continue
        strings = elf.section_data(section[6])
        for name, value, size, _, _, _ in elf.symbol_table(index):
            if name and size:
                symbols[elf.string(strings, name)] = (value, size)
    kernel = (build / "openicestaros_kernel.x").read_bytes()
    jump, jump_size = symbols["kernel_far_task_jump"]
    require(jump_size == 5 and kernel[jump - 0x800000:jump - 0x800000 + 5] ==
            b"\xff\x6c\x24\x04\xc3", "unexpected far task jump boundary")
    returns = {}
    for name in ("kernel_schedule_task", "kernel_yield_task"):
        address, size = symbols[name]
        code = kernel[address - 0x800000:address - 0x800000 + size]
        calls = [address + i + 5 for i in range(len(code) - 4)
                 if code[i] == 0xe8 and
                 address + i + 5 + struct.unpack_from("<i", code, i + 1)[0] == jump]
        require(len(calls) == 1, "expected one far task jump call in " + name)
        returns[calls[0]] = name

    def read_paused(address, size):
        scratch.unlink(missing_ok=True)
        execute(f'pmemsave {address:#x} {size:#x} "{scratch}"')
        data = scratch.read_bytes()
        require(len(data) == size, "incomplete task state read")
        return data

    def dword(address):
        return struct.unpack("<I", read_paused(address, 4))[0]

    evidence = {"expected_far_return_eip": hex(jump + 4),
                "expected_c_returns": {hex(a): n for a, n in returns.items()},
                "samples": []}
    start_tick = None
    observed = {}
    deadline = time.monotonic() + 8
    try:
        while time.monotonic() < deadline:
            execute("stop")
            try:
                tick, count, table, current = [dword(a) for a in
                                               (0x1b084, 0x1b0d0, 0x1b0c8, 0x1b0d8)]
                require(1 < count <= 4096, "unexpected task high-water mark")
                require(table + count * 108 < 128 * 1024 * 1024,
                        "task table outside guest RAM")
                records = read_paused(table, count * 108)
                matches = []
                for index in range(count):
                    record = struct.unpack_from("<27I", records, index * 108)
                    if not record[0] or index == current:
                        continue
                    tss = record[7]
                    require(tss + 104 < 128 * 1024 * 1024, "TSS outside guest RAM")
                    state = read_paused(tss, 104)
                    eip, esp = [struct.unpack_from("<I", state, a)[0] for a in (0x20, 0x38)]
                    if eip != jump + 4:
                        continue
                    require(esp + 12 < 128 * 1024 * 1024, "saved stack outside guest RAM")
                    continuation, offset, selector = struct.unpack("<3I", read_paused(esp, 12))
                    if continuation not in returns:
                        continue
                    require(offset == 0 and selector % 8 == 0,
                            "saved far pointer does not match the task-switch ABI")
                    match = {"task": index, "tss": hex(tss), "saved_eip": hex(eip),
                             "saved_esp": hex(esp), "c_return": hex(continuation),
                             "entry": returns[continuation], "selector": hex(selector),
                             "schedule_counter": record[3]}
                    matches.append(match)
                    observed[(index, continuation)] = match
                if start_tick is None:
                    start_tick = tick
                delta = (tick - start_tick) & 0xffffffff
                evidence["samples"].append({"tick": tick, "current_task": current,
                                             "task_high_water": count, "suspended_c_tasks": matches})
                timer_seen = any(m["entry"] == "kernel_schedule_task" for m in observed.values())
            finally:
                execute("cont")
            if timer_seen and 3 <= delta < 0x80000000:
                evidence["tick_advance"] = delta
                evidence["observed_c_tasks"] = list(observed.values())
                break
            time.sleep(0.12)
        else:
            raise RuntimeError("no advancing timer with a suspended C scheduler continuation")
    finally:
        (build / "task_smoke.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("Advancing timer and real TSS switches verified through suspended C scheduler stacks")
