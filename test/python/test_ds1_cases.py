"""
Tests for the authored DS1 fixture generator (test/fixtures/make_ds1_fixtures.py)
and the snapshot canonicalization in scripts/equivalence/ds1_cases.py, plus an
integration lane that runs the editor's real reader and writer through
test_ds1_reference_probe on every generated fixture.

The integration lane needs the probe executable:
    DS1EDIT_PROBE_EXE=<path to test_ds1_reference_probe>
If it is not set the lane is skipped -- unless DS1EDIT_REQUIRE_PROBE=1, in which
case a missing probe is a failure (CTest's ds1_cases target sets both).
No game install, MPQ or original map is read; every input is generated into a
temporary directory whose path contains spaces.

Run from the repository root:
    python -m unittest -v test.python.test_ds1_cases
"""

import copy
import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import ds1_cases  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "make_ds1_fixtures", REPO_ROOT / "test" / "fixtures" / "make_ds1_fixtures.py")
fixtures = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fixtures)


class GeneratorTests(unittest.TestCase):
    def test_every_case_layout_matches_the_size_formula(self):
        for case in fixtures.build_cases():
            with self.subTest(case=case["id"]):
                data, layout = fixtures.encode(case["spec"])
                fixtures.check_layout(case["spec"], data, layout)

    def test_layout_check_catches_a_dropped_field(self):
        spec = fixtures.default_spec(18)
        data, layout = fixtures.encode(spec)
        bad = dict(layout, end=layout["end"] - 4)
        with self.assertRaises(AssertionError):
            fixtures.check_layout(spec, data[:-4], bad)

    def test_matrix_covers_every_vanilla_and_threshold_version(self):
        ids = {c["spec"]["version"] for c in fixtures.build_cases()}
        for v in (2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18):
            self.assertIn(v, ids)

    def test_v18_header_bytes(self):
        data, _ = fixtures.encode(fixtures.default_spec(18))
        # version 18, width-1 = 2, height-1 = 1, act-1 = 1, tagType 2
        self.assertEqual(data[:20], bytes([18, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0,
                                           1, 0, 0, 0, 2, 0, 0, 0]))

    def test_legacy_versions_drop_the_fields_they_lack(self):
        snap = fixtures.expected_snapshot(fixtures.default_spec(5))
        self.assertEqual(snap["act"], 1)
        self.assertEqual(snap["tag_type"], 0)
        self.assertEqual(snap["objects"][0]["flags"], 0)
        self.assertEqual(snap["layers"], {"walls": 2, "floors": 1, "shadows": 1, "tags": 0})
        snap = fixtures.expected_snapshot(fixtures.default_spec(3))
        self.assertEqual(snap["layers"], {"walls": 1, "floors": 1, "shadows": 1, "tags": 1})

    def test_generate_writes_manifest_and_files(self):
        with tempfile.TemporaryDirectory(prefix="ds1 cases ") as tmp:
            manifest = fixtures.generate(Path(tmp))
            self.assertEqual(manifest["schema"], fixtures.MANIFEST_SCHEMA)
            for case in manifest["cases"]:
                path = Path(tmp) / case["file"]
                self.assertTrue(path.exists())
                self.assertEqual(path.stat().st_size, case["bytes"])


class CanonicalTests(unittest.TestCase):
    def setUp(self):
        self.snap = fixtures.expected_snapshot(fixtures.default_spec(18))

    def test_no_policy_is_identity(self):
        self.assertEqual(ds1_cases.canonical_map(self.snap, {}), self.snap)

    def test_canonical_does_not_mutate_its_input(self):
        before = copy.deepcopy(self.snap)
        ds1_cases.canonical_map(self.snap, {"normalizations": ["writer_v18_upgrade"]})
        self.assertEqual(before, self.snap)

    def test_unknown_normalization_is_rejected(self):
        with self.assertRaises(ValueError):
            ds1_cases.canonical_map(self.snap, {"normalizations": ["ignore_everything"]})

    def test_wrong_schema_is_rejected(self):
        with self.assertRaises(ValueError):
            ds1_cases.canonical_map(dict(self.snap, schema="other/1"), {})

    def test_altered_tile_byte_is_a_difference(self):
        other = copy.deepcopy(self.snap)
        other["cells"][4]["floors"][1][2] ^= 1
        result = ds1_cases.compare_maps(self.snap, other)
        self.assertEqual(result["status"], "fail")
        self.assertIn("cells[4].floors[1][2]", result["differences"][0])

    def test_swapped_wall_layers_are_a_difference(self):
        other = copy.deepcopy(self.snap)
        for cell in other["cells"]:
            cell["walls"].reverse()
        self.assertEqual(ds1_cases.compare_maps(self.snap, other)["status"], "fail")

    def test_altered_object_and_path_are_differences(self):
        other = copy.deepcopy(self.snap)
        other["objects"][0]["flags"] += 1
        other["paths"][0]["points"][1][2] = 9
        diffs = ds1_cases.compare_maps(self.snap, other)["differences"]
        self.assertEqual(len(diffs), 2)

    def test_omitted_field_is_a_difference(self):
        other = copy.deepcopy(self.snap)
        del other["groups"]
        self.assertEqual(ds1_cases.compare_maps(self.snap, other)["status"], "fail")

    def test_version_is_only_ignored_under_the_named_upgrade(self):
        other = dict(copy.deepcopy(self.snap), version=17)
        self.assertEqual(ds1_cases.compare_maps(self.snap, other)["status"], "fail")
        self.assertEqual(ds1_cases.compare_maps(
            self.snap, other, {"normalizations": ["writer_v18_upgrade"]})["status"], "pass")

    def test_rejection_only_matches_rejection(self):
        err = {"schema": ds1_cases.SCHEMA, "status": "error"}
        self.assertEqual(ds1_cases.compare_maps(err, dict(err, error="x"))["status"], "pass")
        self.assertEqual(ds1_cases.compare_maps(err, self.snap)["status"], "fail")
        self.assertEqual(ds1_cases.compare_maps(self.snap, err)["status"], "fail")


def _probe():
    exe = os.environ.get("DS1EDIT_PROBE_EXE")
    if exe and Path(exe).exists():
        return Path(exe)
    if os.environ.get("DS1EDIT_REQUIRE_PROBE") == "1":
        raise AssertionError(f"DS1EDIT_PROBE_EXE missing or not found: {exe!r}")
    raise unittest.SkipTest("DS1EDIT_PROBE_EXE not set: real-reader lane not run")


class ProbeIntegrationTests(unittest.TestCase):
    """The editor's real reader/writer against every generated fixture."""

    @classmethod
    def setUpClass(cls):
        cls.probe = _probe()
        cls._tmp = tempfile.TemporaryDirectory(prefix="ds1 probe ")
        cls.root = Path(cls._tmp.name)
        cls.manifest = fixtures.generate(cls.root / "in")
        (cls.root / "out").mkdir()

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_reader_matches_every_authored_expectation(self):
        for case in self.manifest["cases"]:
            with self.subTest(case=case["id"]):
                out = self.root / "out" / f"{case['id']}.json"
                rc, snap = ds1_cases.run_probe(self.probe, self.root / "in" / case["file"], out)
                self.assertIsNotNone(snap, f"probe wrote nothing (rc={rc})")
                if case["expected"]["status"] == "error":
                    self.assertEqual(rc, 2)
                else:
                    self.assertEqual(rc, 0)
                result = ds1_cases.compare_maps(case["expected"], snap,
                                                {"normalizations": ["source_ignored"]})
                self.assertEqual(result["status"], "pass", "\n".join(result["differences"][:20]))

    def test_resaved_maps_keep_every_field_under_declared_transformations(self):
        for case in self.manifest["cases"]:
            if case["expected"]["status"] != "ok":
                continue
            with self.subTest(case=case["id"]):
                saved = self.root / "out" / f"{case['id']} resaved.ds1"
                rc = ds1_cases.run_resave(self.probe, self.root / "in" / case["file"], saved)
                self.assertEqual(rc, 0)
                out = self.root / "out" / f"{case['id']} resaved.json"
                rc, snap = ds1_cases.run_probe(self.probe, saved, out)
                self.assertEqual(rc, 0)
                self.assertEqual(snap["version"], 18)
                names = ["source_ignored", "writer_v18_upgrade"]
                if case["version"] < 4:
                    names.append("legacy_tag_layer_dropped")
                result = ds1_cases.compare_maps(case["expected"], snap, {"normalizations": names})
                self.assertEqual(result["status"], "pass", "\n".join(result["differences"][:20]))

    def test_a_wrong_expectation_is_detected(self):
        case = next(c for c in self.manifest["cases"] if c["id"] == "v18_default")
        out = self.root / "out" / "control.json"
        _, snap = ds1_cases.run_probe(self.probe, self.root / "in" / case["file"], out)
        wrong = copy.deepcopy(case["expected"])
        wrong["cells"][0]["walls"][0][4] ^= 0x40
        result = ds1_cases.compare_maps(wrong, snap, {"normalizations": ["source_ignored"]})
        self.assertEqual(result["status"], "fail")


if __name__ == "__main__":
    unittest.main()
