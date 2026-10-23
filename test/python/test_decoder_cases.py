"""Package D task 2: authored decoder cases, fixture identity and controls.

The editor's decoders are run on these fixtures by the C tests and by the
CTest ``visual_reference_probe_decoder_cases`` (the probe CLI). Here: the
committed fixtures are exactly what the independent encoder produces, the
encoder itself follows the documented bit order, and each wrong model the
plan names (origin, row direction, dropped delta reuse, transparent skip)
fails its designated case.

    python -m unittest -v test.python.test_decoder_cases
"""

import json
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import decoder_cases as dc  # noqa: E402


def _record(case_id):
    doc = json.loads((dc.FIXTURE_DIR / "expected.json").read_text("utf-8"))
    return next(c for c in doc["cases"] if c["id"] == case_id)


def _frames(case_id):
    return dc.expected_frames(_record(case_id))


class Fixtures(unittest.TestCase):
    def test_committed_fixtures_are_the_generator_output(self):
        self.assertEqual(dc.check_fixtures(), [])

    def test_every_case_declares_its_features(self):
        for case in dc.build_cases():
            self.assertTrue(case["features"], case["id"])

    def test_contract_controls_name_real_cases(self):
        contract = json.loads(dc.CONTRACT_PATH.read_text("utf-8"))
        ids = {c["id"] for c in dc.build_cases()}
        for control, case_id in contract["controls"].items():
            self.assertIn(case_id, ids, control)
        for a in contract["assumptions"]:
            self.assertIn(a["status"], ("needs_original_confirmation", "confirmed",
                                        "refuted"))
            if a["status"] != "needs_original_confirmation":
                self.assertTrue(a.get("evidence"), a["id"])


class Encoder(unittest.TestCase):
    def test_bits_are_lsb_first(self):
        w = dc.BitWriter()
        w.put(0b101, 3)
        w.put(0x1F, 5)
        w.put(1, 1)
        self.assertEqual(w.to_bytes(), bytes([0b11111101, 0b00000001]))

    def test_width_codes(self):
        self.assertEqual(dc.WIDTH_TABLE[dc.width_code([0], False)], 0)
        self.assertEqual(dc.WIDTH_TABLE[dc.width_code([-300], True)], 10)
        self.assertEqual(dc.WIDTH_TABLE[dc.width_code([70002], True)], 20)
        self.assertEqual(dc.WIDTH_TABLE[dc.width_code([255], False)], 8)

    def test_cell_split_follows_the_direction_grid(self):
        self.assertEqual(dc.split_cells(0, 12, 0), [(0, 4), (4, 4), (8, 4)])
        self.assertEqual(dc.split_cells(1000, 4, 998), [(2, 2), (4, 2)])
        self.assertEqual(dc.split_cells(0, 5, 0), [(0, 5)])      # merge rule

    def test_dc6_rows_are_bottom_up_with_skips(self):
        self.assertEqual(dc.dc6_encode_rows([[1, 0, 2], [0, 0, 0]]),
                         bytes([0x80, 0x01, 1, 0x81, 0x01, 2, 0x80]))


class Controls(unittest.TestCase):
    """Each wrong decoder model must disagree with its designated case."""

    def assertDiffers(self, expected, mutated):
        self.assertTrue(dc.compare_frames(expected, mutated))

    def test_identity_agrees(self):
        f = _frames("dcc.sequence")
        self.assertEqual(dc.compare_frames(f, [dict(x) for x in f]), [])

    def test_wrong_origin(self):
        f = _frames("dc6.rle.basic")
        self.assertDiffers(f, [dict(x, origin_y=x["origin_y"] + x["height"] - 1) for x in f])

    def test_wrong_row_direction(self):
        f = _frames("dc6.rle.basic")
        flipped = []
        for x in f:
            w = x["width"]
            rows = [x["pixels"][i:i + w] for i in range(0, len(x["pixels"]), w)]
            flipped.append(dict(x, pixels=b"".join(reversed(rows))))
        self.assertDiffers(f, flipped)

    def test_dropped_delta_reuse(self):
        # frame 1 of direction 0 reuses frame 0's unchanged cells; a decoder
        # that drops equal cells leaves them transparent
        f = _frames("dcc.sequence")
        reuse = f[1]
        w = reuse["width"]
        px = bytearray(reuse["pixels"])
        for y in range(4):
            for x in range(4):            # cell (0,0) is unchanged in frame 1
                self.assertNotEqual(px[y * w + x], 0)
                px[y * w + x] = 0
        mutated = list(f)
        mutated[1] = dict(reuse, pixels=bytes(px))
        self.assertDiffers(f, mutated)

    def test_transparent_skip(self):
        # a decoder that writes the skip count (or any index) where the frame
        # is transparent
        f = _frames("dc6.rle.basic")
        x = f[0]
        self.assertIn(0, x["pixels"])
        mutated = [dict(x, pixels=bytes(v if v else 0x82 for v in x["pixels"]))] + f[1:]
        self.assertDiffers(f, mutated)


if __name__ == "__main__":
    unittest.main()
