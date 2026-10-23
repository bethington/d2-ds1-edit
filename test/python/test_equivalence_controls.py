"""Wrong-model controls against the ORIGINAL-game palette capture.

The tracked capture (test/equivalence/fixtures/palette-nearest-capture.json)
holds D2CMP.dll 1.13c answers recorded from a live member. Each control here is
a deliberately wrong candidate; a control that the comparator does not detect
is itself a failure ("ineffective control").

    python -m unittest -v test.python.test_equivalence_controls
"""

import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import compare as CMP  # noqa: E402
from scripts.equivalence import contracts as C  # noqa: E402

EQ = REPO_ROOT / "test" / "equivalence"


def model(pal, rgb, swap=False, last_tie=False):
    r, g, b = rgb
    if swap:
        r, b = b, r
    best, bi = None, 0
    for i in range(1, 256):
        d = sum((pal[4 * i + c] - v) ** 2 for c, v in enumerate((r, g, b)))
        if best is None or d < best or (last_tie and d == best):
            best, bi = d, i
    return bi


class ControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = C.load_contract(EQ / "contracts" / "palette-nearest-113c.json")
        cls.baseline = C.load_baseline(EQ / "fixtures" / "palette-nearest-baseline.json")
        cls.capture = C.load_capture(EQ / "fixtures" / "palette-nearest-capture.json")
        C.validate_capture(cls.capture, cls.contract, cls.baseline)
        inputs = C.load_json_object(EQ / "fixtures" / "palette-nearest-inputs.json")
        cls.pals = {k: bytes.fromhex(v) for k, v in inputs["palettes"].items()}

    def actual(self, fn):
        prov = {"source_state": {"control": True}, "executable": {"sha256": "0" * 64},
                "build": {"config": "control"}, "packages": []}
        return {"lane": "portable", "platform": "control", "provenance": prov,
                "vectors": [{"id": v["id"],
                             "output": {"index": fn(v["input"]), "writes": []}}
                            for v in self.capture["vectors"]]}

    def control(self, name, fn):
        case = CMP.compare_case(self.contract, self.capture, self.actual(fn))
        if case["status"] != "mismatch":
            self.fail("ineffective control %s: status %s" % (name, case["status"]))
        return case

    def test_capture_is_original_game_evidence(self):
        self.assertEqual(self.capture["adapter"]["name"], "ds1edit.fleet-adapter")
        self.assertTrue(self.capture["original"]["live"]["text_section_verified"])
        self.assertEqual(len(self.capture["vectors"]), 336)

    def test_reference_free_model_agrees(self):
        case = CMP.compare_case(self.contract, self.capture, self.actual(
            lambda i: model(self.pals[i["palette"]], i["rgb"])))
        self.assertEqual(case["status"], "pass", case["failures"][:3])

    def test_constant_control_diverges(self):
        case = self.control("constant", lambda i: 3)
        self.assertGreater(case["coverage"]["vectors_mismatched"], 250)

    def test_channel_swap_control_diverges(self):
        case = self.control("channel-swap",
                            lambda i: model(self.pals[i["palette"]], i["rgb"], swap=True))
        self.assertGreater(case["coverage"]["vectors_mismatched"], 200)
        self.assertEqual(case["failures"][0]["vector"], "main-exact-001")

    def test_last_tie_control_diverges_only_on_ties(self):
        case = self.control("last-tie", lambda i: model(self.pals[i["palette"]], i["rgb"],
                                                        last_tie=True))
        self.assertTrue(all(f["vector"].startswith("ties-") for f in case["failures"]))

    def test_missing_offset_normalization_diverges(self):
        self.control("relative-index", lambda i: model(self.pals[i["palette"]], i["rgb"]) - 1)


if __name__ == "__main__":
    unittest.main()
