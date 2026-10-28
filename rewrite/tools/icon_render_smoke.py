"""Observe the installed DIB decoder and real helpers during a normal boot."""
from collections import Counter
import hashlib
import json
import re

from kernel_image import BASE, Fat16

SLOTS = {
    "icon_put8": (0x81b37c, 0x81b3c0),
    "icon_put32": (0x81b3c0, 0x81b406),
    "icon_quantize": (0x81fd90, 0x81fec2),
    "icon_dib": (0x82b20c, 0x82b678),
    "pixel_put8": (0x81b26e, 0x81b2f4),
    "pixel_put32": (0x81b2f4, 0x81b37c),
    "pixel_get8": (0x81b406, 0x81b486),
    "pixel_get32": (0x81b486, 0x81b514),
    "pixel_sample": (0x81b514, 0x81b714),
    "palette_color": (0x81b714, 0x81b734),
    "raster_line8": (0x81b734, 0x81b8dc),
    "raster_line32": (0x81b8dc, 0x81ba8a),
    "raster_fill8": (0x81ba8a, 0x81bd26),
    "raster_fill32": (0x81bd26, 0x81bf50),
    "raster_gradient32": (0x81bf50, 0x81c058),
    "raster_outline8": (0x81c058, 0x81c1e4),
    "raster_outline32": (0x81c1e4, 0x81c396),
    "raster_circle8": (0x81c396, 0x81c6f2),
    "raster_circle32": (0x81c6f2, 0x81ca82),
    "raster_disc8": (0x81ca82, 0x81cd50),
    "raster_disc32": (0x81cd50, 0x81d032),
    "brush_square": (0x81d032, 0x81d070),
    "brush_line": (0x81d070, 0x81d1fc),
    "brush_circle": (0x81d1fc, 0x81d4e6),
    "circle_quadrants": (0x81d4e6, 0x81d6c2),
    "rounded_outline": (0x81d6c2, 0x81d8ca),
    "rounded_fill": (0x81d8ca, 0x81da68),
    "chamfer_fill": (0x81da68, 0x81db5a),
}
TRACE_FILTER = ",".join(f"{start:#x}..{end-1:#x}" for start, end in SLOTS.values())


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def check_icon_render_execution(root, image, trace):
    build = root / "build"
    manifest = json.loads((root / "kernel/patches.json").read_text())
    by_binary = {item["binary"]: item for item in manifest}
    kernel = (build / "icestar_c.x").read_bytes()
    if Fat16(image.read_bytes()).read_kernel() != kernel:
        raise RuntimeError("boot image kernel differs from the production kernel")
    counts = Counter()
    pattern = re.compile(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/")
    with trace.open() as stream:
        for line in stream:
            match = pattern.search(line)
            if match:
                counts[int(match.group(1), 16)] += 1
    slots = {}
    for name, (start, end) in SLOTS.items():
        binary = f"kernel.{name}.bin"
        patch = by_binary[binary]
        if (int(patch["start"], 0), int(patch["end"], 0)) != (start, end):
            raise RuntimeError("unexpected manifest addresses: " + name)
        body = (build / binary).read_bytes()
        if not 0 < len(body) <= end-start:
            raise RuntimeError("invalid compiled body length: " + name)
        if kernel[start-BASE:end-BASE] != body + b"\x90" * (end-start-len(body)):
            raise RuntimeError("installed body/padding mismatch: " + name)
        entry_records = counts[start]
        body_records = sum(n for pc, n in counts.items() if start < pc < start+len(body))
        slots[name] = dict(entry=hex(start), compiled_bytes=len(body), slot_bytes=end-start,
                           compiled_body_sha256=hashlib.sha256(body).hexdigest(),
                           entry_trace_records=entry_records, body_trace_records=body_records,
                           executed_block_addresses=[hex(pc) for pc in sorted(counts)
                                                     if start <= pc < end])
    executed = lambda name: (slots[name]["entry_trace_records"] and
                              slots[name]["body_trace_records"])
    if not executed("icon_dib"):
        raise RuntimeError("DIB entry and compiled body did not both execute")
    if not any(executed(name) for name in ("icon_put8", "icon_put32")):
        raise RuntimeError("no recovered pixel writer entry/body executed")
    evidence = dict(image=str(image), diagnostic_only=False, boundary_substitutions=0,
                    image_sha256=file_sha256(image),
                    kernel_sha256=hashlib.sha256(kernel).hexdigest(),
                    trace_sha256=file_sha256(trace), installed_intervals_verified=len(manifest),
                    slots=slots,
                    limitation="Trace records under QEMU TB chaining are not exact call counts. "
                               "Unobserved helpers and conversion branches are not covered by this boot.")
    (build / "icon_render_execution.json").write_text(json.dumps(evidence, indent=2)+"\n")
    for name, slot in slots.items():
        print(f"{name}: {slot['entry_trace_records']} entry records; "
              f"{slot['body_trace_records']} body records during normal production boot")
