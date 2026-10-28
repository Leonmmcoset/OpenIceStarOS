"""Check natural execution of the installed C DIB decoder in a production image."""
import hashlib
import json
import re

from kernel_image import BASE, Fat16


def check_icon_dib_execution(root, image, trace):
    build = root / "build"
    manifest = json.loads((root / "kernel/patches.json").read_text())
    patch = next(item for item in manifest if item["binary"] == "kernel.icon_dib.bin")
    start, end = int(patch["start"], 0), int(patch["end"], 0)
    body = (build / patch["binary"]).read_bytes()
    if not body or len(body) > end - start:
        raise RuntimeError("DIB compiled body is empty or exceeds its slot")
    kernel = (build / "openicestaros_kernel.x").read_bytes()
    expected = body + b"\x90" * (end - start - len(body))
    if kernel[start - BASE:end - BASE] != expected:
        raise RuntimeError("installed DIB slot differs from compiled body and NOP padding")
    if Fat16(image.read_bytes()).read_kernel() != kernel:
        raise RuntimeError("boot image kernel differs from the current production kernel")
    pcs = [int(match.group(1), 16) for match in
           re.finditer(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/", trace.read_text())]
    entry_records = sum(pc == start for pc in pcs)
    body_records = sum(start < pc < start + len(body) for pc in pcs)
    if not entry_records or not body_records:
        raise RuntimeError("DIB decoder entry and compiled body did not both execute")
    evidence = {
        "image": str(image), "diagnostic_only": False,
        "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
        "kernel_sha256": hashlib.sha256(kernel).hexdigest(),
        "compiled_body_sha256": hashlib.sha256(body).hexdigest(),
        "trace_sha256": hashlib.sha256(trace.read_bytes()).hexdigest(),
        "entry": hex(start), "compiled_bytes": len(body), "slot_bytes": end - start,
        "installed_intervals_verified": len(manifest),
        "entry_trace_records": entry_records, "body_trace_records": body_records,
        "executed_block_addresses": [hex(pc) for pc in sorted(set(pcs))],
        "limitation": "QEMU TB chaining means trace records are not an exact invocation count; "
                      "this trace does not establish coverage of every conversion path.",
    }
    (build / "icon_dib_execution.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"Recovered DIB decoder executed naturally: {entry_records} entry trace records; "
          f"{body_records} compiled-body trace records")
