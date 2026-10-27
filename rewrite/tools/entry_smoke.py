"""Validate the recovered startup path in an unmodified production boot."""
import hashlib
import json
import re
import subprocess

from kernel_image import Fat16


def calls(path):
    assembly = subprocess.run(
        ["objdump", "-D", "-b", "binary", "-m", "i386", "--adjust-vma=0x800000",
         "--start-address=0x800040", "--stop-address=0x800104", str(path)],
        check=True, capture_output=True, text=True).stdout
    result = {"entry": [], "bootstrap": []}
    for line in assembly.splitlines():
        match = re.match(r"\s*([0-9a-f]+):.*\bcall\s+(0x[0-9a-f]+)\b", line)
        if match:
            address, target = (int(value, 16) for value in match.groups())
            result["entry" if address < 0x800070 else "bootstrap"].append((address, target))
    if len(result["entry"]) != 7 or len(result["bootstrap"]) != 22:
        raise RuntimeError("unexpected entry/bootstrap call counts")
    return result


def check_entry_execution(root, image, trace):
    build = root / "build"
    source = root.parent / "IceStarOS_x86_32M.vhd"
    original = build / "entry_boot_original.x"
    original.write_bytes(Fat16(source.read_bytes()).read_kernel())
    candidate = build / "icestar_c.x"
    expected_calls, installed_calls = calls(original), calls(candidate)
    for name in expected_calls:
        if [target for _, target in installed_calls[name]] != [
                target for _, target in expected_calls[name]]:
            raise RuntimeError("installed startup call order differs: " + name)
    entry, bootstrap = installed_calls["entry"], installed_calls["bootstrap"]
    if entry[0][1] != 0x800070 or entry[-1][1] != 0x8012ca:
        raise RuntimeError("startup must call bootstrap first and the event loop last")
    # Each CALL terminates an x86 translation block. Bootstrap returns; the
    # final event-loop call does not. Log from power-on so every one-time
    # startup block is observed before input and application tests begin.
    expected = ([0x800040, 0x800070] + [address + 5 for address, _ in bootstrap] +
                [address + 5 for address, _ in entry[:-1]])
    observed = [int(match.group(1), 16) for match in
                re.finditer(r"\[[0-9a-fA-F]+/([0-9a-fA-F]+)/", trace.read_text())]
    observed = [pc for pc in observed if 0x800040 <= pc < 0x800104]
    if observed != expected:
        raise RuntimeError("startup block sequence differs: " + json.dumps({
            "expected": [hex(pc) for pc in expected],
            "observed": [hex(pc) for pc in observed]}))
    evidence = {
        "image": str(image), "diagnostic_only": False,
        "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
        "kernel_sha256": hashlib.sha256(candidate.read_bytes()).hexdigest(),
        "trace_sha256": hashlib.sha256(trace.read_bytes()).hexdigest(),
        "installed_intervals_verified": len(json.loads((root / "kernel/patches.json").read_text())),
        "ordered_block_events": [hex(pc) for pc in observed],
        "ordered_external_calls": [hex(target) for _, target in bootstrap + entry[1:]],
        "final_event_loop_returned": False,
    }
    (build / "entry_execution.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"Recovered kernel entry/bootstrap: {len(observed)} ordered blocks; "
          "28 original call targets; event loop reached")
