"""Inventory original X/DX programs and their current source recovery status."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path

from kernel_image import BASE, Fat16, dword, require, word

ROOT = Path(__file__).resolve().parents[1]


def short_name(raw):
    name = raw[:8].rstrip().decode("cp437")
    extension = raw[8:11].rstrip().decode("cp437")
    return name + ("." + extension if extension else "")


def inventory():
    source = ROOT.parent / "IceStarOS_x86_32M.vhd"
    data = source.read_bytes()
    disk = Fat16(data)
    apps = json.loads((ROOT / "apps/images.json").read_text())
    recovered = {tuple(p.encode("ascii") for p in app["path"]): app for app in apps}
    patches = json.loads((ROOT / "kernel/patches.json").read_text())
    programs, files, directories = [], 0, 0

    def visit(contents, path=(), ancestors=()):
        nonlocal files, directories
        for offset in range(0, len(contents), 32):
            entry = contents[offset:offset+32]
            require(len(entry) == 32, "truncated directory")
            if not entry[0]:
                break
            if entry[0] in (0xe5, 46) or entry[11] & 8 or entry[11] == 0x0f:
                continue
            name = entry[:11]
            full_path = path + (name,)
            if entry[11] & 16:
                cluster = word(entry, 26)
                require(cluster not in ancestors, "cyclic directory ancestry")
                directory = b"".join(data[o:o+disk.cluster_bytes] for o in disk.chain(cluster))
                directories += 1
                visit(directory, full_path, ancestors + (cluster,))
                continue
            files += 1
            if name[8:11] not in (b"X  ", b"DX "):
                continue
            original = disk.read_file(*full_path)
            require(len(original) == dword(entry, 28) and len(original) >= 64, "program size mismatch")
            require(dword(original, 4) == len(original), "program header size mismatch")
            digest = hashlib.sha256(original).hexdigest()
            display_path = "/" + "/".join(short_name(p) for p in full_path)
            role = "driver" if name[8:11] == b"DX " else "application"
            recovery = dict(state="unrecovered", sources=[])
            if full_path == (b"INIT    X  ",):
                role = "init"
                recovery = dict(state="partial", sources=["init/entry.c", "init/directory.c", "init/clusters.c"],
                                note="Recovered startup/filesystem paths still execute within original INIT.")
            elif full_path == (b"ICESTAR    ", b"ICESTAR X  "):
                role = "kernel"
                previous = BASE + 64
                for p in patches:
                    start, end = int(p["start"], 0), int(p["end"], 0)
                    require(previous <= start < end <= BASE+len(original), "invalid kernel intervals")
                    previous = end
                slot_bytes = sum(int(p["end"], 0)-int(p["start"], 0) for p in patches)
                recovery = dict(state="partial", sources=["kernel/patches.json", "kernel/patches.ld", "kernel/"],
                    declared_intervals=len(patches), declared_slot_bytes=slot_bytes,
                    exact_instruction_intervals=sum(bool(p.get("exact")) for p in patches),
                    retained_payload_bytes=len(original)-64-slot_bytes,
                    note="Slot bytes include padding and exact hardware instructions; these are not semantic completion percentages.")
            elif full_path in recovered:
                app = recovered[full_path]
                require(digest == app["original_sha256"], "original application hash mismatch")
                stem = Path(app["binary"]).stem
                recovery = dict(state="rebuilt", sources=[f"apps/{stem}.c", f"apps/{stem}.ld"],
                    output=f"build/{app['binary']}",
                    note="Reachable application logic and required API wrappers rebuilt; the guest OS still contains original components.")
            for p in recovery["sources"]:
                require((ROOT / p).exists(), "missing recovery source: " + p)
            programs.append(dict(path=display_path, short_names=[p.decode("ascii") for p in full_path],
                role=role, bytes=len(original), sha256=digest,
                header=dict(kind=hex(dword(original, 0)), entry=hex(dword(original, 16)),
                    stack_pages=dword(original, 12), relocation_count=dword(original, 32),
                    relocation_offset=hex(dword(original, 36)), bss_bytes=dword(original, 44)),
                recovery=recovery))

    visit(disk.root)
    programs.sort(key=lambda p: p["path"])
    require(set(recovered) <= {tuple(n.encode("ascii") for n in p["short_names"]) for p in programs},
            "recovery manifest path not found")
    groups = defaultdict(list)
    for p in programs:
        groups[p["sha256"]].append(p["path"])
    report = dict(schema=1, source=dict(path=source.name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()),
        scope="Original FAT16 .X/.DX program files and separately listed boot regions.",
        complete=False, counts=dict(files=files, directories=directories, program_paths=len(programs),
            unique_program_binaries=len(groups), roles=dict(sorted(Counter(p["role"] for p in programs).items())),
            recovery=dict(sorted(Counter(p["recovery"]["state"] for p in programs).items()))),
        duplicate_binaries=[dict(sha256=h, paths=paths) for h, paths in sorted(groups.items()) if len(paths)>1],
        boot=[dict(region="MBR", offset="0x0", bytes=512, recovery="C implementation with assembly entry and retained partition data",
                   sources=["boot/mbr.c", "boot/mbr_entry.S"]),
              dict(region="menu", offset="0x200", bytes=1536, recovery="C implementation using BIOS tick polling in place of original RTC handler",
                   sources=["boot/menu.c", "boot/menu_entry.S"]),
              dict(region="third stage", offset="0xa00", bytes=word(data, 0xa00), recovery="source-built C/assembly replacement; VBE, GDT, A20, CPU/display setup, PCI and INIT load; no incbin",
                   sources=["boot/stage3_hybrid.S", "boot/stage3_hybrid.ld", "boot/stage3_video.c", "boot/stage3_video.h",
                            "boot/stage3_video_io.S", "boot/stage3_display.c", "boot/stage3_load.c",
                            "boot/fat16.c", "boot/pci.c", "boot/init_transfer.c"])],
        programs=programs)
    for boot in report["boot"]:
        for path in boot["sources"]:
            require((ROOT/path).is_file(), "missing boot recovery source: " + path)
    return report


def markdown(report):
    count = report["counts"]
    lines = ["# Binary recovery inventory", "",
        "Generated from the original VHD and the committed recovery manifests by",
        "`python3 tools/binary_inventory.py`. Recheck with `make inventory-check`.", "",
        f"There are {count['program_paths']} original `.X`/`.DX` paths and "
        f"{count['unique_program_binaries']} distinct binaries. "
        f"{count['recovery'].get('rebuilt', 0)} applications are rebuilt, "
        f"{count['recovery'].get('partial', 0)} programs are partially recovered and "
        f"{count['recovery'].get('unrecovered', 0)} paths still use original programs.", "",
        "The complete OS recovery is unfinished. Rebuilt applications continue to",
        "depend on original guest components. Kernel interval/byte counts describe",
        "replacement slots, including padding and hardware instructions, and are",
        "not completion percentages. Detailed original SHA-256 values and raw X",
        "headers are in [inventory.json](inventory.json).", "",
        "| Original path | Bytes | Role | Recovery |",
        "| --- | ---: | --- | --- |"]
    for p in report["programs"]:
        recovery = p["recovery"]
        label = recovery["state"]
        if p["role"] == "kernel":
            label += f"; {recovery['declared_intervals']} declared intervals"
        lines.append(f"| `{p['path']}` | {p['bytes']:,} | {p['role']} | {label} |")
    lines += ["", "Boot regions are separate from the program-file count:", ""]
    for b in report["boot"]:
        lines.append(f"- {b['region']} at `{b['offset']}`, {b['bytes']} original bytes: {b['recovery']}.")
    lines += ["", "Identical original binaries occur at multiple paths:", ""]
    for group in report["duplicate_binaries"]:
        lines.append("- " + ", ".join(f"`{path}`" for path in group["paths"]) + ".")
    return "\n".join(lines)+"\n"


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--check", action="store_true", help="fail when committed inventory is stale")
    args = parser.parse_args()
    report = inventory()
    files = {ROOT/"recovery/inventory.json": json.dumps(report, indent=2)+"\n",
             ROOT/"recovery/INVENTORY.md": markdown(report)}
    for path, content in files.items():
        if args.check:
            require(path.is_file() and path.read_text() == content, "stale inventory: " + str(path))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
    print(json.dumps(report["counts"], ensure_ascii=False))


if __name__ == "__main__":
    main()
