"""
Unit tests for scripts/run_golden_tests.py.

These never launch the real editor and never read or write bin/, user INIs or
test/golden/. Every renderer is a mocked subprocess.run whose side effect
writes an authored image; every fixture lives in a temporary directory whose
path deliberately contains spaces.

Run from the repository root:
    python -m unittest -v test.python.test_golden_runner
or:
    python -m unittest discover -s test/python -p test_golden_runner.py -v
"""

import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from PIL import Image  # noqa: E402

from scripts import run_golden_tests as runner  # noqa: E402

RUN = "scripts.run_golden_tests.subprocess.run"


def write_image(path, color=(10, 20, 30), size=(2, 2), fmt=None):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new("RGB", size, color).save(path, fmt)
    return path


def fake_renderer(returncode=0, write=True, color=(10, 20, 30), payload=None,
                  stdout="render log", stderr="render errors"):
    """subprocess.run side effect standing in for ds1edit --headless.

    The output path is the last element of the argument list, exactly as the
    real command line passes it.
    """
    def run(cmd, **kwargs):
        out = Path(cmd[-1])
        if payload is not None:
            out.write_bytes(payload)
        elif write:
            write_image(out, color, fmt="BMP")
        return subprocess.CompletedProcess(cmd, returncode, stdout, stderr)
    return run


class TempDirCase(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory(prefix="golden runner ")
        self.root = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)
        self.bin = self.root / "bin dir"
        self.bin.mkdir()
        self.exe = self.bin / "ds1 edit.exe"
        self.exe.write_bytes(b"not really an executable")
        self.out_dir = self.root / "out dir"
        self.out_dir.mkdir()


class RenderCaseTests(TempDirCase):
    """Task 1: a render only succeeds with exit 0 and a fresh, valid image."""

    def render(self, output=None, **kwargs):
        output = output or self.out_dir / "actual.bmp"
        return runner.render_case(self.exe, self.bin, ("1", "2"),
                                  "assets/tiles/ACT1/TOWN/townE1.ds1",
                                  output, **kwargs)

    def test_nonzero_exit_with_nonempty_output_fails(self):
        with mock.patch(RUN, side_effect=fake_renderer(returncode=3)) as run:
            result = self.render()
        run.assert_called_once()
        self.assertFalse(result.ok)
        self.assertEqual(result.returncode, 3)
        self.assertIn("exit", result.reason)
        self.assertIn("3", result.reason)
        self.assertEqual(result.stdout, "render log")
        self.assertEqual(result.stderr, "render errors")

    def test_compat_wrapper_rejects_nonzero_exit_with_output(self):
        # The reproduced false success: the old render_headless() ignored the
        # exit status and accepted any nonempty file at the output path.
        output = self.out_dir / "legacy.bmp"
        with mock.patch.object(runner, "EXE", str(self.exe)), \
                mock.patch.object(runner, "BIN_DIR", str(self.bin)), \
                mock.patch(RUN, side_effect=fake_renderer(returncode=1)):
            ok = runner.render_headless("1", "2", "x.ds1", str(output))
        self.assertFalse(ok)

    def test_zero_exit_without_output_fails(self):
        with mock.patch(RUN, side_effect=fake_renderer(write=False)):
            result = self.render()
        self.assertFalse(result.ok)
        self.assertEqual(result.returncode, 0)
        self.assertIn("no output", result.reason)
        self.assertEqual(result.stdout, "render log")

    def test_zero_exit_with_empty_output_fails(self):
        with mock.patch(RUN, side_effect=fake_renderer(payload=b"")):
            result = self.render()
        self.assertFalse(result.ok)
        self.assertIn("empty", result.reason)

    def test_stale_output_is_refused(self):
        output = write_image(self.out_dir / "actual.bmp", fmt="BMP")
        before = output.read_bytes()
        with mock.patch(RUN, side_effect=fake_renderer()) as run:
            result = self.render(output)
        run.assert_not_called()
        self.assertFalse(result.ok)
        self.assertIsNone(result.returncode)
        self.assertIn("already exists", result.reason)
        # A caller's file is refused, never deleted or rewritten.
        self.assertEqual(output.read_bytes(), before)

    def test_invalid_image_fails(self):
        with mock.patch(RUN, side_effect=fake_renderer(payload=b"BMnot an image")):
            result = self.render()
        self.assertFalse(result.ok)
        self.assertEqual(result.returncode, 0)
        self.assertIn("invalid image", result.reason)
        self.assertEqual(result.stderr, "render errors")

    def test_truncated_image_fails(self):
        good = write_image(self.root / "good.bmp", size=(16, 16), fmt="BMP")
        truncated = good.read_bytes()[:-40]
        with mock.patch(RUN, side_effect=fake_renderer(payload=truncated)):
            result = self.render()
        self.assertFalse(result.ok)
        self.assertIn("invalid image", result.reason)

    def test_timeout_and_oserror_are_reported(self):
        timeout = subprocess.TimeoutExpired(["x"], 7, output=b"partial out",
                                            stderr=b"partial err")
        with mock.patch(RUN, side_effect=timeout):
            result = self.render(timeout_s=7)
        self.assertFalse(result.ok)
        self.assertIsNone(result.returncode)
        self.assertIn("timed out", result.reason)
        self.assertIn("7", result.reason)
        self.assertEqual(result.stdout, "partial out")
        self.assertEqual(result.stderr, "partial err")

        with mock.patch(RUN, side_effect=PermissionError(13, "Access is denied")):
            result = self.render(self.out_dir / "second.bmp")
        self.assertFalse(result.ok)
        self.assertIsNone(result.returncode)
        self.assertIn("could not launch", result.reason)
        self.assertIn("Access is denied", result.reason)

    def test_missing_executable_is_reported_without_launch(self):
        missing = self.bin / "missing.exe"
        with mock.patch(RUN, side_effect=fake_renderer()) as run:
            result = runner.render_case(missing, self.bin, ("1", "2"), "x.ds1",
                                        self.out_dir / "actual.bmp")
        run.assert_not_called()
        self.assertFalse(result.ok)
        self.assertIn("executable not found", result.reason)
        self.assertIn(str(missing), result.reason)

    def test_valid_fresh_image_passes(self):
        with mock.patch(RUN, side_effect=fake_renderer()):
            result = self.render()
        self.assertTrue(result.ok, result.reason)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "render log")
        self.assertEqual(result.stderr, "render errors")

    def test_executable_and_output_paths_with_spaces(self):
        output = self.out_dir / "my map.bmp"
        ds1 = "assets/tiles/ACT1/TOWN/town E1.ds1"
        with mock.patch(RUN, side_effect=fake_renderer()) as run:
            result = runner.render_case(self.exe, self.bin, ("17", "42"), ds1,
                                        output, timeout_s=33)
        self.assertTrue(result.ok, result.reason)
        args, kwargs = run.call_args
        cmd = args[0] if args else kwargs["args"]
        # An argument list, never a shell string: spaces survive intact.
        self.assertIsInstance(cmd, list)
        self.assertEqual(cmd[0], str(self.exe))
        self.assertEqual(cmd[1:4], [ds1, "17", "42"])
        self.assertIn("--zoom=" + runner.GOLDEN_ZOOM, cmd)
        self.assertIn("--headless", cmd)
        self.assertEqual(cmd[-1], str(output))
        self.assertFalse(kwargs.get("shell", False))
        self.assertEqual(Path(kwargs["cwd"]), self.bin)
        self.assertEqual(kwargs["timeout"], 33)


class SuiteCase(TempDirCase):
    """Fixture layout: assets/<name>.ini, golden/<name>.png, a fake exe."""

    def setUp(self):
        super().setUp()
        self.assets = self.root / "assets dir"
        self.assets.mkdir()
        self.golden = self.root / "golden dir"
        self.golden.mkdir()
        self.artifacts = self.root / "artifact runs"
        self.summaries = []

    def add_map(self, name, ini="1 2 assets/tiles/ACT1/TOWN/townE1.ds1\n",
                golden_color=(10, 20, 30)):
        if ini is not None:
            (self.assets / f"{name}.ini").write_text(ini)
        if golden_color is not None:
            write_image(self.golden / f"{name}.png", golden_color)

    def argv(self, *extra):
        return ["--exe", str(self.exe), "--assets-dir", str(self.assets),
                "--golden-dir", str(self.golden),
                "--artifacts-dir", str(self.artifacts), *extra]

    def run_main(self, *extra, renderer=None, argv=None):
        """Run main() quietly with a mocked renderer; returns (code, run mock).
        Each run_suite() summary is appended to self.summaries."""
        real = runner.run_suite

        def spy(names, options):
            summary = real(names, options)
            self.summaries.append(summary)
            return summary

        out = io.StringIO()
        with mock.patch(RUN, side_effect=renderer or fake_renderer()) as run, \
                mock.patch.object(runner, "run_suite", spy), \
                contextlib.redirect_stdout(out), \
                contextlib.redirect_stderr(out):
            code = runner.main(self.argv(*extra) if argv is None else argv)
        self.output = out.getvalue()
        return code, run

    @property
    def summary(self):
        self.assertEqual(len(self.summaries), 1)
        return self.summaries[0]


class SuiteSelectionTests(SuiteCase):
    """Task 2: only a nonempty, completely successful selection passes."""

    def test_all_missing_references_fails(self):
        # The reproduced false success: every core case skipped, exit 0.
        for name in runner.CORE_MAPS:
            self.add_map(name, golden_color=None)
        code, run = self.run_main("--core")
        self.assertEqual(code, 1)
        run.assert_not_called()
        s = self.summary
        self.assertEqual(s["selected"], len(runner.CORE_MAPS))
        self.assertEqual(s["skipped"], len(runner.CORE_MAPS))
        self.assertEqual(s["passed"], 0)
        for case in s["cases"]:
            self.assertEqual(case["status"], "skipped")
            self.assertIn("no golden reference", case["reason"])

    def test_one_missing_ini_among_passes_fails(self):
        self.add_map("m1")
        self.add_map("m2")
        self.add_map("m3", ini=None)
        code, run = self.run_main("m1", "m2", "m3")
        self.assertEqual(code, 1)
        self.assertEqual(run.call_count, 2)
        s = self.summary
        self.assertEqual((s["selected"], s["passed"], s["failed"], s["skipped"]),
                         (3, 2, 0, 1))
        by_id = {c["id"]: c for c in s["cases"]}
        self.assertEqual(by_id["m3"]["status"], "skipped")
        self.assertIn("no INI", by_id["m3"]["reason"])

    def test_invalid_ini_fails(self):
        self.add_map("m1")
        self.add_map("empty", ini="# only a comment\n\n; another\n")
        code, run = self.run_main("m1", "empty")
        self.assertEqual(code, 1)
        self.assertEqual(run.call_count, 1)
        case = self.summary["cases"][1]
        self.assertEqual(case["status"], "failed")
        self.assertIn("no entries", case["reason"])

    def test_malformed_ini_numeric_ids(self):
        self.add_map("letters", ini="one 2 assets/x.ds1\n")
        self.add_map("negative", ini="1 -2 assets/x.ds1\n")
        self.add_map("nopath", ini="1 2\n")
        code, run = self.run_main("letters", "negative", "nopath")
        self.assertEqual(code, 1)
        run.assert_not_called()
        for case in self.summary["cases"]:
            self.assertEqual(case["status"], "failed", case)
            self.assertIn("invalid INI", case["reason"])

    def test_ini_path_with_spaces_is_passed_intact(self):
        ds1 = "assets/tiles/ACT1/My Town/town E1.ds1"
        self.add_map("spaced", ini=f"# header\n  7   9   {ds1}  \n1 1 other.ds1\n")
        code, run = self.run_main("spaced")
        self.assertEqual(code, 0, self.output)
        cmd = run.call_args[0][0]
        self.assertEqual(cmd[1:4], [ds1, "7", "9"])

    def test_empty_full_selection_is_a_configuration_error(self):
        code, run = self.run_main("--full")
        self.assertEqual(code, 2)
        run.assert_not_called()
        self.assertEqual(self.summaries, [])
        self.assertIn("no maps", self.output)

    def test_missing_assets_dir_is_a_configuration_error(self):
        self.add_map("m1")
        argv = ["--exe", str(self.exe), "--assets-dir", str(self.root / "nope"),
                "--golden-dir", str(self.golden),
                "--artifacts-dir", str(self.artifacts), "m1"]
        code, run = self.run_main(argv=argv)
        self.assertEqual(code, 2)
        run.assert_not_called()
        self.assertIn("assets directory", self.output)

    def test_missing_golden_dir_is_a_configuration_error(self):
        self.add_map("m1")
        argv = ["--exe", str(self.exe), "--assets-dir", str(self.assets),
                "--golden-dir", str(self.root / "nope"),
                "--artifacts-dir", str(self.artifacts), "m1"]
        code, run = self.run_main(argv=argv)
        self.assertEqual(code, 2)
        run.assert_not_called()
        self.assertIn("golden directory", self.output)

    def test_missing_executable_is_a_configuration_error(self):
        self.add_map("m1")
        self.exe.unlink()
        code, run = self.run_main("m1")
        self.assertEqual(code, 2)
        run.assert_not_called()
        self.assertIn("executable not found", self.output)

    def test_duplicate_requested_names_run_once_in_order(self):
        self.add_map("m1")
        self.add_map("m2")
        code, run = self.run_main("m2", "m1", "m2", "m1")
        self.assertEqual(code, 0, self.output)
        self.assertEqual(run.call_count, 2)
        s = self.summary
        self.assertEqual([c["id"] for c in s["cases"]], ["m2", "m1"])
        self.assertEqual((s["selected"], s["passed"]), (2, 2))

    def test_invalid_tolerance_is_rejected_before_launch(self):
        self.add_map("m1")
        for bad in ("-1", "256", "abc", "1.5"):
            with self.subTest(tolerance=bad):
                code, run = self.run_main("--tolerance", bad, "m1")
                self.assertEqual(code, 2)
                run.assert_not_called()
        self.assertEqual(self.summaries, [])

    def test_boundary_tolerances_are_accepted(self):
        self.add_map("m1")
        for good in ("0", "255"):
            with self.subTest(tolerance=good):
                code, _ = self.run_main("--tolerance", good, "m1")
                self.assertEqual(code, 0, self.output)

    def test_unknown_flag_is_rejected_before_launch(self):
        self.add_map("m1")
        code, run = self.run_main("--bogus", "m1")
        self.assertEqual(code, 2)
        run.assert_not_called()

    def test_incompatible_mode_flags_are_rejected(self):
        self.add_map("m1")
        for extra in (("--core", "--full"), ("--full", "m1"), ("--core", "m1")):
            with self.subTest(args=extra):
                code, run = self.run_main(*extra)
                self.assertEqual(code, 2)
                run.assert_not_called()

    def test_complete_matching_suite_passes(self):
        for name in runner.CORE_MAPS:
            self.add_map(name)
        code, run = self.run_main("--core")
        self.assertEqual(code, 0, self.output)
        self.assertEqual(run.call_count, len(runner.CORE_MAPS))
        s = self.summary
        self.assertEqual(s["schema"], "ds1edit.golden/1")
        self.assertEqual(s["selected"], len(runner.CORE_MAPS))
        self.assertEqual(s["passed"], len(runner.CORE_MAPS))
        self.assertEqual((s["failed"], s["skipped"]), (0, 0))
        for case in s["cases"]:
            self.assertEqual(case["status"], "passed")
            self.assertIn("artifacts", case)

    def test_default_selection_is_core(self):
        for name in runner.CORE_MAPS:
            self.add_map(name)
        self.add_map("extra")
        code, run = self.run_main()
        self.assertEqual(code, 0, self.output)
        self.assertEqual(run.call_count, len(runner.CORE_MAPS))

    def test_full_selects_every_ini_sorted(self):
        for name in ("b_map", "a_map"):
            self.add_map(name)
        code, _ = self.run_main("--full")
        self.assertEqual(code, 0, self.output)
        self.assertEqual([c["id"] for c in self.summary["cases"]],
                         ["a_map", "b_map"])

    def test_one_differing_pixel_at_tolerance_zero_fails(self):
        self.add_map("m1")
        # Reference differs from the render in exactly one pixel.
        ref = Image.new("RGB", (2, 2), (10, 20, 30))
        ref.putpixel((1, 1), (11, 20, 30))
        ref.save(self.golden / "m1.png")
        code, _ = self.run_main("--tolerance", "0", "m1")
        self.assertEqual(code, 1)
        s = self.summary
        self.assertEqual((s["selected"], s["passed"], s["failed"]), (1, 0, 1))
        self.assertIn("1/4 pixels differ", s["cases"][0]["reason"])

        self.summaries.clear()
        code, _ = self.run_main("--tolerance", "1", "m1")
        self.assertEqual(code, 0, self.output)

    def test_matching_single_image_passes(self):
        self.add_map("m1")
        code, _ = self.run_main("--tolerance", "0", "m1")
        self.assertEqual(code, 0, self.output)
        s = self.summary
        self.assertEqual((s["selected"], s["passed"]), (1, 1))

    def test_render_failure_fails_the_suite(self):
        self.add_map("m1")
        code, _ = self.run_main("m1", renderer=fake_renderer(returncode=1))
        self.assertEqual(code, 1)
        case = self.summary["cases"][0]
        self.assertEqual(case["status"], "failed")
        self.assertIn("status 1", case["reason"])

    def test_size_mismatch_fails(self):
        self.add_map("m1")
        write_image(self.golden / "m1.png", size=(3, 2))
        code, _ = self.run_main("m1")
        self.assertEqual(code, 1)
        self.assertIn("Size mismatch", self.summary["cases"][0]["reason"])

    def test_corrupt_reference_is_reported_per_case(self):
        self.add_map("bad")
        self.add_map("good")
        (self.golden / "bad.png").write_bytes(b"\x89PNG broken")
        code, run = self.run_main("bad", "good")
        self.assertEqual(code, 1)
        # An unreadable reference is detected before spending a render on it.
        self.assertEqual(run.call_count, 1)
        bad, good = self.summary["cases"]
        self.assertEqual(bad["status"], "failed")
        self.assertIn("reference", bad["reason"])
        self.assertEqual(good["status"], "passed")

    def test_unexpected_exception_is_reported_per_case(self):
        self.add_map("m1")
        self.add_map("m2")
        real_compare = runner.compare_images
        effects = [RuntimeError("boom"), real_compare]

        def flaky(ref, test, tol):
            effect = effects.pop(0)
            if isinstance(effect, Exception):
                raise effect
            return effect(ref, test, tol)

        with mock.patch.object(runner, "compare_images", side_effect=flaky):
            code, _ = self.run_main("m1", "m2")
        self.assertEqual(code, 1)
        first, second = self.summary["cases"]
        self.assertEqual(first["status"], "failed")
        self.assertIn("RuntimeError: boom", first["reason"])
        self.assertEqual(second["status"], "passed")

    def test_core_maps_and_zoom_are_preserved(self):
        self.assertEqual(runner.CORE_MAPS, [
            "act1_town", "act1_cave", "act2_desert",
            "act3_jungle", "act4_fortress", "act5_town"])
        self.assertEqual(runner.GOLDEN_ZOOM, "1:4")

    def test_default_executable_is_native(self):
        expected = "ds1edit.exe" if os.name == "nt" else "ds1edit"
        self.assertEqual(Path(runner.parse_args([]).exe).name, expected)


class ArtifactAndUpdateTests(SuiteCase):
    """Task 3: failures leave evidence behind; references change only on an
    explicit, fully successful --update."""

    def load_results(self, summary=None):
        summary = summary or self.summary
        run_dir = Path(summary["run_dir"])
        with open(run_dir / "results.json", encoding="utf-8") as f:
            return run_dir, json.load(f)

    def test_failed_run_artifacts_survive(self):
        self.add_map("crash")
        self.add_map("differs")
        write_image(self.golden / "differs.png", (200, 0, 0))
        renderers = {"crash": fake_renderer(returncode=5, stdout="crash out",
                                            stderr="crash err"),
                     "differs": fake_renderer(stdout="ok out", stderr="")}

        def by_map(cmd, **kwargs):
            name = Path(cmd[-1]).parent.name
            return renderers[name](cmd, **kwargs)

        code, _ = self.run_main("crash", "differs", renderer=by_map)
        self.assertEqual(code, 1)
        run_dir, results = self.load_results()
        self.assertTrue(run_dir.is_dir())
        self.assertTrue(run_dir.is_relative_to(self.artifacts))
        self.assertIn(str(run_dir), self.output)

        crash, differs = results["cases"]
        self.assertEqual(crash["status"], "failed")
        self.assertEqual(Path(crash["artifacts"]["stdout"]).read_text(), "crash out")
        self.assertEqual(Path(crash["artifacts"]["stderr"]).read_text(), "crash err")

        self.assertEqual(differs["status"], "failed")
        actual = Path(differs["artifacts"]["actual"])
        expected = Path(differs["artifacts"]["expected"])
        self.assertTrue(actual.is_file())
        self.assertTrue(actual.is_relative_to(run_dir))
        self.assertTrue(expected.is_relative_to(run_dir))
        self.assertEqual(expected.read_bytes(),
                         (self.golden / "differs.png").read_bytes())
        with Image.open(actual) as img:
            self.assertEqual(img.convert("RGB").getpixel((0, 0)), (10, 20, 30))
        self.assertEqual(Path(differs["artifacts"]["stdout"]).read_text(), "ok out")

    def test_passing_case_does_not_copy_expected(self):
        self.add_map("m1")
        code, _ = self.run_main("m1")
        self.assertEqual(code, 0, self.output)
        _, results = self.load_results()
        self.assertNotIn("expected", results["cases"][0]["artifacts"])

    def test_repeated_runs_keep_prior_evidence(self):
        self.add_map("m1")
        write_image(self.golden / "m1.png", (1, 2, 3))
        self.run_main("m1")
        self.run_main("m1")
        first, second = self.summaries
        self.assertNotEqual(first["run_dir"], second["run_dir"])
        run1, results1 = self.load_results(first)
        before = (run1 / "results.json").read_bytes()
        run2, _ = self.load_results(second)
        self.assertEqual((run1 / "results.json").read_bytes(), before)
        self.assertTrue(Path(results1["cases"][0]["artifacts"]["actual"]).is_file())
        self.assertEqual(len(list(self.artifacts.iterdir())), 2)

    def test_results_json_totals_match_cases(self):
        self.add_map("good")
        self.add_map("bad")
        write_image(self.golden / "bad.png", (0, 0, 0))
        self.add_map("noref", golden_color=None)
        code, _ = self.run_main("good", "bad", "noref")
        self.assertEqual(code, 1)
        _, results = self.load_results()
        self.assertEqual(results["schema"], "ds1edit.golden/1")
        statuses = [c["status"] for c in results["cases"]]
        self.assertEqual(results["selected"], len(results["cases"]))
        for key in ("passed", "failed", "skipped", "updated"):
            self.assertEqual(results[key], statuses.count(key), key)
        self.assertEqual(results["selected"],
                         results["passed"] + results["failed"]
                         + results["skipped"] + results["updated"])
        self.assertFalse(results["ok"])
        for case in results["cases"]:
            self.assertEqual(set(case) >= {"id", "status", "reason",
                                           "artifacts"}, True)

    def test_results_write_failure_returns_nonzero(self):
        self.add_map("m1")
        with mock.patch.object(runner, "_write_results",
                               side_effect=OSError(28, "No space left on device")):
            code, _ = self.run_main("m1")
        self.assertEqual(code, 1)
        self.assertIn("No space left", self.output)

    def test_unusable_artifacts_dir_is_rejected_before_launch(self):
        self.add_map("m1")
        self.artifacts.write_text("a file, not a directory")
        code, run = self.run_main("m1")
        self.assertEqual(code, 2)
        run.assert_not_called()

    def test_failed_update_preserves_reference(self):
        self.add_map("m1", golden_color=(1, 2, 3))
        before = (self.golden / "m1.png").read_bytes()
        code, _ = self.run_main("--update", "m1",
                                renderer=fake_renderer(returncode=1,
                                                       color=(9, 9, 9)))
        self.assertEqual(code, 1)
        self.assertEqual((self.golden / "m1.png").read_bytes(), before)
        self.assertEqual(sorted(p.name for p in self.golden.iterdir()), ["m1.png"])

    def test_update_failing_at_replace_preserves_reference(self):
        self.add_map("m1", golden_color=(1, 2, 3))
        before = (self.golden / "m1.png").read_bytes()
        with mock.patch.object(runner.os, "replace",
                               side_effect=OSError(13, "Access is denied")):
            code, _ = self.run_main("--update", "m1",
                                    renderer=fake_renderer(color=(9, 9, 9)))
        self.assertEqual(code, 1)
        self.assertEqual((self.golden / "m1.png").read_bytes(), before)
        # The temporary file is removed; nothing else is left in golden/.
        self.assertEqual(sorted(p.name for p in self.golden.iterdir()), ["m1.png"])
        case = self.summary["cases"][0]
        self.assertEqual(case["status"], "failed")
        self.assertIn("Access is denied", case["reason"])

    def test_successful_explicit_update(self):
        self.add_map("m1", golden_color=(1, 2, 3))
        self.add_map("new", golden_color=None)
        old_bytes = (self.golden / "m1.png").read_bytes()
        code, _ = self.run_main("--update", "m1", "new",
                                renderer=fake_renderer(color=(9, 8, 7)))
        self.assertEqual(code, 0, self.output)
        for name in ("m1", "new"):
            with Image.open(self.golden / f"{name}.png") as img:
                self.assertEqual(img.convert("RGB").getpixel((1, 1)), (9, 8, 7))
        self.assertEqual(sorted(p.name for p in self.golden.iterdir()),
                         ["m1.png", "new.png"])
        _, results = self.load_results()
        self.assertEqual(results["mode"], "update")
        self.assertEqual((results["selected"], results["updated"],
                          results["passed"]), (2, 2, 0))
        m1 = results["cases"][0]
        self.assertEqual(m1["status"], "updated")
        # Before/after evidence for review.
        self.assertEqual(Path(m1["artifacts"]["previous"]).read_bytes(), old_bytes)
        self.assertTrue(Path(m1["artifacts"]["actual"]).is_file())

    def test_mismatch_never_changes_golden(self):
        self.add_map("m1", golden_color=(1, 2, 3))
        before = (self.golden / "m1.png").read_bytes()
        code, _ = self.run_main("--tolerance", "255", "m1",
                                renderer=fake_renderer(color=(9, 9, 9)))
        self.assertEqual(code, 0)   # tolerance 255 accepts any colour
        code, _ = self.run_main("m1", renderer=fake_renderer(color=(9, 9, 9)))
        self.assertEqual(code, 1)
        self.assertEqual((self.golden / "m1.png").read_bytes(), before)
        self.assertEqual(sorted(p.name for p in self.golden.iterdir()), ["m1.png"])


if __name__ == "__main__":
    unittest.main()
