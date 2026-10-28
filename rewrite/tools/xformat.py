"""Build OpenIceStarOS flat X applications from linked ELF32 and retained relocations.

Loader evidence: kernel 0x80229a..0x8024aa. Types 0x0a and 0x50 use the same
relocation and clear algorithm, with different task startup selectors.
Each relocation names a DWORD
to which the load base is added. The table is then erased together with BSS
and stack storage. The X entry remains an offset; header +0x1c holds the base.
"""

import argparse
import json
from pathlib import Path
import struct


def require(condition, message):
    if not condition:
        raise ValueError(message)


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


class XImage:
    def __init__(self, data):
        require(len(data) >= 64, "truncated X header")
        self.data = bytes(data)
        self.header = struct.unpack_from("<16I", data)
        kind, size, _, pages, entry = self.header[:5]
        count, offset = self.header[8:10]
        require(kind in (0x0a, 0x50), "expected application type 0x0a or 0x50")
        require(size == len(data), "X file size mismatch")
        require(64 <= entry < offset <= size, "invalid X entry or relocation offset")
        require(count <= (size - offset) // 4, "truncated X relocation table")
        require(pages > 0, "missing X stack")
        self.relocations = list(struct.unpack_from(f"<{count}I", data, offset))
        require(len(set(self.relocations)) == count, "duplicate X relocation")
        require(all(64 <= r and r + 4 <= offset for r in self.relocations),
                "X relocation outside initialized payload")
        self.entry, self.bss_start, self.bss_size = entry, offset, self.header[11]
        self.memory_size = size + self.bss_size + pages * 4096
        require(self.memory_size <= 0x10000000, "unreasonable X allocation")

    def load(self, base):
        """Model the observed loader; tests also run actual guest-loaded code."""
        require(0 <= base <= 0xffffffff - self.memory_size, "invalid X load base")
        memory = bytearray(self.memory_size)
        memory[:len(self.data)] = self.data
        for offset in self.relocations:
            struct.pack_into("<I", memory, offset, (u32(memory, offset) + base) & 0xffffffff)
        struct.pack_into("<I", memory, 0x1c, base)
        end = self.bss_start + len(self.relocations) * 4 + self.bss_size + self.header[3] * 4096
        require(end <= len(memory), "X clear range exceeds allocation")
        memory[self.bss_start:end] = bytes(end - self.bss_start)
        return memory


class Elf32:
    def __init__(self, data):
        self.data = data
        require(len(data) >= 52 and data[:7] == b"\x7fELF\x01\x01\x01", "expected little-endian ELF32")
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
        require(header[1:3] == (2, 3), "expected linked i386 executable")
        self.entry = header[4]
        offset, stride, count, names_index = header[6], header[11], header[12], header[13]
        require(stride == 40 and count and offset + count * stride <= len(data), "invalid ELF section table")
        require(names_index < count, "invalid section-name table")
        self.sections = [struct.unpack_from("<10I", data, offset + i * stride) for i in range(count)]
        names = self.section_data(names_index)
        self.names = [self.string(names, section[0]) for section in self.sections]

    @staticmethod
    def string(data, offset):
        require(offset < len(data), "string outside ELF string table")
        end = data.find(b"\0", offset)
        require(end >= 0, "unterminated ELF string")
        return data[offset:end].decode("ascii")

    def section_data(self, index):
        require(index < len(self.sections), "invalid ELF section index")
        section = self.sections[index]
        offset, size = section[4:6]
        require(section[1] != 8 and offset + size <= len(self.data), "invalid ELF section contents")
        return self.data[offset:offset + size]

    def symbol_table(self, index):
        section = self.sections[index]
        require(section[1] == 2 and section[9] == 16, "invalid ELF symbol table")
        content = self.section_data(index)
        require(len(content) % 16 == 0, "truncated ELF symbols")
        return [struct.unpack_from("<IIIBBH", content, i) for i in range(0, len(content), 16)]

    def build_x(self, stack_pages=16, file_size=None, kind=0x50):
        require(kind in (0x0a, 0x50), "unsupported X application type")
        allocated = [(i, s) for i, s in enumerate(self.sections) if s[2] & 2 and s[5]]
        bss = [s for _, s in allocated if s[1] == 8]
        require(len(bss) == 1, "expected one nonempty BSS section")
        bss_start, bss_size = bss[0][3], bss[0][5]
        require(64 < bss_start <= 0x1000000, "invalid BSS address")
        payload = bytearray(bss_start)
        occupied = []
        for index, section in allocated:
            if section[1] == 8:
                continue
            require(section[1] == 1, "unsupported allocated ELF section")
            start, end = section[3], section[3] + section[5]
            require(64 <= start < end <= bss_start, "ELF payload overlaps header or BSS")
            require(all(end <= a or start >= b for a, b in occupied), "overlapping ELF payload sections")
            occupied.append((start, end))
            content = self.section_data(index)
            payload[start:end] = content
        require(any(s[2] & 4 and s[3] <= self.entry < s[3] + s[5] for _, s in allocated),
                "entry outside executable ELF payload")
        relocations, symbols = [], {}
        for index, section in enumerate(self.sections):
            if section[1] == 2:
                strings = self.section_data(section[6])
                for symbol in self.symbol_table(index):
                    name, value, size, info, _, target = symbol
                    if name and info & 0xf in (1, 2) and 0 < target < len(self.sections):
                        symbols[self.string(strings, name)] = {"address": value, "size": size}
            if section[1] not in (4, 9):
                continue
            require(section[7] < len(self.sections), "invalid relocation target section")
            target = self.sections[section[7]]
            if not target[2] & 2:
                continue
            require(section[1] == 9 and section[9] == 8, "expected ELF REL relocations")
            table = self.symbol_table(section[6])
            content = self.section_data(index)
            require(len(content) % 8 == 0, "truncated ELF relocations")
            for cursor in range(0, len(content), 8):
                address, info = struct.unpack_from("<II", content, cursor)
                relocation_kind, symbol_index = info & 255, info >> 8
                require(symbol_index < len(table), "invalid ELF relocation symbol")
                symbol = table[symbol_index]
                require(0 < symbol[5] < len(self.sections) and self.sections[symbol[5]][2] & 2,
                        "ELF relocation references an external or absolute symbol")
                require(target[3] <= address and address + 4 <= target[3] + target[5],
                        "ELF relocation outside target section")
                require(relocation_kind in (1, 2), f"unsupported i386 relocation type {relocation_kind}")
                if relocation_kind == 1:  # R_386_32; PC32 already remains correct after moving.
                    require(address + 4 <= bss_start, "ELF absolute relocation is not initialized")
                    relocations.append(address)
        relocations.sort()
        require(len(set(relocations)) == len(relocations), "duplicate ELF absolute relocation")
        minimum = bss_start + 4 * len(relocations)
        size = minimum if file_size is None else file_size
        require(minimum <= size <= 0x1000000, "requested X file size cannot hold payload and relocations")
        require(0 < stack_pages <= 4096, "invalid stack page count")
        payload += struct.pack(f"<{len(relocations)}I", *relocations)
        payload += bytes(size - len(payload))
        struct.pack_into("<16I", payload, 0, kind, size, 0, stack_pages,
                         self.entry, 0, 0, 0, len(relocations), bss_start, 0, bss_size, 0, 0, 0, 0)
        XImage(payload)
        return bytes(payload), {"kind": kind, "entry": self.entry, "file_size": size, "bss_start": bss_start,
                                "bss_size": bss_size, "stack_pages": stack_pages,
                                "relocations": relocations, "symbols": symbols}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--metadata", type=Path)
    parser.add_argument("--file-size", type=lambda value: int(value, 0))
    parser.add_argument("--stack-pages", type=int, default=16)
    parser.add_argument("--kind", type=lambda value: int(value, 0), default=0x50,
                        choices=(0x0a, 0x50), help="preserve the original task startup type")
    args = parser.parse_args()
    data, metadata = Elf32(args.elf.read_bytes()).build_x(args.stack_pages, args.file_size, args.kind)
    args.output.write_bytes(data)
    if args.metadata:
        args.metadata.write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"{args.output}: {len(data)} bytes, {len(metadata['relocations'])} relocations, "
          f"entry {metadata['entry']:#x}, BSS {metadata['bss_start']:#x}+{metadata['bss_size']:#x}")


if __name__ == "__main__":
    main()
