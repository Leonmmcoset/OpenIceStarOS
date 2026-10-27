"""Compare X relocation with an independent second link at a new address."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from kernel_image import Fat16
from xformat import Elf32, XImage


class FormatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="icestar-xformat-")
        cls.path = Path(cls.temporary.name)
        (cls.path / "fixture.c").write_text("""
unsigned value = 0x12345678, zeros[19];
unsigned *pointer = &value;
unsigned helper(unsigned x) { return x + value; }
unsigned entry(void) { zeros[7] = helper(*pointer); return zeros[7]; }
""")
        subprocess.run(["gcc", "-m32", "-Os", "-ffreestanding", "-fno-pie", "-fno-pic",
                        "-fno-asynchronous-unwind-tables", "-c", "fixture.c", "-o", "fixture.o"],
                       cwd=cls.path, check=True)
        (cls.path / "fixture.ld").write_text("""
ENTRY(entry)
SECTIONS {
 .text (load_base + 64) : SUBALIGN(1) { *(.text*) }
 .rodata (load_base + 0x400) : SUBALIGN(1) { *(.rodata*) }
 .data (load_base + 0x800) : SUBALIGN(1) { *(.data*) }
 .bss (load_base + 0x1000) (NOLOAD) : SUBALIGN(1) { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
""")
        cls.elf = cls.link(0)
        cls.packed, cls.metadata = cls.elf.build_x()

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    @classmethod
    def link(cls, base):
        output = f"fixture-{base:x}.elf"
        subprocess.run(["ld", "-m", "elf_i386", "--emit-relocs", "-T", "fixture.ld",
                        f"--defsym=load_base={base}", "fixture.o", "-o", output],
                       cwd=cls.path, check=True, capture_output=True)
        return Elf32((cls.path / output).read_bytes())

    def test_independent_links(self):
        self.assertGreaterEqual(len(self.metadata["relocations"]), 4)
        for kind in (0x0a, 0x50):
            packed, metadata = self.elf.build_x(kind=kind)
            self.assertEqual(struct.unpack_from("<I", packed)[0], kind)
            self.assertEqual(metadata["kind"], kind)
            for base in (0x900000, 0x44a9010, 0x713001):
                memory = XImage(packed).load(base)
                linked = self.link(base)
                for section in linked.sections:
                    if section[2] & 2 and section[1] == 1:
                        offset = section[3] - base
                        self.assertEqual(memory[offset:offset + section[5]],
                                         linked.data[section[4]:section[4] + section[5]])
                self.assertEqual(struct.unpack_from("<I", memory, 0x1c)[0], base)
                self.assertFalse(any(memory[metadata["bss_start"]:]))

    def test_original_color_header(self):
        root = Path(__file__).resolve().parents[2]
        data = Fat16((root / "IceStarOS_x86_32M.vhd").read_bytes()).read_file(
            b"ICESTAR    ", b"COLOR   X  ")
        original = XImage(data)
        self.assertEqual(original.header[0], 0x0a)
        self.assertEqual((original.entry, original.bss_start, original.bss_size), (0x40, 0x3898, 16))
        self.assertEqual(len(original.relocations), 7)
        memory = original.load(0x12345000)
        self.assertEqual(struct.unpack_from("<I", memory, 0x51)[0], 0x12348890)
        self.assertEqual(struct.unpack_from("<I", memory, 0x105)[0], 0x1b0e0)
        self.assertFalse(any(memory[0x3898:]))

    def test_original_snake_header(self):
        root = Path(__file__).resolve().parents[2]
        data = Fat16((root / "IceStarOS_x86_32M.vhd").read_bytes()).read_file(
            b"ICESTAR    ", b"GAMES      ", b"SNAKE   X  ")
        original = XImage(data)
        self.assertEqual((original.entry, original.bss_start, original.bss_size), (0x5dc, 0x980, 0x380))
        self.assertEqual(len(original.relocations), 105)
        memory = original.load(0x12345000)
        self.assertEqual(struct.unpack_from("<I", memory, 0x9a)[0], 0x12345980)
        self.assertFalse(any(memory[0x980:]))

    def test_reject_invalid_inputs(self):
        for data in (b"", self.packed[:-1], b"\0" * 64):
            with self.assertRaises(ValueError):
                XImage(data)
        for offset, value in ((0, 0x51), (0x10, 0), (0x20, 0xffffffff), (0x24, 63)):
            data = bytearray(self.packed)
            struct.pack_into("<I", data, offset, value)
            with self.assertRaises(ValueError):
                XImage(data)
        with self.assertRaises(ValueError):
            self.elf.build_x(file_size=64)
        with self.assertRaises(ValueError):
            self.elf.build_x(kind=0x51)
        data = bytearray(self.packed)
        first = self.metadata["bss_start"]
        data[first + 4:first + 8] = data[first:first + 4]
        with self.assertRaises(ValueError):
            XImage(data)


if __name__ == "__main__":
    unittest.main()
