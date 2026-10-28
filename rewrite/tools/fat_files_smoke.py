"""Verify real production execution of the recovered FAT file readers."""
import hashlib
import json
import re

from kernel_image import require


def check_fat_execution(root, image, trace):
    pcs = [int(m.group(1), 16) for m in
           re.finditer(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/", trace.read_text())]
    functions = {}
    for name, start, end in (
            ("allocate_record", 0x827454, 0x82760c),
            ("read_record", 0x82760c, 0x827798),
            ("allocate_path", 0x827798, 0x8277dc),
            ("read_path", 0x8277dc, 0x827826),
            ("header_relative", 0x827826, 0x8278f4),
            ("header_record", 0x8278f4, 0x82798c)):
        functions[name] = dict(entry=hex(start), entry_events=pcs.count(start),
            body_events=sum(start < pc < end for pc in pcs),
            executed_blocks=[hex(pc) for pc in sorted(set(pcs)) if start <= pc < end])
    for name in ("allocate_record", "read_record", "allocate_path", "header_record"):
        require(functions[name]["entry_events"] > 0 and functions[name]["body_events"] > 0,
                "recovered FAT entry/body was not observed: " + name)
    evidence = dict(image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest(), functions=functions,
        installed_intervals=len(json.loads((root / "kernel/patches.json").read_text())),
        scope="Production boot and CMD/application paths; unobserved entries require differential fixtures.")
    (root / "build/fat_files_execution.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("Recovered FAT reader execution: " + ", ".join(
        f"{name}={item['entry_events']}" for name, item in functions.items()))
