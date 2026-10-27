"""Install fixed-address C routines without reallocating the FAT16 kernel."""

import hashlib
import json
from pathlib import Path
import struct
import sys

BASE = 0x800000
ORIGINAL_SHA256 = "9edc55ea50b889f86ec7b00a1305eada39c979221113f6d86e13fd2b31f99ae3"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def word(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def dword(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


class Fat16:
    def __init__(self, data):
        self.data = data
        require(data[510:512] == b"\x55\xaa", "missing MBR signature")
        partitions = [446 + 16 * i for i in range(4) if data[450 + 16 * i]]
        require(len(partitions) == 1, "expected one partition")
        entry = partitions[0]
        self.partition_lba = dword(data, entry + 8)
        require(self.partition_lba == 17, "unexpected partition start")
        bpb = self.partition_lba * 512
        require(word(data, bpb + 11) == 512, "expected 512-byte sectors")
        spc = data[bpb + 13]
        require(spc > 0 and spc & (spc - 1) == 0, "invalid cluster size")
        self.cluster_bytes = spc * 512
        fats = data[bpb + 16]
        fat_bytes = word(data, bpb + 22) * 512
        require(fats > 0 and fat_bytes > 0, "missing FAT16 tables")
        fat_start = bpb + word(data, bpb + 14) * 512
        self.fat = data[fat_start:fat_start + fat_bytes]
        require(len(self.fat) == fat_bytes, "truncated FAT")
        # The source disk differs in unused entries of its secondary FAT.
        # Match INIT's first-FAT traversal and verify only the chains we use.
        self.fat_copies = [data[fat_start + i * fat_bytes:fat_start + (i + 1) * fat_bytes]
                           for i in range(1, fats)]
        root_start = fat_start + fats * fat_bytes
        root_bytes = word(data, bpb + 17) * 32
        require(root_bytes > 0, "missing FAT16 root directory")
        self.root = data[root_start:root_start + root_bytes]
        self.data_start = root_start + (root_bytes + 511) // 512 * 512
        total_sectors = word(data, bpb + 19) or dword(data, bpb + 32)
        require(total_sectors <= dword(data, entry + 12), "partition length mismatch")
        self.end = bpb + total_sectors * 512
        require(self.end <= len(data), "truncated partition")

    def chain(self, cluster):
        seen = set()
        while cluster < 0xfff8:
            require(2 <= cluster < 0xfff0 and cluster not in seen,
                    "invalid or cyclic FAT16 chain")
            require(cluster * 2 + 2 <= len(self.fat), "cluster outside FAT")
            require(all(word(fat, cluster * 2) == word(self.fat, cluster * 2)
                        for fat in self.fat_copies), "FAT copies disagree on kernel path")
            seen.add(cluster)
            offset = self.data_start + (cluster - 2) * self.cluster_bytes
            require(offset + self.cluster_bytes <= self.end, "cluster outside partition")
            yield offset
            cluster = word(self.fat, cluster * 2)

    @staticmethod
    def find(directory, name, is_directory):
        for offset in range(0, len(directory), 32):
            entry = directory[offset:offset + 32]
            if entry[0] == 0:
                break
            if entry[0] == 0xe5 or entry[11] & 8:
                continue
            if entry[:11] == name and bool(entry[11] & 16) == is_directory:
                return entry
        raise ValueError("directory entry not found: " + name.decode("ascii"))

    def kernel_extents(self):
        extents = self.file_extents(b"ICESTAR    ", b"ICESTAR X  ")
        require(bool(extents), "empty kernel")
        return extents

    def read_kernel(self):
        return b"".join(self.data[o:o + n] for o, n in self.kernel_extents())

    def file_extents(self, *short_names):
        """Locate file bytes without including final-cluster slack or directory data."""
        require(bool(short_names) and all(len(name) == 11 for name in short_names),
                "expected a nonempty FAT short-name path")
        contents = self.root
        for index, name in enumerate(short_names):
            directory = index < len(short_names) - 1
            entry = self.find(contents, name, directory)
            if directory:
                contents = b"".join(self.data[o:o + self.cluster_bytes]
                                    for o in self.chain(word(entry, 26)))
            else:
                remaining = dword(entry, 28)
                extents = []
                if remaining:
                    for offset in self.chain(word(entry, 26)):
                        length = min(remaining, self.cluster_bytes)
                        if length:
                            extents.append((offset, length))
                            remaining -= length
                require(remaining == 0, "truncated file")
                return extents

    def read_file(self, *short_names):
        """Read an explicit path of FAT 8.3 names, each exactly 11 bytes."""
        return b"".join(self.data[o:o + n] for o, n in self.file_extents(*short_names))

    def replace_file(self, replacement, *short_names):
        extents = self.file_extents(*short_names)
        require(sum(n for _, n in extents) == len(replacement), "file size must stay unchanged")
        result = bytearray(self.data)
        cursor = 0
        for offset, length in extents:
            result[offset:offset + length] = replacement[cursor:cursor + length]
            cursor += length
        return result

    def replace_kernel(self, kernel):
        return self.replace_file(kernel, b"ICESTAR    ", b"ICESTAR X  ")


def patched_kernel(original, manifest_path, build_path):
    require(hashlib.sha256(original).hexdigest() == ORIGINAL_SHA256,
            "original kernel SHA-256 mismatch")
    result = bytearray(original)
    previous_end = BASE
    for patch in json.loads(Path(manifest_path).read_text()):
        start, end = int(patch["start"], 0), int(patch["end"], 0)
        require(BASE + 64 <= start < end <= BASE + len(original), "patch outside kernel code")
        require(start >= previous_end, "overlapping or unordered patches")
        binary = (Path(build_path) / patch["binary"]).read_bytes()
        require(0 < len(binary) <= end - start, "empty or oversized patch")
        if patch.get("exact"):
            require(binary == original[start - BASE:end - BASE],
                    "hardware primitive must retain exact original instructions: " + patch["name"])
        result[start - BASE:end - BASE] = binary + b"\x90" * (end - start - len(binary))
        previous_end = end
        print(f'{patch["name"]}: {len(binary)}/{end - start} bytes at {start:#x}')
    return bytes(result)


def write_new(path, data, source):
    require(Path(path).resolve() != Path(source).resolve(), "refusing to overwrite source")
    Path(path).write_bytes(data)


def verify(original_path, image_path, kernel_path, manifest_path, build_path):
    original = Path(original_path).read_bytes()
    kernel = Path(kernel_path).read_bytes()
    expected_kernel = patched_kernel(Fat16(original).read_kernel(), manifest_path, build_path)
    require(kernel == expected_kernel, "patched kernel differs from linked C patches")
    expected = Fat16(original).replace_kernel(kernel)
    for offset, length, name in ((0, 512, "mbr.bin"),
                                 (512, 1536, "menu_hybrid.bin"),
                                 (2560, 4096, "stage3_c_init.bin"),
                                 (6656, 2048, "init_c.bin")):
        binary = (Path(build_path) / name).read_bytes()
        require(len(binary) == length, "unexpected boot stage length")
        expected[offset:offset + length] = binary
    actual = Path(image_path).read_bytes()
    require(len(actual) == len(expected), "disk image length changed")
    if actual != expected:
        offset = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
        raise ValueError(f"unexpected disk modification at {offset:#x}")
    require(Fat16(actual).read_kernel() == kernel, "installed kernel mismatch")
    print("entire disk verified: only compiled boot stages and declared kernel slots changed")


def main():
    command, *args = sys.argv[1:]
    if command == "patch" and len(args) == 4:
        source, manifest, build, output = args
        kernel = patched_kernel(Fat16(Path(source).read_bytes()).read_kernel(), manifest, build)
        write_new(output, kernel, source)
    elif command == "install" and len(args) == 3:
        source, kernel, output = args
        image = Fat16(Path(source).read_bytes()).replace_kernel(Path(kernel).read_bytes())
        write_new(output, image, source)
    elif command == "verify" and len(args) == 5:
        verify(*args)
    else:
        raise ValueError("usage: kernel_image.py patch|install|verify ARGS")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, struct.error) as error:
        sys.exit(str(error))
