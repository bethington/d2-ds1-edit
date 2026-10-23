"""
Tests for the local-only Project Diablo 2 compatibility lane
(scripts/equivalence/pd2_lane.py).

Portable: no PD2 data, MPQ, probe or renderer. Archives are replaced by fakes
that follow the reader interface (tools/d2mpq.py D2Archive); the code under
test is the lane's own identity binding, inventory classification, exit-code
and output judgement, baseline store and case construction.

Run from the repository root:
    python -m unittest -v test.python.test_pd2_lane
"""

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import pd2_lane as P  # noqa: E402
from scripts.equivalence import qualify as Q  # noqa: E402


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


class FakeArchive:
    """The slice of tools/d2mpq.D2Archive the lane relies on."""

    def __init__(self, name, files, listfile=True, unnamed=0):
        self.name = name
        self.files = {k: v for k, v in files.items()}
        self._lower = {k.lower(): k for k in files}
        self.has_listfile = listfile
        self.unnamed = unnamed
        self.reads = []
        self.closed = False

    @property
    def block_count(self):
        return len(self.files) + self.unnamed + (1 if self.has_listfile else 0)

    def has(self, name):
        return name.lower() in self._lower

    def read(self, name, required=False):
        self.reads.append(name)
        k = self._lower.get(name.lower())
        return None if k is None else self.files[k]

    def namelist(self, extra_names=()):
        names = list(self.files) if self.has_listfile else []
        known = {n.lower() for n in names}
        for n in extra_names:
            if n.lower() not in known and self.has(n):
                names.append(self._lower[n.lower()])
                known.add(n.lower())
        return sorted(names, key=str.lower)

    def entries(self, names=None):
        out = []
        for n in (names if names is not None else self.namelist()):
            k = self._lower.get(n.lower())
            if k is not None:
                out.append((k, 0x80000100, len(self.files[k]), len(self.files[k])))
        return out

    def close(self):
        self.closed = True


# ---------------------------------------------------------------------------
# identity
# ---------------------------------------------------------------------------

class VersionTests(unittest.TestCase):
    def test_finds_nested_version_key(self):
        v, where = P.extract_version({"a": {"b": 1}, "client": {"Version": "1.2.3"}})
        self.assertEqual((v, where), ("1.2.3", "client/Version"))

    def test_numeric_build_is_stringified(self):
        v, where = P.extract_version({"build": 4821})
        self.assertEqual((v, where), ("4821", "build"))

    def test_no_version_is_none_not_a_guess(self):
        self.assertEqual(P.extract_version({"audio": {"volume": 3}, "x": [1, 2]}),
                         (None, None))

    def test_booleans_and_empty_strings_are_not_versions(self):
        self.assertEqual(P.extract_version({"version": True, "build": ""}), (None, None))


def make_install(root: Path, pd2_files=None, base_files=None, project_json=None):
    pd2 = root / "ProjectD2"
    pd2.mkdir(parents=True)
    for n in P.PD2_ARCHIVES:
        (pd2 / n).write_bytes((pd2_files or {}).get(n, b"pd2:" + n.encode()))
    for n in P.BASE_ARCHIVES:
        (root / n).write_bytes((base_files or {}).get(n, b"base:" + n.encode()))
    if project_json is not None:
        (pd2 / "ProjectDiablo.json").write_text(json.dumps(project_json))
    return root, pd2


class IdentityTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def ident(self, sub, **kw):
        base, pd2 = make_install(self.root / sub, **kw)
        return P.compute_identity(pd2, base)

    def test_every_pd2_archive_is_hashed(self):
        i = self.ident("a", project_json={"version": "9.1"})
        names = [a["name"] for a in i["archives"] if a["role"] == "pd2"]
        self.assertEqual(names, list(P.PD2_ARCHIVES))
        a = {x["name"]: x for x in i["archives"] if x["role"] == "pd2"}
        self.assertEqual(a["pd2maps.mpq"]["sha256"], sha(b"pd2:pd2maps.mpq"))
        self.assertEqual(i["version"], "9.1")
        self.assertEqual(i["version_source"], "ProjectDiablo.json:version")

    def test_unversioned_install_is_recorded_as_such(self):
        i = self.ident("a", project_json={"audio": {"volume": 1}})
        self.assertIsNone(i["version"])
        self.assertIn("no version", i["version_source"])

    def test_digest_binds_archive_bytes(self):
        a = self.ident("a", project_json={"version": "1"})
        b = self.ident("b", project_json={"version": "1"},
                       pd2_files={"pd2maps.mpq": b"changed"})
        self.assertNotEqual(a["digest"], b["digest"])
        self.assertEqual(a["digest"], C.canonical_digest(a))

    def test_digest_ignores_where_the_install_lives(self):
        a = self.ident("a", project_json={"version": "1"})
        b = self.ident("b", project_json={"version": "1"})
        self.assertEqual(a["digest"], b["digest"])
        self.assertNotIn("a", json.dumps(a).split("ProjectD2")[0][-3:])

    def test_digest_binds_the_vanilla_chain_the_lane_reads(self):
        a = self.ident("a")
        b = self.ident("b", base_files={"d2data.mpq": b"other"})
        self.assertNotEqual(a["digest"], b["digest"])

    def test_missing_pd2_archive_is_an_error(self):
        base, pd2 = make_install(self.root / "a")
        (pd2 / "pd2data.mpq").unlink()
        with self.assertRaises(P.Pd2Error):
            P.compute_identity(pd2, base)

    def test_metadata_md5_cross_check(self):
        base, pd2 = make_install(self.root / "a")
        good = hashlib.md5(b"pd2:pd2maps.mpq").hexdigest()
        (pd2 / "local_metadata.json").write_text(json.dumps({"checksum": [
            good + "  pd2maps.mpq", "0" * 32 + "  pd2data.mpq"]}))
        i = P.compute_identity(pd2, base)
        self.assertEqual(i["metadata_check"]["matched"], ["pd2maps.mpq"])
        self.assertEqual(i["metadata_check"]["differ"], ["pd2data.mpq"])


class CompareIdentityTests(unittest.TestCase):
    def ident(self, version="1", digest="d" * 64):
        return {"version": version, "digest": digest}

    def test_same_identity_is_comparable(self):
        r = P.compare_identity(self.ident(), self.ident())
        self.assertEqual(r["verdict"], "comparable")

    def test_other_version_is_incomparable(self):
        r = P.compare_identity(self.ident("1"), self.ident("2", "e" * 64))
        self.assertEqual(r["verdict"], "incomparable")
        self.assertTrue(any("version" in x for x in r["reasons"]))

    def test_same_version_other_bytes_is_still_incomparable(self):
        r = P.compare_identity(self.ident(), self.ident(digest="e" * 64))
        self.assertEqual(r["verdict"], "incomparable")

    def test_missing_baseline_identity_is_incomparable(self):
        self.assertEqual(P.compare_identity(None, self.ident())["verdict"], "incomparable")


# ---------------------------------------------------------------------------
# inventory
# ---------------------------------------------------------------------------

class ClassifyTests(unittest.TestCase):
    def test_unique_when_vanilla_lacks_it(self):
        self.assertEqual(P.classify_file(b"abc", None), "pd2_unique")

    def test_identical_bytes(self):
        self.assertEqual(P.classify_file(b"abc", lambda: b"abc", vanilla_size=3), "identical")

    def test_different_size_overrides_without_reading_vanilla(self):
        def boom():
            raise AssertionError("must not read")
        self.assertEqual(P.classify_file(b"abcd", boom, vanilla_size=3), "overrides_vanilla")

    def test_same_size_different_bytes_overrides(self):
        self.assertEqual(P.classify_file(b"abc", lambda: b"abd", vanilla_size=3),
                         "overrides_vanilla")


def pd2_set():
    return [
        ("pd2data", FakeArchive("pd2data.mpq", {
            "data\\global\\excel\\Levels.txt": b"L2",
            "data\\global\\excel\\Objects.txt": b"O1",
            "data\\global\\excel\\armor.bin": b"ignored"})),
        ("pd2maps", FakeArchive("pd2maps.mpq", {
            "data\\global\\tiles\\ACT1\\new.ds1": b"N",
            "data\\global\\tiles\\ACT1\\town\\townE1.ds1": b"T2"})),
        ("pd2assets", FakeArchive("pd2assets.mpq", {
            "data\\global\\items\\a.dc6": b"6",
            "data\\global\\tiles\\ACT1\\town\\townE1.ds1": b"shadowed"})),
        ("pd2monchars", FakeArchive("pd2monchars.mpq", {
            "data\\global\\monsters\\x\\hth\\a.DCC": b"d",
            "data\\global\\monsters\\x\\cof\\a.COF": b"c"})),
        ("pd2patch", FakeArchive("patch_d2.mpq", {}, listfile=False, unnamed=2)),
    ]


def base_set():
    return [
        ("patch_d2", FakeArchive("patch_d2.mpq", {}, listfile=False)),
        ("d2data", FakeArchive("d2data.mpq", {
            "data\\global\\excel\\Levels.txt": b"L1",
            "data\\global\\excel\\Objects.txt": b"O1",
            "data\\global\\tiles\\ACT1\\town\\townE1.ds1": b"T1",
            "data\\global\\monsters\\x\\hth\\a.dcc": b"d"})),
    ]


class InventoryTests(unittest.TestCase):
    def inv(self):
        return P.build_inventory(pd2_set(), base_set())

    def test_counts_by_kind_and_class(self):
        c = self.inv()["counts"]
        self.assertEqual(c["txt"], {"pd2_unique": 0, "overrides_vanilla": 1, "identical": 1})
        self.assertEqual(c["ds1"], {"pd2_unique": 1, "overrides_vanilla": 1, "identical": 0})
        self.assertEqual(c["dcc"], {"pd2_unique": 0, "overrides_vanilla": 0, "identical": 1})
        self.assertEqual(c["dc6"]["pd2_unique"], 1)
        self.assertEqual(c["cof"]["pd2_unique"], 1)
        self.assertEqual(c["dt1"], {"pd2_unique": 0, "overrides_vanilla": 0, "identical": 0})

    def test_untracked_kinds_are_not_inventoried(self):
        files = self.inv()["files"]
        self.assertFalse(any(p.endswith(".bin") for p in files))

    def test_effective_file_is_the_first_pd2_archive_and_shadow_is_recorded(self):
        inv = self.inv()
        e = inv["files"]["data\\global\\tiles\\act1\\town\\towne1.ds1"]
        self.assertEqual(e["archive"], "pd2maps")
        self.assertEqual(e["sha256"], sha(b"T2"))
        self.assertEqual(inv["shadowed"], [{
            "path": "data\\global\\tiles\\act1\\town\\towne1.ds1",
            "effective": "pd2maps", "shadowed_in": ["pd2assets"]}])

    def test_override_remembers_the_vanilla_side(self):
        e = self.inv()["files"]["data\\global\\tiles\\act1\\town\\towne1.ds1"]
        self.assertEqual(e["class"], "overrides_vanilla")
        self.assertEqual(e["vanilla_sha256"], sha(b"T1"))

    def test_missing_listfile_is_a_recorded_limit(self):
        inv = self.inv()
        patch = [a for a in inv["archives"] if a["slot"] == "pd2patch"][0]
        self.assertFalse(patch["listfile"])
        self.assertEqual(patch["blocks"], 2)
        self.assertEqual(patch["unnamed"], 2)
        self.assertTrue(any("pd2patch" in l and "no (listfile)" in l for l in inv["limits"]))

    def test_archive_with_a_listfile_has_no_limit(self):
        inv = self.inv()
        maps = [a for a in inv["archives"] if a["slot"] == "pd2maps"][0]
        self.assertTrue(maps["listfile"])
        self.assertFalse(any("pd2maps" in l for l in inv["limits"]))

    def test_names_resolve_in_a_listfile_less_archive_via_sibling_names(self):
        pd2 = pd2_set()
        pd2[4] = ("pd2patch", FakeArchive("patch_d2.mpq", {
            "data\\global\\excel\\Levels.txt": b"L3"}, listfile=False, unnamed=1))
        inv = P.build_inventory(pd2, base_set())
        e = inv["files"]["data\\global\\excel\\levels.txt"]
        self.assertEqual(e["archive"], "pd2data")  # earlier slot still wins
        self.assertEqual(inv["shadowed"][0]["shadowed_in"], ["pd2patch"])
        patch = [a for a in inv["archives"] if a["slot"] == "pd2patch"][0]
        self.assertEqual(patch["names"], 1)
        self.assertEqual(patch["unnamed"], 1)


# ---------------------------------------------------------------------------
# chain
# ---------------------------------------------------------------------------

class ChainTests(unittest.TestCase):
    def test_first_archive_wins_and_names_the_source(self):
        a = FakeArchive("a", {"x.txt": b"1"})
        b = FakeArchive("b", {"x.txt": b"2", "y.txt": b"3"})
        ch = P.Chain([("pd2", a), ("base", b)])
        self.assertEqual(ch.read("X.TXT"), (b"1", "pd2"))
        self.assertEqual(ch.read("y.txt"), (b"3", "base"))
        self.assertIsNone(ch.read("z.txt"))
        self.assertTrue(ch.has("Y.txt"))
        self.assertFalse(ch.has("z"))
        self.assertIn("y.txt", ch.pathset())
        ch.close()
        self.assertTrue(a.closed and b.closed)


# ---------------------------------------------------------------------------
# process outcomes
# ---------------------------------------------------------------------------

class ExitTests(unittest.TestCase):
    def test_zero_is_ok_and_one_is_a_declared_rejection(self):
        self.assertEqual(P.classify_exit(0), "ok")
        self.assertEqual(P.classify_exit(1), "rejected")

    def test_access_violation_is_a_crash(self):
        self.assertEqual(P.classify_exit(0xC0000005), "crash")
        self.assertEqual(P.classify_exit(-1073741819), "crash")

    def test_posix_signal_is_a_crash(self):
        self.assertEqual(P.classify_exit(-11), "crash")

    def test_other_nonzero_is_a_crash_not_a_rejection(self):
        self.assertEqual(P.classify_exit(3), "crash")
        self.assertEqual(P.classify_exit(2), "crash")  # bad usage from a lane is a lane bug

    def test_timeout_is_a_hang(self):
        self.assertEqual(P.classify_exit(None, timed_out=True), "hang")

    def test_dll_not_found_is_environment_not_a_defect(self):
        self.assertEqual(P.classify_exit(0xC0000135), "environment")


class GraphicsTests(unittest.TestCase):
    def files(self):
        return [{"path": "a\\1.dcc", "kind": "dcc"},
                {"path": "a\\2.dcc", "kind": "dcc"},
                {"path": "a\\3.dcc", "kind": "dcc"},
                {"path": "b\\1.dc6", "kind": "dc6"}]

    def runner(self, results):
        def run(item):
            return results[item["path"]]
        return run

    def test_all_ok_passes(self):
        res = P.run_graphics(self.files(), self.runner(
            {f["path"]: (0, "", False) for f in self.files()}), jobs=2)
        self.assertEqual(res["counts"]["dcc"], {"ok": 3})
        self.assertEqual(res["defects"], [])
        self.assertEqual(res["rejections"], [])

    def test_rejection_and_crash_are_kept_apart(self):
        r = {f["path"]: (0, "", False) for f in self.files()}
        r["a\\2.dcc"] = (1, "decode failed: bad", False)
        r["a\\3.dcc"] = (0xC0000005, "", False)
        res = P.run_graphics(self.files(), self.runner(r), jobs=1)
        self.assertEqual([x["path"] for x in res["rejections"]], ["a\\2.dcc"])
        self.assertEqual([x["path"] for x in res["defects"]], ["a\\3.dcc"])
        self.assertEqual(res["defects"][0]["outcome"], "crash")
        self.assertEqual(res["counts"]["dcc"], {"ok": 1, "rejected": 1, "crash": 1})

    def test_status_of_graphics_case(self):
        self.assertEqual(P.graphics_status({"defects": [], "rejections": [],
                                            "environment": []}), "pass")
        self.assertEqual(P.graphics_status({"defects": [{}], "rejections": [],
                                            "environment": []}), "mismatch")
        # a declared rejection is the decoder's contract: a finding, not a failure
        self.assertEqual(P.graphics_status({"defects": [], "rejections": [{}],
                                            "environment": []}), "pass")
        self.assertEqual(P.graphics_status({"defects": [], "rejections": [],
                                            "environment": [{}]}), "unavailable")

    def test_crash_writes_a_repro(self):
        with tempfile.TemporaryDirectory() as d:
            src = Path(d) / "in.dcc"
            src.write_bytes(b"\x01\x02")
            out = P.write_repro(Path(d) / "failures", {
                "path": "data\\global\\x\\in.dcc", "kind": "dcc", "outcome": "crash",
                "returncode": 3221225477, "command": ["probe", "decode"],
                "stderr": "oops"}, src)
            self.assertEqual((out / "input.dcc").read_bytes(), b"\x01\x02")
            meta = json.loads((out / "repro.json").read_text())
            self.assertEqual(meta["returncode"], 3221225477)
            self.assertEqual(meta["input_sha256"], sha(b"\x01\x02"))


# ---------------------------------------------------------------------------
# render checks
# ---------------------------------------------------------------------------

PD2_OK = ("d2install: using configured path <C:\\Diablo2>\n"
          "d2install: Project Diablo 2 data from <C:\\Diablo2\\ProjectD2> "
          "(use_pd2=yes, found via ini)\nheadless mode : saved 10 x 10 screenshot\n")


class PD2DataTests(unittest.TestCase):
    def test_detects_pd2_data(self):
        self.assertTrue(P.render_uses_pd2(PD2_OK, "C:\\Diablo2\\ProjectD2")[0])
        self.assertTrue(P.render_uses_pd2(PD2_OK, "c:/diablo2/projectd2/")[0])

    def test_vanilla_data_line_fails(self):
        out = "d2install: vanilla Diablo II data from <C:\\Diablo2> (use_pd2=no)\n"
        ok, why = P.render_uses_pd2(out, "C:\\Diablo2\\ProjectD2")
        self.assertFalse(ok)
        self.assertIn("Project Diablo 2 data from", why)

    def test_missing_line_fails(self):
        self.assertFalse(P.render_uses_pd2("headless mode : saved\n", "C:\\x")[0])

    def test_fallback_error_line_fails_even_with_the_pd2_line(self):
        out = PD2_OK + "d2install: ERROR use_pd2=yes but no Project Diablo 2 folder\n"
        self.assertFalse(P.render_uses_pd2(out, "C:\\Diablo2\\ProjectD2")[0])

    def test_other_pd2_folder_fails(self):
        self.assertFalse(P.render_uses_pd2(PD2_OK, "D:\\Elsewhere\\ProjectD2")[0])

    def test_read_source_names_the_archive_of_the_last_read(self):
        out = ("want to read a txt from mpq : Data\\Global\\Excel\\LvlPrest.txt\n"
               "ok (10 bytes), found in C:\\Diablo2\\d2data.mpq\n"
               "want to read a txt from mpq : Data\\Global\\Excel\\LvlPrest.txt\n"
               "ok (10 bytes), found in C:\\Diablo2\\ProjectD2\\pd2data.mpq\n")
        self.assertEqual(P.read_source(out, "lvlprest.txt"),
                         "C:\\Diablo2\\ProjectD2\\pd2data.mpq")
        self.assertIsNone(P.read_source("nothing\n", "lvlprest.txt"))


class IniTests(unittest.TestCase):
    def test_owned_ini_is_windowed_pd2_and_explicit(self):
        ini = P.render_ini("C:\\Diablo2", "C:\\Diablo2\\ProjectD2", zoom="1:4", gamma="1.00")
        self.assertIn("full_screen = NO", ini)
        self.assertIn("use_pd2 = yes", ini)
        self.assertIn("pd2_dir = C:\\Diablo2\\ProjectD2", ini)
        self.assertIn("d2_install = C:\\Diablo2", ini)
        self.assertIn("mod_dir =", ini)
        self.assertIn("upscale_enabled = NO", ini)


class SelectionTests(unittest.TestCase):
    def files(self):
        f = {}
        for i, (d, size) in enumerate([("act1\\a", 50), ("act1\\a", 10), ("act2\\b", 20),
                                       ("act3\\c", 30), ("act4\\d", 40), ("act5\\e", 60),
                                       ("act5\\e", 5)]):
            p = "data\\global\\tiles\\%s\\m%d.ds1" % (d, i)
            f[p] = {"kind": "ds1", "class": "pd2_unique", "size": size}
        f["data\\global\\tiles\\act1\\town\\towne1.ds1"] = {
            "kind": "ds1", "class": "overrides_vanilla", "size": 99}
        f["data\\global\\tiles\\act2\\town\\bad.ds1"] = {
            "kind": "ds1", "class": "overrides_vanilla", "size": 1}
        return f

    def test_picks_one_town_override_and_a_bounded_unique_set(self):
        sel = P.select_render_set(self.files(), usable=lambda p: True, unique_count=4)
        uniq = [s for s in sel if s["role"] == "unique"]
        town = [s for s in sel if s["role"] == "override_town"]
        self.assertEqual(len(uniq), 4)
        self.assertEqual([t["path"] for t in town],
                         ["data\\global\\tiles\\act1\\town\\towne1.ds1"])

    def test_unique_picks_span_distinct_areas_and_prefer_small_maps(self):
        sel = P.select_render_set(self.files(), usable=lambda p: True, unique_count=4)
        dirs = [s["path"].rsplit("\\", 1)[0] for s in sel if s["role"] == "unique"]
        self.assertEqual(len(set(dirs)), 4)
        sizes = {s["path"]: s["size"] for s in sel}
        self.assertEqual(sizes["data\\global\\tiles\\act1\\a\\m1.ds1"], 10)

    def test_selection_is_deterministic(self):
        a = P.select_render_set(self.files(), usable=lambda p: True, unique_count=3)
        b = P.select_render_set(dict(reversed(list(self.files().items()))),
                                usable=lambda p: True, unique_count=3)
        self.assertEqual(a, b)

    def test_unusable_maps_are_skipped(self):
        sel = P.select_render_set(self.files(), usable=lambda p: "act1" not in p,
                                  unique_count=4)
        self.assertFalse(any("act1" in s["path"] for s in sel))


# ---------------------------------------------------------------------------
# local baseline
# ---------------------------------------------------------------------------

IDENT = {"version": "1.0", "digest": "a" * 64}
OTHER = {"version": "2.0", "digest": "b" * 64}


class BaselineTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name) / "baseline"
        self.shots = Path(self.tmp.name) / "shots"
        self.shots.mkdir()

    def shot(self, name, data):
        p = self.shots / name
        p.write_bytes(data)
        return p

    def test_first_run_creates_an_unreviewed_baseline(self):
        r = P.baseline_check(self.dir, IDENT, {"m1": self.shot("m1.png", b"IMG1")})
        self.assertEqual(r["m1"]["outcome"], "created")
        meta = json.loads((self.dir / "baseline.json").read_text())
        self.assertIs(meta["reviewed"], False)
        self.assertEqual(meta["identity"]["digest"], IDENT["digest"])
        self.assertEqual(meta["cases"]["m1"]["sha256"], sha(b"IMG1"))
        self.assertEqual((self.dir / "images" / "m1.png").read_bytes(), b"IMG1")

    def test_second_run_matches(self):
        P.baseline_check(self.dir, IDENT, {"m1": self.shot("m1.png", b"IMG1")})
        r = P.baseline_check(self.dir, IDENT, {"m1": self.shot("n.png", b"IMG1")})
        self.assertEqual(r["m1"]["outcome"], "match")

    def test_changed_pixels_mismatch_and_baseline_is_untouched(self):
        P.baseline_check(self.dir, IDENT, {"m1": self.shot("m1.png", b"IMG1")})
        r = P.baseline_check(self.dir, IDENT, {"m1": self.shot("n.png", b"IMG2")})
        self.assertEqual(r["m1"]["outcome"], "mismatch")
        self.assertEqual((self.dir / "images" / "m1.png").read_bytes(), b"IMG1")

    def test_other_pd2_version_is_incomparable_not_a_verdict(self):
        P.baseline_check(self.dir, IDENT, {"m1": self.shot("m1.png", b"IMG1")})
        r = P.baseline_check(self.dir, OTHER, {"m1": self.shot("n.png", b"DIFFERENT")})
        self.assertEqual(r["m1"]["outcome"], "incomparable")
        self.assertEqual((self.dir / "images" / "m1.png").read_bytes(), b"IMG1")
        meta = json.loads((self.dir / "baseline.json").read_text())
        self.assertEqual(meta["identity"]["digest"], IDENT["digest"])

    def test_new_case_is_added_to_a_matching_baseline_unreviewed(self):
        P.baseline_check(self.dir, IDENT, {"m1": self.shot("m1.png", b"IMG1")})
        meta = json.loads((self.dir / "baseline.json").read_text())
        meta["reviewed"] = True
        (self.dir / "baseline.json").write_text(json.dumps(meta))
        r = P.baseline_check(self.dir, IDENT, {"m2": self.shot("m2.png", b"IMG2")})
        self.assertEqual(r["m2"]["outcome"], "created")
        meta = json.loads((self.dir / "baseline.json").read_text())
        self.assertIs(meta["reviewed"], False)  # something unseen was added

    def test_reviewed_flag_is_reported(self):
        P.baseline_check(self.dir, IDENT, {"m1": self.shot("m1.png", b"IMG1")})
        r = P.baseline_check(self.dir, IDENT, {"m1": self.shot("n.png", b"IMG1")})
        self.assertIs(r["m1"]["reviewed"], False)

    def test_unreadable_baseline_is_incomparable(self):
        self.dir.mkdir(parents=True)
        (self.dir / "baseline.json").write_text("{not json")
        r = P.baseline_check(self.dir, IDENT, {"m1": self.shot("m1.png", b"IMG1")})
        self.assertEqual(r["m1"]["outcome"], "incomparable")


class DeterminismTests(unittest.TestCase):
    def test_byte_identical(self):
        with tempfile.TemporaryDirectory() as d:
            a, b, c = (Path(d) / n for n in "abc")
            a.write_bytes(b"x"); b.write_bytes(b"x"); c.write_bytes(b"y")
            self.assertTrue(P.files_identical(a, b))
            self.assertFalse(P.files_identical(a, c))


# ---------------------------------------------------------------------------
# cases and exit codes
# ---------------------------------------------------------------------------

class CaseTests(unittest.TestCase):
    def case(self, **kw):
        args = dict(case_id="pd2/x", status="pass", identity=IDENT, failures=[])
        args.update(kw)
        return P.make_case(**args)

    def test_case_is_a_valid_optional_pd2_case(self):
        c = self.case()
        Q.validate_case(c)
        self.assertEqual(c["lane"], "pd2")
        self.assertIs(c["required"], False)
        self.assertEqual(c["baseline_digest"], IDENT["digest"])
        self.assertEqual(c["provenance"]["reference"]["pd2_version"], "1.0")
        self.assertTrue(any("not an original-game" in l for l in c["limitations"]))

    def test_incomparable_maps_to_unavailable_and_says_why(self):
        c = self.case(status="incomparable", failures=[{"kind": "identity"}])
        Q.validate_case(c)
        self.assertEqual(c["status"], "unavailable")
        self.assertEqual(c["comparison"], "incomparable")

    def test_unknown_status_is_rejected(self):
        with self.assertRaises(ValueError):
            self.case(status="great")

    def test_exit_code(self):
        mk = lambda s: {"status": s}  # noqa: E731
        self.assertEqual(P.exit_code([mk("pass"), mk("pass")]), 0)
        self.assertEqual(P.exit_code([mk("pass"), mk("mismatch")]), 1)
        self.assertEqual(P.exit_code([mk("pass"), mk("unavailable")]), 2)
        self.assertEqual(P.exit_code([mk("mismatch"), mk("unavailable")]), 1)
        self.assertEqual(P.exit_code([]), 2)

    def test_baseline_creation_does_not_fail_the_run(self):
        self.assertEqual(P.exit_code([{"status": "partial", "baseline_created": True},
                                      {"status": "pass"}]), 0)
        self.assertEqual(P.exit_code([{"status": "partial"}]), 2)

    def test_summary_table_lists_every_case_status(self):
        txt = P.summary_table([{"id": "pd2/a", "status": "pass", "failures": []},
                               {"id": "pd2/b", "status": "mismatch",
                                "failures": [{"kind": "x"}, {"kind": "y"}]}])
        self.assertIn("pd2/a", txt)
        self.assertIn("mismatch", txt)
        self.assertRegex(txt, r"pd2/b\s+mismatch\s+2")


class QualifyOptionalTests(unittest.TestCase):
    """qualify.py must never let the pd2 lane block or satisfy a release."""

    def test_pd2_lane_is_declared_optional(self):
        self.assertIn("pd2", Q.OPTIONAL_LANES)
        self.assertNotIn("pd2", Q.LANES)

    def test_profile_cannot_require_the_pd2_lane(self):
        prof = {"schema": Q.PROFILE_SCHEMA, "id": "p", "platforms": ["windows-x86"],
                "requirements": [{"id": "r", "title": "t", "cases": [
                    {"id": "pd2/x", "lane": "pd2", "kind": "suite",
                     "platforms": ["windows-x86"], "success_outcome": "pass"}]}]}
        with self.assertRaises(C.ValidationError) as cm:
            Q.validate_profile(C.seal(prof))
        self.assertIn("pd2", str(cm.exception))
        self.assertIn("optional", str(cm.exception))


class QualifyDoesNotBlockTests(unittest.TestCase):
    def test_failing_pd2_case_is_optional_and_does_not_block(self):
        from test.python import test_qualification as T
        profile = T.unit_profile()
        cand = T.make_candidate(profile)
        results = T.passing_results(profile, cand)
        bad = P.make_case("pd2/maps/x", "mismatch", IDENT, [{"kind": "rejected"}])
        results.append(bad)
        report = Q.qualify(profile, cand, results)
        self.assertTrue(report["ready"])
        self.assertIn("pd2/maps/x@pd2/" + bad["platform"],
                      [o["key"] for o in report["optional"]])


if __name__ == "__main__":  # pragma: no cover
    unittest.main()
