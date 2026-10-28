"""QEMU's explicit pre-execution TB cancellation must not mimic a repeat."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from entry_smoke import block_events


class TraceEvents(unittest.TestCase):
    def test_observed_mov_cancellation(self):
        text = ("Trace 0: 0x7f2fa4259500 [00000000/0080005c/004002b0/ff000000]\n"
                "Stopped execution of TB chain before 0x7f2fa4259500 [0080005c]\n"
                "Trace 0: 0x7f2fa4259500 [00000000/0080005c/004002b0/ff000000]\n"
                "Trace 0: 0x7f2fa4259600 [00000000/00800061/004002b0/ff000000]\n")
        self.assertEqual(block_events(text),([0x80005c,0x800061],[0x80005c]))

    def test_real_repeat_is_retained(self):
        line = "Trace 0: 0x1234 [00000000/0080005c/0/0]\n"
        self.assertEqual(block_events(line+line),([0x80005c,0x80005c],[]))

    def test_host_and_guest_must_both_match(self):
        text = ("Trace 0: 0x1234 [00000000/0080005c/0/0]\n"
                "Stopped execution of TB chain before 0x5678 [0080005c]\n"
                "Stopped execution of TB chain before 0x1234 [00800061]\n")
        self.assertEqual(block_events(text),([0x80005c],[]))

    def test_cancellation_does_not_reorder_events(self):
        text = ("Trace 0: 0x1234 [00000000/00800061/0/0]\n"
                "Trace 0: 0x5678 [00000000/0080005c/0/0]\n")
        self.assertEqual(block_events(text),([0x800061,0x80005c],[]))


if __name__ == "__main__":
    unittest.main()
