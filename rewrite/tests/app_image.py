"""Verify exact FAT16 application replacement and preservation of all other bytes."""

from pathlib import Path
import json
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from app_image import installed, verify
from kernel_image import Fat16

SNAKE = (b"ICESTAR    ", b"GAMES      ", b"SNAKE   X  ")


class ApplicationImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "build/OpenIceStarOS_c_init.vhd").read_bytes()
        cls.manifest = ROOT / "apps/images.json"
        cls.build = ROOT / "build"
        cls.result = installed(cls.source, cls.manifest, cls.build)

    def test_exact_extent_changes(self):
        restored = self.result
        for app in json.loads(self.manifest.read_text()):
            path = tuple(name.encode("ascii") for name in app["path"])
            expected = (self.build / app["binary"]).read_bytes()
            self.assertEqual(Fat16(self.result).read_file(*path), expected)
            original = Fat16(self.source).read_file(*path)
            restored = Fat16(restored).replace_file(original, *path)
        verify(self.source, self.result, self.manifest, self.build)
        self.assertEqual(restored, self.source)
        self.assertNotEqual(self.result, self.source)

    def test_snake_only_checkpoint(self):
        manifest = ROOT / "apps/snake_image.json"
        result = installed(self.source, manifest, self.build)
        verify(self.source, result, manifest, self.build)
        self.assertEqual(Fat16(result).read_file(*SNAKE), (self.build / "snake.x").read_bytes())
        restored = Fat16(result).replace_file(Fat16(self.source).read_file(*SNAKE), *SNAKE)
        self.assertEqual(restored, self.source)
        self.assertNotEqual(result, self.result)

    def test_multiple_cluster_file(self):
        path = (b"ICESTAR    ", b"CMD     X  ")
        disk = Fat16(self.source)
        extents = disk.file_extents(*path)
        self.assertGreater(len(extents), 1)
        original = disk.read_file(*path)
        self.assertEqual(sum(size for _, size in extents), len(original))
        replacement = bytes((i * 37 + 5) % 256 for i in range(len(original)))
        changed = Fat16(disk.replace_file(replacement, *path))
        self.assertEqual(changed.read_file(*path), replacement)
        self.assertEqual(changed.replace_file(original, *path), self.source)

    def test_reject_invalid_installation(self):
        with self.assertRaises(ValueError):
            installed(self.result, self.manifest, self.build)
        for data in (self.result[:-1], b"x" + self.result[1:]):
            with self.assertRaises(ValueError):
                verify(self.source, data, self.manifest, self.build)
        disk = Fat16(self.source)
        for replacement in (b"", b"x", disk.read_file(*SNAKE) + b"x"):
            with self.assertRaises(ValueError):
                disk.replace_file(replacement, *SNAKE)
        for path in ((), (b"bad",), (b"MISSING X  ",)):
            with self.assertRaises(ValueError):
                disk.file_extents(*path)

    def test_reject_changed_application_type(self):
        replacement = bytearray((self.build / "snake.x").read_bytes())
        replacement[0] = 0x0a
        manifest = json.loads(self.manifest.read_text())
        with tempfile.TemporaryDirectory(prefix="openicestaros-app-type-") as temporary:
            build = Path(temporary)
            (build / manifest[0]["binary"]).write_bytes(replacement)
            with self.assertRaisesRegex(ValueError, "application type/memory/stack contract changed"):
                installed(self.source, self.manifest, build)


if __name__ == "__main__":
    unittest.main()
