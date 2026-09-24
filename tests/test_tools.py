import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from interleave_i960 import interleave_words
from scan_i960_refs import scan

class ToolTests(unittest.TestCase):
    def test_interleave_words(self):
        self.assertEqual(
            interleave_words(bytes.fromhex("11 22 55 66"), bytes.fromhex("33 44 77 88")),
            bytes.fromhex("11 22 33 44 55 66 77 88"),
        )

    def test_scan_absolute_reference(self):
        target = 0x00500000
        instruction = (0x90 << 24) | (12 << 10)
        data = struct.pack("<II", instruction, target)
        refs = scan(data, {target: "RAM"})
        self.assertEqual(len(refs), 1)
        self.assertEqual(refs[0]["target"], "RAM")

if __name__ == "__main__":
    unittest.main()
