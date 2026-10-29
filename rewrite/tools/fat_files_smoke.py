"""Verify real production execution of the recovered FAT file readers."""
import hashlib
import json
import re

from kernel_image import require


def check_fat_execution(root, image, trace, require_relative=False):
    pcs = [int(m.group(1), 16) for m in
           re.finditer(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/", trace.read_text())]
    functions = {}
    for name, start, end in (
            ("root_name", 0x8258a6, 0x825a7e),
            ("directory_name", 0x825a7e, 0x825bcc),
            ("root_extension", 0x825bcc, 0x825c46),
            ("directory_extension", 0x825c46, 0x825c8e),
            ("find_relative", 0x825c8e, 0x825d34),
            ("directory_find", 0x825d34, 0x825df4),
            ("root_size", 0x826940, 0x82698c),
            ("directory_size", 0x82698c, 0x8269a6),
            ("root_attributes", 0x8269a6, 0x8269f2),
            ("directory_attributes", 0x8269f2, 0x826a0e),
            ("find_path", 0x8263c0, 0x826940),
            ("name83", 0x827bba, 0x827c72),
            ("relative_read", 0x826ade, 0x826c3a),
            ("relative_named", 0x826c3a, 0x826c6e),
            ("relative_cluster", 0x826c6e, 0x826dd0),
            ("relative_named_cluster", 0x826dd0, 0x826f38),
            ("relative_allocate_named", 0x826f38, 0x827032),
            ("relative_allocate_index", 0x827032, 0x82711e),
            ("allocate_directory_index", 0x82711e, 0x8272f4),
            ("allocate_clusters", 0x8272f4, 0x827454),
            ("allocate_record", 0x827454, 0x82760c),
            ("read_record", 0x82760c, 0x827798),
            ("allocate_path", 0x827798, 0x8277dc),
            ("read_path", 0x8277dc, 0x827826),
            ("header_relative", 0x827826, 0x8278f4),
            ("header_record", 0x8278f4, 0x82798c)):
        functions[name] = dict(entry=hex(start), entry_events=pcs.count(start),
            body_events=sum(start < pc < end for pc in pcs),
            executed_blocks=[hex(pc) for pc in sorted(set(pcs)) if start <= pc < end])
    required = ["find_path", "name83", "allocate_record", "read_record", "allocate_path", "header_record"]
    if require_relative:
        required += ["relative_read", "relative_named", "relative_cluster",
                     "relative_named_cluster", "relative_allocate_named", "relative_allocate_index",
                     "allocate_directory_index", "allocate_clusters"]
        required += ["root_name", "directory_name", "root_extension", "directory_extension",
                     "find_relative", "directory_find", "root_size", "directory_size",
                     "root_attributes", "directory_attributes"]
    single_block = {"root_extension", "directory_extension", "root_size", "directory_size",
                    "root_attributes", "directory_attributes"}
    for name in required:
        require(functions[name]["entry_events"] > 0 and
                (name in single_block or functions[name]["body_events"] > 0),
                "recovered FAT entry/body was not observed: " + name)
    evidence = dict(diagnostic_only=require_relative,
        image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        trace_sha256=hashlib.sha256(trace.read_bytes()).hexdigest(), functions=functions,
        installed_intervals=len(json.loads((root / "kernel/patches.json").read_text())),
        scope=("Separate diagnostic X using production kernel services and real disk I/O."
               if require_relative else
               "Production boot and CMD/application paths; unobserved entries require differential fixtures."))
    (root / "build/fat_files_execution.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("Recovered FAT reader execution: " + ", ".join(
        f"{name}={item['entry_events']}" for name, item in functions.items()))
