"""Install explicit display-only branding; this does not recover program logic."""

import argparse
import hashlib
import json
from pathlib import Path
import re

from kernel_image import Fat16, require, write_new

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "branding/patches.json"
KERNEL = "/ICESTAR/ICESTAR.X"


def patch_file(data, item):
    result = bytearray(data)
    end = 0
    for edit in sorted(item["edits"], key=lambda e: int(e["offset"], 0)):
        start = int(edit["offset"], 0)
        before, after = bytes.fromhex(edit["before_hex"]), bytes.fromhex(edit["after_hex"])
        require(start >= end and len(before) == len(after) > 0, "invalid branding interval")
        end = start + len(before)
        require(data[start:end] == before, "branding source bytes differ: " + item["path"])
        result[start:end] = after
    require(len(result) == len(data), "branding changed file length")
    return bytes(result)


def brand_kernel(data):
    item = next(i for i in json.loads(MANIFEST.read_text()) if i["path"] == KERNEL)
    # Display edits must never silently override any source-built code interval.
    intervals = json.loads((ROOT / "kernel/patches.json").read_text())
    for edit in item["edits"]:
        start = int(edit["offset"], 0) + 0x800000
        end = start + len(bytes.fromhex(edit["before_hex"]))
        require(all(end <= int(p["start"], 0) or start >= int(p["end"], 0)
                    for p in intervals), "branding overlaps a kernel source interval")
    return patch_file(data, item)


def installed(source):
    result = source
    for item in json.loads(MANIFEST.read_text()):
        path = tuple(n.encode("ascii") for n in item["short_names"])
        disk = Fat16(result)
        original = disk.read_file(*path)
        require(hashlib.sha256(original).hexdigest() == item["original_sha256"],
                "branding original SHA-256 mismatch: " + item["path"])
        replacement = brand_kernel(original) if item["path"] == KERNEL else patch_file(original, item)
        result = disk.replace_file(replacement, *path)
    return bytes(result)


def verify_boot(source, actual, build, stage3=False):
    expected = bytearray(installed(source))
    parts = [(0, 512, "mbr.bin"), (512, 1536, "menu_hybrid.bin" if stage3 else "menu.bin")]
    if stage3:
        parts.append((2560, 4096, "stage3_hybrid.bin"))
    for offset, size, name in parts:
        data = (Path(build) / name).read_bytes()
        require(len(data) == size, "boot binary length changed")
        expected[offset:offset+size] = data
    require(actual == expected, "disk differs outside declared branding/boot intervals")


def files(data, extents=False):
    """Walk live FAT files, excluding deleted entries, slack and directory labels."""
    from kernel_image import word
    from binary_inventory import short_name
    disk = Fat16(data)

    def visit(directory, path=(), ancestors=()):
        for offset in range(0, len(directory), 32):
            entry = directory[offset:offset+32]
            if not entry[0]:
                break
            if entry[0] in (0xe5, 46) or entry[11] & 8 or entry[11] == 0x0f:
                continue
            names = path + (entry[:11],)
            if entry[11] & 16:
                cluster = word(entry, 26)
                require(cluster not in ancestors, "cyclic branding scan directory")
                contents = b"".join(data[o:o+disk.cluster_bytes] for o in disk.chain(cluster))
                yield from visit(contents, names, ancestors + (cluster,))
            else:
                yield ("/" + "/".join(short_name(n) for n in names),
                       disk.file_extents(*names) if extents else disk.read_file(*names))

    yield from visit(disk.root)


def scan(data):
    findings = []
    # ASCII also covers GBK-embedded Latin product names. Check both UTF-16 orders.
    for path, content in [("<boot>", data[:17*512]), *files(data)]:
        for encoding, unit in (("ascii", 1), ("utf-16le", 2), ("utf-16be", 2)):
            pattern = "IceStar".encode(encoding)
            for match in re.finditer(re.escape(pattern), content, re.I):
                offset = match.start()
                previous = content[max(0, offset-4*unit):offset].decode(encoding, errors="replace")
                after = content[offset:offset+32*unit].decode(encoding, errors="replace")
                # New product name contains the old letters; it is not a residual.
                if previous.lower() == "open":
                    continue
                compatibility = (previous.endswith(("\\", "/")) or
                                 after.upper().startswith(("ICESTAR.X", "ICESTAR\x00")))
                findings.append(dict(path=path, offset=hex(offset), encoding=encoding,
                                     classification="compatibility-path" if compatibility else "REVIEW",
                                     context=previous + after))
    require(not any(f["classification"] == "REVIEW" for f in findings),
            "unclassified old product name: " + repr([f for f in findings if f["classification"] == "REVIEW"]))
    return findings


def raw_residuals(data):
    """Account for old display names outside live files without erasing evidence."""
    from kernel_image import word
    disk = Fat16(data)
    live = [extent for _, extents in files(data, extents=True) for extent in extents]
    findings = []
    for match in re.finditer(rb"icestar ?os", data, re.I):
        offset = match.start()
        if data[offset-4:offset].lower() == b"open":
            continue
        require(not any(start <= offset < start+size for start, size in live),
                "old display brand remains in a live file")
        require(disk.data_start <= offset < disk.end, "old display brand outside data area")
        cluster = (offset-disk.data_start)//disk.cluster_bytes+2
        allocated = word(disk.fat, cluster*2) != 0
        require(not allocated or any(
            (start-disk.data_start)//disk.cluster_bytes+2 == cluster
            and offset >= start+size for start, size in live),
            "old display brand in an unclassified allocated cluster")
        findings.append(dict(offset=hex(offset), cluster=cluster,
            classification="file-slack" if allocated else "free-cluster",
            retained_original_bytes=True))
    return findings


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("command", choices=("install", "verify-boot", "scan"))
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--stage3", action="store_true")
    args = parser.parse_args()
    source = args.source.read_bytes()
    if args.command == "install":
        write_new(args.output, installed(source), args.source)
    elif args.command == "verify-boot":
        verify_boot(source, args.output.read_bytes(), args.build, args.stage3)
        print("Entire boot disk verified: only declared boot and branding intervals changed")
    else:
        findings = scan(source)
        args.output.write_text(json.dumps(dict(image=args.source.name,
            sha256=hashlib.sha256(source).hexdigest(), unclassified=0,
            retained=findings, raw_display_residuals=raw_residuals(source)), indent=2) + "\n")
        print(f"Live files/boot ASCII and UTF-16 scan passed; {len(findings)} compatibility references retained")


if __name__ == "__main__":
    main()
