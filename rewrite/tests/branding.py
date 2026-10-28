"""Check branding safety against real X relocation/format and FAT contracts."""
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import branding
from kernel_image import Fat16
from xformat import XImage


class BrandingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT.parent / "IceStarOS_x86_32M.vhd").read_bytes()
        cls.result = branding.installed(cls.source)
        cls.items = json.loads(branding.MANIFEST.read_text())

    def test_restore_entire_original_disk(self):
        restored = self.result
        for item in self.items:
            path = tuple(n.encode("ascii") for n in item["short_names"])
            original = Fat16(self.source).read_file(*path)
            replacement = Fat16(self.result).read_file(*path)
            self.assertEqual(len(original), len(replacement))
            if item["path"].endswith(".X"):
                self.assertEqual(original[:64], replacement[:64])
            restored = Fat16(restored).replace_file(original, *path)
        self.assertEqual(restored, self.source)

    def test_cmd_runtime_format_and_relocations(self):
        for item in self.items:
            if not item["path"].endswith("CMD.X"):
                continue
            path = tuple(n.encode("ascii") for n in item["short_names"])
            old = XImage(Fat16(self.source).read_file(*path))
            new = XImage(Fat16(self.result).read_file(*path))
            self.assertEqual(old.relocations, new.relocations)
            self.assertIn(0x523, new.relocations)
            self.assertIn(0x130b, new.relocations)
            for base in (0x1000000, 0x7340000):
                memory = new.load(base)
                pointer = struct.unpack_from("<I", memory, 0x523)[0] - base
                fmt = bytes(memory[pointer:]).split(b"\0")[0]
                self.assertEqual(fmt % (0, 1, 30), b"OpenIceStarOS [0.1.30]")

    def test_kernel_title_reference_and_boundaries(self):
        kernel = Fat16(self.result).read_kernel()
        pointer = struct.unpack_from("<I", kernel, 0x137fe)[0] - 0x800000
        self.assertEqual(kernel[pointer:].split(b"\0")[0], b"OpenIceStarOS")
        self.assertEqual(kernel[0x3ccf6:0x3cd08], b"C:\\ICESTAR\\GB.BIN\0")
        self.assertEqual(kernel[0x3cdcf:0x3cde5], b"Register Information:\0")

    def test_reject_wrong_original_and_double_install(self):
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            branding.installed(self.result)
        item = self.items[0]
        path = tuple(n.encode("ascii") for n in item["short_names"])
        disk = Fat16(self.source)
        data = bytearray(disk.read_file(*path))
        data[100] ^= 1
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            branding.installed(disk.replace_file(data, *path))

    def test_live_production_scan(self):
        image = (ROOT / "build/OpenIceStarOS_c_apps.vhd").read_bytes()
        findings = branding.scan(image)
        self.assertTrue(findings)
        self.assertTrue(all(f["classification"] == "compatibility-path" for f in findings))

    def test_whole_boot_verification_rejects_undeclared_bytes(self):
        image = bytearray((ROOT / "build/OpenIceStarOS_c_fat16.vhd").read_bytes())
        branding.verify_boot(self.source, image, ROOT / "build", stage3=True)
        image[0x3000] ^= 1
        with self.assertRaisesRegex(ValueError, "outside declared"):
            branding.verify_boot(self.source, image, ROOT / "build", stage3=True)


if __name__ == "__main__":
    unittest.main()
