"""Package D task 1: palette/tile-key/AnimData vectors and their controls.

The C tests (test_tile_properties, test_animdata_lookup, test_palette) run the
editor on these vectors; here the expectations themselves are checked against
the documented rules, and each named wrong model is shown to fail a vector.

    python -m unittest -v test.python.test_visual_values
"""

import json
import re
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import visual_values as vv  # noqa: E402


class TileKeys(unittest.TestCase):
    def test_every_main_and_sub_round_trips(self):
        for m in range(64):
            for s in range(256):
                p3 = (m & 0x0F) << 4
                p4 = (m >> 4) & 0x03
                self.assertEqual(vv.tile_key(0x80, s, p3, p4), (m, s))

    def test_documented_worked_example(self):
        # ds1-map-format.md: c2 00 50 00 -> main 5, sub 0
        self.assertEqual(vv.tile_key(0xC2, 0x00, 0x50, 0x00), (5, 0))

    def test_seven_bit_mask_control_fails(self):
        disagreements = 0
        for m in range(64):
            p3, p4 = (m & 0x0F) << 4, ((m >> 4) & 0x03) | 0x04   # bit 26 set
            if vv.tile_key(0x80, 1, p3, p4) != vv.tile_key(0x80, 1, p3, p4, mask=0x7F):
                disagreements += 1
        self.assertEqual(disagreements, 64)

    def test_flag_bits_do_not_move_the_key(self):
        for bit in range(32):
            byte, mask = divmod(bit, 8)
            mask = 1 << mask
            if byte == 1 or (byte == 2 and mask >= 0x10) or (byte == 3 and mask <= 2):
                continue
            props = [0x80, 9, 0x30, 0x01]
            props[byte] ^= mask
            self.assertEqual(vv.tile_key(*props), (0x13, 9), bit)

    def test_legacy_orientation_table_matches_the_reader(self):
        src = (REPO_ROOT / "src" / "core" / "ds1.c").read_text("utf-8", "replace")
        m = re.search(r"dir_lookup\[25\]\s*=\s*\{([^}]*)\}", src)
        self.assertIsNotNone(m)
        table = [int(x, 0) for x in m.group(1).replace("\n", " ").split(",") if x.strip()]
        self.assertEqual(table, vv.LEGACY_ORIENTATION)
        # a wrong-table control: the identity mapping disagrees
        self.assertNotEqual(list(range(25)), vv.LEGACY_ORIENTATION)


class AnimData(unittest.TestCase):
    def setUp(self):
        self.data = vv.build_animdata()

    def test_fixture_is_the_generator_output(self):
        self.assertEqual(vv.check_fixtures(), [])

    def test_layout_is_256_counts_and_160_byte_records(self):
        self.assertEqual(len(self.data), 256 * 4 + 160 * len(vv.ANIMDATA_RECORDS))

    def test_documented_hash(self):
        self.assertEqual(vv.animdata_hash("AMTNHTH"), 20)
        self.assertEqual(vv.animdata_hash("amtnhth"), 20)

    def test_records_collisions_case_and_misses(self):
        colliding = [r for r in vv.ANIMDATA_RECORDS if vv.animdata_hash(r[0]) == 20]
        self.assertGreaterEqual(len(colliding), 2)
        for name, fpd, speed, _ in vv.ANIMDATA_RECORDS:
            self.assertEqual(vv.animdata_lookup(self.data, name), (fpd, speed))
            self.assertEqual(vv.animdata_lookup(self.data, name.lower()), (fpd, speed))
        for name in vv.ANIMDATA_MISSES:
            self.assertIsNone(vv.animdata_lookup(self.data, name))
        empty = sum(1 for b in range(256)
                    if all(vv.animdata_hash(r[0]) != b for r in vv.ANIMDATA_RECORDS))
        self.assertGreater(empty, 200)

    def test_wrong_stride_control_fails(self):
        bad = [r for r in vv.ANIMDATA_RECORDS
               if vv.animdata_lookup(self.data, r[0], stride=156) != (r[1], r[2])]
        self.assertTrue(bad)

    def test_case_sensitive_hash_control_fails(self):
        self.assertIsNone(vv.animdata_lookup(self.data, "amtnhth", case_fold=False))

    def test_millisecond_policy_is_separate_from_the_game(self):
        self.assertEqual(vv.frame_delay_ms(80, 40), 128)
        self.assertEqual(vv.frame_delay_ms(256, 99), 40)
        self.assertEqual(vv.frame_delay_ms(512, 99), 20)
        self.assertEqual(vv.frame_delay_ms(0, 77), 77)
        self.assertEqual(vv.frame_delay_ms(1, 5), 10000)


def _all_test_text():
    files = list((REPO_ROOT / "test").glob("test_*.c"))
    files += list((REPO_ROOT / "test" / "python").glob("test_*.py"))
    return "".join(p.read_text("utf-8", "replace") for p in files)


class Contract(unittest.TestCase):
    def setUp(self):
        self.contract = vv.load_contract()

    def test_every_assumption_is_named_and_settled_only_with_evidence(self):
        assumptions = self.contract["assumptions"]
        self.assertTrue(assumptions)
        tests = _all_test_text()
        for a in assumptions:
            for key in ("id", "assumed", "status", "evidence_needed", "tests"):
                self.assertIn(key, a, a.get("id"))
            self.assertIn(a["status"], ("needs_original_confirmation", "confirmed",
                                        "refuted"), a["id"])
            if a["status"] != "needs_original_confirmation":
                # a settled assumption names its original-game evidence
                self.assertTrue(a.get("evidence"), a["id"])
                for e in a["evidence"]:
                    self.assertTrue(e.get("ref") and e.get("observation"), a["id"])
            if a["status"] == "refuted":
                self.assertTrue(a.get("resolution"), a["id"])
            for t in a["tests"]:
                self.assertIn(t, tests, "%s names a missing test %s" % (a["id"], t))

    def test_cases_map_to_tests(self):
        tests = _all_test_text()
        for case in self.contract["cases"]:
            for t in case["tests"]:
                self.assertIn(t, tests, case["id"])


if __name__ == "__main__":
    unittest.main()
