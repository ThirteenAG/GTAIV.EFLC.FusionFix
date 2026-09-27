"""Offline regression for the legacy counter-animation patch; no game required."""
import re
import unittest
from pathlib import Path

class CounterAnimationPatchTests(unittest.TestCase):
    def test_branch_is_removed_without_overwriting_next_instruction(self):
        source = (Path(__file__).resolve().parents[1] / 'source/episodiccontent.ixx').read_text(encoding='utf-8')
        # Read the actual patch offset from the production source.
        pattern = r'pattern = hook::pattern\("39 1D \? \? \? \? 75 2A 80 7F 28 00"\);\s*injector::MakeNOP\(pattern.get_first\((\d+)\), 2, true\);'
        match = re.search(pattern, source)
        self.assertIsNotNone(match)
        original = bytes.fromhex('39 1D 54 D7 DA 00 75 2A 80 7F 28 00')
        patched = bytearray(original)
        offset = int(match.group(1))
        patched[offset:offset+2] = b'\x90\x90'
        self.assertEqual(patched[:6], original[:6])
        self.assertEqual(patched[6:8], b'\x90\x90')
        self.assertEqual(patched[8:], bytes.fromhex('80 7F 28 00'))
        # Old offset corrupts the conditional jump and the next opcode.
        broken = bytearray(original)
        broken[7:9] = b'\x90\x90'
        self.assertEqual(broken[6:9], bytes.fromhex('75 90 90'))
        # JNE rel8 0x90 points backwards by 112 bytes from its end.
        self.assertEqual(8 + int.from_bytes(broken[7:8], 'little', signed=True), -104)

if __name__ == '__main__':
    unittest.main()
