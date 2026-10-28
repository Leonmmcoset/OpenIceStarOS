"""Verify the installed C math entries/bodies execute in a normal CLOCK run."""
from collections import Counter
import hashlib
import json
import re

from kernel_image import BASE, Fat16, require
from icon_render_smoke import file_sha256

SLOTS = {"factorial":(0x81a2aa,0x81a2dc), "sine":(0x81a2dc,0x81a366),
         "cosine":(0x81a366,0x81a3f6), "acos":(0x81a3f6,0x81a4fc),
         "round":(0x81a4fc,0x81a538), "power":(0x81a538,0x81a604),
         "absolute":(0x81a604,0x81a634), "format_float":(0x81a634,0x81a9ee),
         "floor":(0x81a9ee,0x81aa52), "ceil":(0x81aa52,0x81aab8),
         "remainder":(0x81aab8,0x81ab2a), "sqrt":(0x81ab2a,0x81ab34)}
REQUIRED_CLOCK_FUNCTIONS = {"factorial", "sine", "cosine", "power"}
TRACE_FILTER = ",".join(f"{a:#x}..{b-1:#x}" for a,b in SLOTS.values())


def check_math_execution(root,image,trace):
    build=root/"build"
    kernel=(build/"openicestaros_kernel.x").read_bytes()
    require(Fat16(image.read_bytes()).read_kernel()==kernel,"math disk/production kernel mismatch")
    manifest=json.loads((root/"kernel/patches.json").read_text())
    by_binary={p["binary"]:p for p in manifest}
    counts=Counter()
    pattern=re.compile(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/")
    with trace.open() as stream:
        for line in stream:
            match=pattern.search(line)
            if match: counts[int(match.group(1),16)]+=1
    slots={}
    for name,(start,end) in SLOTS.items():
        binary=f"kernel.{name}.bin";patch=by_binary[binary]
        require((int(patch["start"],0),int(patch["end"],0))==(start,end),"math manifest addresses")
        body=(build/binary).read_bytes()
        require(0<len(body)<=end-start,"math compiled size")
        require(kernel[start-BASE:end-BASE]==body+b"\x90"*(end-start-len(body)),"math installed body/padding")
        entry=counts[start];inside=sum(n for pc,n in counts.items() if start<pc<start+len(body))
        if name in REQUIRED_CLOCK_FUNCTIONS:
            require(entry and inside,"math entry/body did not execute: "+name)
        slots[name]=dict(entry=hex(start),compiled_bytes=len(body),slot_bytes=end-start,
            entry_trace_records=entry,body_trace_records=inside,
            observed_entry_and_body=bool(entry and inside),
            executed_block_addresses=[hex(pc) for pc in sorted(counts) if start<=pc<end])
    report=dict(image=str(image),image_sha256=file_sha256(image),
        kernel_sha256=hashlib.sha256(kernel).hexdigest(),trace_sha256=file_sha256(trace),
        installed_intervals_verified=len(manifest),diagnostic_only=False,boundary_substitutions=0,
        slots=slots,required_clock_functions=sorted(REQUIRED_CLOCK_FUNCTIONS),
        limitation="QEMU TB records are not exact function call counts; unobserved functions require separate differential evidence.")
    (build/"math_series_execution.json").write_text(json.dumps(report,indent=2)+"\n")
    for name,s in slots.items():
        print(f"{name}: {s['entry_trace_records']} entry / {s['body_trace_records']} body records in normal CLOCK run")
