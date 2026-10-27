"""Install recovered X applications in their existing FAT16 file extents."""

import argparse
import hashlib
import json
from pathlib import Path

from kernel_image import Fat16, require, write_new
from xformat import XImage


def installed(source, manifest, build):
    result = source
    paths = set()
    for app in json.loads(Path(manifest).read_text()):
        path = tuple(name.encode("ascii") for name in app["path"])
        require(path not in paths, "duplicate application path")
        paths.add(path)
        disk = Fat16(result)
        original = disk.read_file(*path)
        require(hashlib.sha256(original).hexdigest() == app["original_sha256"],
                "original application SHA-256 mismatch: " + app["name"])
        replacement = (Path(build) / app["binary"]).read_bytes()
        before, after = XImage(original), XImage(replacement)
        require((before.bss_size, before.header[3]) == (after.bss_size, after.header[3]),
                "application memory/stack contract changed: " + app["name"])
        result = disk.replace_file(replacement, *path)
        require(Fat16(result).read_file(*path) == replacement, "installed application mismatch")
    return bytes(result)


def verify(source, actual, manifest, build):
    require(actual == installed(source, manifest, build),
            "disk differs outside the declared compiled application bytes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("install", "verify"))
    parser.add_argument("source", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.source.read_bytes()
    if args.command == "install":
        write_new(args.output, installed(source, args.manifest, args.build), args.source)
        print("Recovered applications installed without changing FAT, directories, or file slack")
    else:
        verify(source, args.output.read_bytes(), args.manifest, args.build)
        print("Entire disk verified: only declared application file extents changed")


if __name__ == "__main__":
    main()
