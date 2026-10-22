"""
Tests for the catalogued vanilla DS1 sweep (scripts/equivalence/ds1_sweep.py).

Portable: no game data, MPQ or probe. The archive reader and the probe are
replaced by fakes; the code under test is the sweep's own extraction,
accounting, classification and case construction.

Run from the repository root:
    python -m unittest -v test.python.test_ds1_sweep
"""

import copy
import hashlib
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import ds1_sweep as S  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "make_ds1_fixtures", REPO_ROOT / "test" / "fixtures" / "make_ds1_fixtures.py")
fx = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fx)

LVLPREST = ("Name\tDef\tLevelId\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\tDt1Mask\r\n"
            "A\t1\t5\tAct1/Town/a.ds1\t0\t0\t0\t0\t0\t3\r\n"
            "B\t2\t0\tAct1/Out/b.ds1\t0\t0\t0\t0\t0\t1\r\n")
LEVELS = "Name\tId\tLevelType\r\nTown\t5\t7\r\n"
LVLTYPES = ("Name\tId\tFile 1\tFile 2\tAct\r\n"
            "T\t7\tAct1/Town/floor.dt1\tAct1/Town/missing.dt1\t1\r\n"
            "W\t8\tAct1/Out/w.dt1\t0\t1\r\n")


def vp(rel):
    return C.normalize_virtual_path(S.TILES_DIR + rel)


class HeaderTests(unittest.TestCase):
    def test_header_fields_by_version(self):
        d18, _ = fx.encode(fx.default_spec(18))
        self.assertEqual(S.header_fields(d18), {"version": 18, "width": 3, "height": 2, "act": 2})
        d3, _ = fx.encode(fx.default_spec(3))
        self.assertEqual(S.header_fields(d3)["act"], 1)
        self.assertIsNone(S.header_fields(b"\x01\x00"))

    def test_header_mismatch_reported(self):
        snap = fx.expected_snapshot(fx.default_spec(18))
        h = {"version": 18, "width": 3, "height": 2, "act": 2}
        self.assertEqual(S.header_mismatches(h, snap), [])
        h["width"] = 4
        self.assertTrue(S.header_mismatches(h, snap)[0].startswith("width"))
        self.assertTrue(S.header_mismatches(None, snap))

    def test_families_and_policy(self):
        self.assertEqual(S.version_family(3), "v3")
        self.assertEqual(S.version_family(13), "v13-14")
        self.assertEqual(S.version_family(18), "v18")
        self.assertIn("legacy_tag_layer_dropped", S.resave_policy({"version": 3}))
        self.assertNotIn("legacy_tag_layer_dropped", S.resave_policy({"version": 8}))


class TileInputTests(unittest.TestCase):
    def setUp(self):
        self.t = S.TileInputs(LVLPREST, LEVELS, LVLTYPES,
                              {vp("Act1/Town/floor.dt1"), vp("Act1/Out/w.dt1")})

    def test_authoritative_level_type_and_mask(self):
        r = self.t.resolve(vp("Act1/Town/a.ds1"), 1)
        classes = {k["key"]: k["class"] for k in r["keys"]}
        self.assertEqual(classes[vp("Act1/Town/floor.dt1")], "resolved")
        self.assertEqual(classes[vp("Act1/Town/missing.dt1")], "dt1_missing_from_corpus")

    def test_context_dependent_and_unreferenced(self):
        r = self.t.resolve(vp("Act1/Out/b.ds1"), 1)
        self.assertIn("level_type_context_dependent", {k["class"] for k in r["keys"]})
        r = self.t.resolve(vp("Act1/none.ds1"), 1)
        self.assertEqual(r["keys"][0]["class"], "no_lvlprest_reference")


class FakeArchive:
    def __init__(self, files):
        self.files = files
        self.closed = False

    def read(self, name):
        return self.files.get(name.lower())

    def close(self):
        self.closed = True


def manifest_for(entries):
    assets = [{"path": p, "archive": "D2Data.mpq", "kind": "ds1", "size": len(b),
               "sha256": hashlib.sha256(b).hexdigest()} for p, b in entries]
    return {"schema": C.BASELINE_SCHEMA, "digest": "0" * 64, "assets": assets,
            "enumeration": {"limits": ["test limit"]}}


class ExtractionTests(unittest.TestCase):
    def test_extracts_once_checks_hash_and_reuses(self):
        good, _ = fx.encode(fx.default_spec(18))
        files = {vp("a/x.ds1"): good, vp("a/bad.ds1"): b"tampered"}
        man = manifest_for([(vp("a/x.ds1"), good), (vp("a/bad.ds1"), b"original")])
        calls = []

        def factory(path):
            calls.append(path)
            return FakeArchive(files)
        with tempfile.TemporaryDirectory() as d:
            game = Path(d) / "game"
            game.mkdir()
            (game / "D2Data.mpq").write_bytes(b"")
            corpus = Path(d) / "corpus root"
            got = S.extract_assets(man, game, corpus, man["assets"], factory)
            self.assertIsNotNone(got[vp("a/x.ds1")][0])
            self.assertIsNone(got[vp("a/bad.ds1")][0])
            self.assertIn("sha256", got[vp("a/bad.ds1")][1])
            got2 = S.extract_assets(man, game, corpus, man["assets"][:1], factory)
            self.assertEqual(len(calls), 1, "a valid extracted copy is reused")
            self.assertEqual(got2[vp("a/x.ds1")][0].read_bytes(), good)

    def test_missing_archive_makes_assets_unavailable(self):
        man = manifest_for([(vp("a/x.ds1"), b"x")])
        with tempfile.TemporaryDirectory() as d:
            got = S.extract_assets(man, Path(d), Path(d) / "c", man["assets"],
                                   lambda p: FakeArchive({}))
            self.assertIsNone(got[vp("a/x.ds1")][0])


class CaseTests(unittest.TestCase):
    """sweep_one with a fake probe."""

    def run_one(self, data, dump, resave_rc=0, redump=None):
        asset = {"path": vp("act1/t.ds1"), "archive": "D2Data.mpq", "size": len(data),
                 "sha256": hashlib.sha256(data).hexdigest()}

        def fake_probe(exe, path, out):
            snap = dump if "resaved" not in str(out) else (redump or dump)
            return (2 if snap.get("status") == "error" else 0), copy.deepcopy(snap)

        def fake_resave(exe, src, out):
            Path(out).write_bytes(b"x")
            return resave_rc
        with tempfile.TemporaryDirectory() as d, \
                mock.patch.object(S.ds1_cases, "run_probe", fake_probe), \
                mock.patch.object(S.ds1_cases, "run_resave", fake_resave):
            local = Path(d) / "t.ds1"
            local.write_bytes(data)
            return S.sweep_one(asset, local, None, Path("probe"), Path(d) / "w", None,
                               "0" * 64, {"platform": "test"})

    def test_pass_records_features(self):
        spec = fx.default_spec(18)
        data, _ = fx.encode(spec)
        snap = fx.expected_snapshot(spec)
        case = self.run_one(data, snap)
        self.assertEqual(case["status"], "pass", case["failures"])
        self.assertEqual(case["read"]["features"]["objects"], 2)
        self.assertEqual(case["round_trip"]["status"], "pass")
        self.assertEqual(case["schema"], C.CASE_SCHEMA)
        self.assertEqual(case["coverage"]["original_loader"], "not_compared")

    def test_rejection_is_a_mismatch(self):
        data, _ = fx.encode(fx.default_spec(18))
        case = self.run_one(data, {"schema": "ds1edit.map/1", "status": "error", "error": "x"})
        self.assertEqual(case["status"], "mismatch")
        self.assertEqual(case["failures"][0]["kind"], "rejected")

    def test_round_trip_difference_is_a_mismatch(self):
        spec = fx.default_spec(18)
        data, _ = fx.encode(spec)
        snap = fx.expected_snapshot(spec)
        other = copy.deepcopy(snap)
        other["objects"][0]["flags"] = 7
        case = self.run_one(data, snap, redump=other)
        self.assertEqual(case["status"], "mismatch")
        self.assertTrue(any(f["kind"] == "round_trip" for f in case["failures"]))

    def test_header_disagreement_is_a_mismatch(self):
        spec = fx.default_spec(18)
        data, _ = fx.encode(spec)
        snap = fx.expected_snapshot(spec)
        snap["width"] = 9
        case = self.run_one(data, snap)
        self.assertEqual(case["status"], "mismatch")

    def test_failed_resave_is_a_mismatch(self):
        spec = fx.default_spec(18)
        data, _ = fx.encode(spec)
        case = self.run_one(data, fx.expected_snapshot(spec), resave_rc=3)
        self.assertEqual(case["status"], "mismatch")

    def test_missing_dll_is_environment_not_rejection(self):
        data, _ = fx.encode(fx.default_spec(18))
        asset = {"path": vp("act1/t.ds1"), "archive": "D2Data.mpq", "size": len(data),
                 "sha256": hashlib.sha256(data).hexdigest()}
        for rc in (0xC0000135, -1073741515):
            with tempfile.TemporaryDirectory() as d, \
                    mock.patch.object(S.ds1_cases, "run_probe", lambda *a, rc=rc: (rc, None)):
                local = Path(d) / "t.ds1"
                local.write_bytes(data)
                case = S.sweep_one(asset, local, None, Path("probe"), Path(d) / "w", None,
                                   "0" * 64, {})
            self.assertEqual(case["status"], "unavailable")
            self.assertNotIn("read", case)

    def test_unextracted_input_is_unavailable(self):
        asset = {"path": vp("x.ds1"), "archive": "D2Data.mpq", "size": 1, "sha256": "0" * 64}
        case = S.sweep_one(asset, None, "absent", Path("p"), Path("w"), None, "0" * 64, {})
        self.assertEqual(case["status"], "unavailable")


class AggregateTests(unittest.TestCase):
    def test_each_input_accounted_once(self):
        man = manifest_for([(vp("a.ds1"), b"a"), (vp("b.ds1"), b"b")])
        cases = [{"id": S.case_id(vp("a.ds1")), "status": "pass", "failures": []},
                 {"id": S.case_id(vp("b.ds1")), "status": "pass", "failures": []}]
        agg = S.aggregate(cases, man)
        self.assertTrue(agg["accounting"]["each_input_once"])
        self.assertEqual(S.exit_code(agg), 0)
        agg = S.aggregate([cases[0], cases[0]], man)
        self.assertFalse(agg["accounting"]["each_input_once"])
        self.assertEqual(S.exit_code(agg), 2)
        self.assertIn("test limit", agg["limitations"])

    def test_mismatch_exit_code(self):
        man = manifest_for([(vp("a.ds1"), b"a")])
        agg = S.aggregate([{"id": S.case_id(vp("a.ds1")), "status": "mismatch",
                            "failures": [], "read": {"status": "rejected", "error": "e"}}], man)
        self.assertEqual(S.exit_code(agg), 1)
        self.assertEqual(len(agg["rejected"]), 1)


if __name__ == "__main__":
    unittest.main()
