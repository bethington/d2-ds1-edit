"""Package D task 4: render_cases refuses every way a map comparison can be
made meaningless, and passes only an executed, reproducible, exact match.

No game data: the archives, baseline, reference images and renderer are all
authored here; the renderer is a fake that writes a known image and the log
lines the real one prints.

    python -m unittest -v test.python.test_render_cases
"""

import copy
import hashlib
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from PIL import Image  # noqa: E402

from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import render_cases as RC  # noqa: E402

PROFILE_PATH = REPO_ROOT / "test" / "equivalence" / "profiles" / "visual-core.json"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class FakeResult:
    def __init__(self, ok, stdout="", stderr="", reason="rendered", returncode=0):
        self.ok, self.stdout, self.stderr = ok, stdout, stderr
        self.reason, self.returncode = reason, returncode


class World:
    """An authored game directory, baseline, references and profile."""

    ASSETS = {
        "data\\global\\tiles\\act1\\town\\towne1.ds1": b"ds1 bytes",
        "data\\global\\tiles\\act1\\town\\floor.dt1": b"dt1 bytes",
        "data\\global\\palette\\act1\\pal.pl2": b"pl2 bytes",
    }
    NAMES = {  # how the renderer names them in its log
        "data\\global\\tiles\\act1\\town\\towne1.ds1": "assets/tiles/ACT1/TOWN/townE1.ds1",
        "data\\global\\tiles\\act1\\town\\floor.dt1": "Data\\Global\\Tiles\\Act1\\Town\\Floor.dt1",
        "data\\global\\palette\\act1\\pal.pl2": "Data\\Global\\Palette\\Act1\\Pal.pl2",
    }

    def __init__(self, root: Path):
        self.root = root
        self.game = root / "game"
        self.game.mkdir()
        archives = []
        for i, name in enumerate(("Patch_D2.mpq", "D2Exp.mpq", "D2Data.mpq", "D2Char.mpq")):
            data = ("archive %d" % i).encode()
            (self.game / name).write_bytes(data)
            archives.append({"name": name, "priority": i, "sha256": sha(data),
                             "size": len(data)})
        self.baseline = C.seal({
            "schema": C.BASELINE_SCHEMA, "build": "vanilla-113c",
            "binaries": [{"name": "D2CMP.dll", "sha256": sha(b"d2cmp"), "size": 5}],
            "archives": archives,
            "enumeration": {"sources": [], "limits": [], "complete": True},
            "assets": [{"path": p, "archive": "D2Data.mpq", "size": len(b),
                        "sha256": sha(b)} for p, b in sorted(self.ASSETS.items())],
            "render": {"mod_directories": "none", "user_preferences": "ignored"},
        })
        self.editor = root / "editor"
        self.editor.mkdir()
        (self.editor / "ds1edit.dt1").write_bytes(b"editor dt1")
        self.image = Image.new("RGB", (8, 6), (10, 20, 30))
        self.image.putpixel((3, 2), (200, 0, 0))
        self.ref = root / "ref.png"
        self.image.save(self.ref)
        self.candidate = root / "bin" / "ds1edit.exe"
        self.candidate.parent.mkdir()
        self.candidate.write_bytes(b"not really an executable")
        self.profile = {
            "schema": RC.PROFILE_SCHEMA, "id": "test-visual",
            "baseline": {"build": "vanilla-113c", "digest": self.baseline["digest"]},
            "render": {"zoom": "1:4", "gamma": "1.00", "mod_dir": "none",
                       "preferences": "isolated", "seed": "fixed", "frame": "0",
                       "time": "none", "repeat": 2},
            "editor_assets": [{"path": str(self.editor / "ds1edit.dt1"),
                               "sha256": sha(b"editor dt1")}],
            "aggregate_case": "visual.representative",
            "required_features": ["act1", "dt1-rle"],
            "external_features": {},
            "cases": [{
                "id": "visual.core.act1_town", "required": True, "act": 1,
                "map": {"lvltype": 1, "lvlprest": 1,
                        "ds1": "assets/tiles/ACT1/TOWN/townE1.ds1"},
                "features": ["act1", "dt1-rle"],
                "inputs": [{"path": p, "sha256": sha(b)}
                           for p, b in sorted(self.ASSETS.items())],
                "reference": {"path": str(self.ref),
                              "sha256": sha(self.ref.read_bytes())},
                "compare": "exact-rgb",
            }],
        }
        self.calls = 0

    def log(self, source=None, extra=(), mod="(null)"):
        source = source or str(self.game / "D2Data.mpq")
        lines = ["d2char = x", f"mod_dir                 = {mod}"]
        lines += list(extra)
        for key in sorted(self.ASSETS):
            lines.append(f"want to read a thing : {self.NAMES[key]}")
            lines.append(f"ok ({len(self.ASSETS[key])} bytes), found in {source}")
        return "\n".join(lines) + "\n"

    def runner(self, image=None, stdout=None, ok=True, vary=False):
        def run(runtime, case, out):
            self.calls += 1
            img = (image or self.image).copy()
            if vary and self.calls % 2 == 0:
                img.putpixel((0, 0), (1, 2, 3))
            if ok:
                img.save(out, "BMP")
            return FakeResult(ok, stdout if stdout is not None else self.log(),
                              reason="rendered" if ok else "renderer exited with status 3",
                              returncode=0 if ok else 3)
        return run

    def run(self, profile=None, runner=None, game_dir="default", **kw):
        out = self.root / "out"
        if out.exists():
            shutil.rmtree(out)
        return RC.render_cases(profile or self.profile, self.baseline, self.candidate,
                               out, game_dir=self.game if game_dir == "default" else game_dir,
                               runner=runner or self.runner(), **kw)


class RenderCasesTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.w = World(Path(self._tmp.name))

    def tearDown(self):
        self._tmp.cleanup()

    def by_id(self, results):
        return {r["id"]: r for r in results}

    # --- the one way to pass ---------------------------------------------------

    def test_exact_reproducible_render_from_pinned_inputs_passes(self):
        res = self.by_id(self.w.run())
        case = res["visual.core.act1_town"]
        self.assertEqual("pass", case["status"], case["failures"])
        self.assertEqual(case["expected_sha256"], case["actual_sha256"])
        self.assertEqual("pass", res["visual.representative"]["status"])
        self.assertEqual(2, self.w.calls)          # rendered twice
        self.assertEqual(0, RC.exit_code(list(res.values())))
        for key in ("schema", "lane", "platform", "required", "contract_digest",
                    "baseline_digest", "input_sha256", "provenance"):
            self.assertIn(key, case)

    # --- wrong input -------------------------------------------------------------

    def test_wrong_input_hash_is_unavailable_not_pass(self):
        p = copy.deepcopy(self.w.profile)
        p["cases"][0]["inputs"][0]["sha256"] = "0" * 64
        case = self.by_id(self.w.run(p))["visual.core.act1_town"]
        self.assertEqual("unavailable", case["status"])
        self.assertEqual("input_hash", case["failures"][0]["kind"])
        self.assertEqual(0, self.w.calls)          # never rendered

    def test_archive_bytes_not_the_baseline_are_unavailable(self):
        (self.w.game / "D2Data.mpq").write_bytes(b"a modded archive")
        case = self.by_id(self.w.run())["visual.core.act1_town"]
        self.assertEqual("unavailable", case["status"])
        self.assertEqual("archive", case["failures"][0]["kind"])

    def test_no_game_directory_is_unavailable(self):
        res = self.by_id(self.w.run(game_dir=self.w.root / "missing"))
        self.assertEqual("unavailable", res["visual.core.act1_town"]["status"])
        self.assertEqual("not_run", res["visual.representative"]["status"])

    def test_changed_reference_image_is_unavailable(self):
        Image.new("RGB", (8, 6), (0, 0, 0)).save(self.w.ref)
        case = self.by_id(self.w.run())["visual.core.act1_town"]
        self.assertEqual("unavailable", case["status"])
        self.assertEqual("reference_hash", case["failures"][0]["kind"])

    def test_baseline_the_profile_does_not_pin_is_refused(self):
        p = copy.deepcopy(self.w.profile)
        p["baseline"]["digest"] = "1" * 64
        with self.assertRaises(RC.ProfileError):
            self.w.run(p)

    # --- accidental overlay / preferences ------------------------------------------

    def test_read_from_a_mod_directory_fails(self):
        run = self.w.runner(stdout=self.w.log(source=r"D:\mods\pd2\data"))
        case = self.by_id(self.w.run(runner=run))["visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        self.assertEqual("overlay", case["failures"][0]["kind"])
        self.assertEqual("resource_selection", case["failures"][0]["stage"])

    def test_configured_mod_dir_fails_even_if_unused(self):
        run = self.w.runner(stdout=self.w.log(mod=r"D:\mods\pd2\data"))
        case = self.by_id(self.w.run(runner=run))["visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        self.assertIn("overlay", [f["kind"] for f in case["failures"]])

    def test_install_seeded_from_user_preferences_fails(self):
        run = self.w.runner(stdout=self.w.log(
            extra=["d2install: seeded from preferences <C:\\Diablo2>"]))
        case = self.by_id(self.w.run(runner=run))["visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        self.assertEqual("preferences", case["failures"][0]["kind"])

    def test_project_diablo_2_data_in_the_log_fails(self):
        run = self.w.runner(stdout=self.w.log(
            extra=["d2install: Project Diablo 2 data from <C:\\Diablo2\\ProjectD2> (use_pd2=auto)"]))
        case = self.by_id(self.w.run(runner=run))["visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        self.assertIn("game_data", [f["kind"] for f in case["failures"]])

    def test_owned_ini_pins_vanilla_data(self):
        self.assertRegex(RC.INI_TEMPLATE, r"(?m)^use_pd2\s*=\s*no\s*$")

    def test_profile_must_forbid_overlays_and_preferences(self):
        for key, value in (("mod_dir", r"D:\mods"), ("preferences", "user")):
            p = copy.deepcopy(self.w.profile)
            p["render"][key] = value
            with self.assertRaises(RC.ProfileError, msg=key):
                RC.validate_profile(p)

    def test_unpinned_read_and_pinned_input_not_read_fail(self):
        extra = ["want to read a dt1 from mpq : Data\\Global\\Tiles\\Act1\\Town\\Fence.dt1",
                 "ok (5 bytes), found in " + str(self.w.game / "D2Data.mpq")]
        run = self.w.runner(stdout=self.w.log(extra=extra))
        case = self.by_id(self.w.run(runner=run))["visual.core.act1_town"]
        self.assertEqual("unpinned_input", case["failures"][0]["kind"])

        # the profile pins three inputs but the renderer only reads two
        lines = self.w.log().splitlines()
        i = next(k for k, line in enumerate(lines) if "Pal.pl2" in line)
        stdout = "\n".join(lines[:i] + lines[i + 2:]) + "\n"
        case = self.by_id(self.w.run(runner=self.w.runner(stdout=stdout)))[
            "visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        self.assertIn("pinned_input_not_read", [f["kind"] for f in case["failures"]])

    # --- animation / frame / determinism -------------------------------------------

    def test_unpinned_animation_frame_seed_or_time_is_refused(self):
        for key in ("frame", "seed", "time", "gamma", "zoom"):
            for bad in (None, "", "unpinned"):
                p = copy.deepcopy(self.w.profile)
                p["render"][key] = bad
                with self.assertRaises(RC.ProfileError, msg=f"{key}={bad!r}"):
                    RC.validate_profile(p)
            p = copy.deepcopy(self.w.profile)
            del p["render"][key]
            with self.assertRaises(RC.ProfileError):
                RC.validate_profile(p)

    def test_single_render_is_not_enough(self):
        p = copy.deepcopy(self.w.profile)
        p["render"]["repeat"] = 1
        with self.assertRaises(RC.ProfileError):
            RC.validate_profile(p)

    def test_renders_that_differ_between_runs_fail(self):
        case = self.by_id(self.w.run(runner=self.w.runner(vary=True)))[
            "visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        self.assertEqual("nondeterministic", case["failures"][0]["kind"])

    def test_tolerance_needs_a_named_rule(self):
        p = copy.deepcopy(self.w.profile)
        p["cases"][0]["compare"] = "tolerance-4"
        with self.assertRaises(RC.ProfileError):
            RC.validate_profile(p)

    # --- the comparison itself ---------------------------------------------------------

    def test_one_pixel_difference_is_a_mismatch_with_its_location(self):
        img = self.w.image.copy()
        img.putpixel((5, 4), (10, 20, 31))
        case = self.by_id(self.w.run(runner=self.w.runner(image=img)))[
            "visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        f = case["failures"][0]
        self.assertEqual("final_rgb", f["stage"])
        self.assertEqual(1, f["detail"]["diff_pixels"])
        self.assertEqual(1, f["detail"]["max_channel_diff"])
        self.assertEqual([5, 4, 6, 5], f["detail"]["bbox"])
        self.assertEqual(1, RC.exit_code(self.w.run(runner=self.w.runner(image=img))))

    def test_renderer_failure_is_a_process_mismatch(self):
        case = self.by_id(self.w.run(runner=self.w.runner(ok=False)))[
            "visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        self.assertEqual("process", case["failures"][0]["stage"])

    def test_runner_exception_never_passes(self):
        def boom(*_):
            raise RuntimeError("crashed")
        case = self.by_id(self.w.run(runner=boom))["visual.core.act1_town"]
        self.assertEqual("mismatch", case["status"])
        self.assertEqual("internal", case["failures"][0]["kind"])

    # --- coverage --------------------------------------------------------------------

    def test_missing_case_feature_coverage_is_partial(self):
        p = copy.deepcopy(self.w.profile)
        p["required_features"].append("dc6")
        agg = self.by_id(self.w.run(p))["visual.representative"]
        self.assertEqual("partial", agg["status"])
        self.assertIn({"kind": "missing_feature_coverage", "path": "dc6",
                       "expected": "a case", "actual": None}, agg["failures"])
        self.assertEqual(2, RC.exit_code(self.w.run(p)))

    def test_external_feature_counts_only_with_a_passing_result(self):
        p = copy.deepcopy(self.w.profile)
        p["required_features"].append("dc6")
        p["external_features"] = {"dc6": ["ctest.dc6_decode"]}
        self.assertEqual("partial",
                         self.by_id(self.w.run(p))["visual.representative"]["status"])
        ext = [{"id": "ctest.dc6_decode", "status": "pass", "success_outcome": "pass"}]
        self.assertEqual("pass", self.by_id(self.w.run(p, external_results=ext))[
            "visual.representative"]["status"])
        bad = [{"id": "ctest.dc6_decode", "status": "mismatch", "success_outcome": "pass"}]
        self.assertEqual("partial", self.by_id(self.w.run(p, external_results=bad))[
            "visual.representative"]["status"])

    def test_all_selected_but_unexecuted_is_not_run(self):
        res = self.by_id(self.w.run(only=["something.else"]))
        self.assertEqual("not_run", res["visual.core.act1_town"]["status"])
        self.assertEqual("not_run", res["visual.representative"]["status"])
        self.assertEqual(0, self.w.calls)
        self.assertEqual(2, RC.exit_code(list(res.values())))

    def test_empty_profile_is_refused(self):
        p = copy.deepcopy(self.w.profile)
        p["cases"] = []
        with self.assertRaises(RC.ProfileError):
            RC.validate_profile(p)

    def test_results_are_written(self):
        self.w.run()
        doc = json.loads((self.w.root / "out" / "results.json").read_text("utf-8"))
        self.assertEqual(["visual.core.act1_town", "visual.representative"],
                         [d["id"] for d in doc])


class ResourceLogTest(unittest.TestCase):
    def test_parse_pairs_requests_with_their_source(self):
        log = ("want to read a dt1 : assets\\editor\\ds1edit.dt1\n"
               "ok (185257 bytes), found\n"
               "want to read a dt1 from mpq : Data\\Global\\Tiles\\Act1\\Town\\Floor.dt1\n"
               "ok (1008003 bytes), found in F:\\g\\d2data.mpq\n"
               "reading ds1 : assets/tiles/ACT1/TOWN/townE1.ds1\n"
               "ok (57691 bytes), found in F:\\g\\d2data.mpq\n"
               "want to read obj.txt from assets\\editor\\\n"
               "ok (78025 bytes), found in .\n")
        reads = RC.parse_resource_reads(log)
        self.assertEqual(["Data\\Global\\Tiles\\Act1\\Town\\Floor.dt1",
                          "assets/tiles/ACT1/TOWN/townE1.ds1"],
                         [r["name"] for r in reads])
        self.assertEqual(1008003, reads[0]["size"])

    def test_names_map_to_baseline_keys(self):
        self.assertEqual("data\\global\\tiles\\act1\\town\\towne1.ds1",
                         RC.baseline_asset_key("assets/tiles/ACT1/TOWN/townE1.ds1"))
        self.assertEqual("data\\global\\palette\\act1\\pal.pl2",
                         RC.baseline_asset_key("Data\\Global\\Palette\\Act1\\Pal.pl2"))


class CommittedProfileTest(unittest.TestCase):
    def test_visual_core_profile_is_valid_and_maps_the_release_cases(self):
        profile = json.loads(PROFILE_PATH.read_text("utf-8"))
        RC.validate_profile(profile)
        ids = {c["id"] for c in profile["cases"]}
        release = json.loads((REPO_ROOT / "test" / "equivalence" / "profiles" /
                              "release-core.json").read_text("utf-8"))
        r5 = next(r for r in release["requirements"] if r["id"] == "R5")
        wanted = {c["id"] for c in r5["cases"] if c["id"].startswith("visual.core.")}
        self.assertEqual(wanted, ids)
        rep = next(c for c in r5["cases"] if c["id"] == "visual.representative")
        self.assertEqual(profile["aggregate_case"], rep["id"])
        self.assertEqual(set(rep["coverage"]["features"]),
                         set(profile["required_features"]))
        acts = {c["act"] for c in profile["cases"]}
        self.assertEqual({1, 2, 3, 4, 5}, acts)
        for c in profile["cases"]:
            self.assertTrue(any(i["path"].endswith(".ds1") for i in c["inputs"]), c["id"])
            self.assertTrue(any(i["path"].endswith("pal.pl2") for i in c["inputs"]), c["id"])
        covered = {f for c in profile["cases"] for f in c["features"]}
        covered |= set(profile["external_features"])
        self.assertEqual(set(profile["required_features"]), covered)


if __name__ == "__main__":
    unittest.main()
