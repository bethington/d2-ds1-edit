"""Tests for scripts/perf/fps_test.py.

Fake runners, fake reports and fake profiler processes only: no editor build,
no game data, no display.
"""

import json
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.perf import fps_test as ft  # noqa: E402

HOST = {"host_class": "Windows|AMD64|cpu", "gpu": "GPU A", "cpu": "cpu",
        "os_build": "1", "node": "n"}
BUILD = "2.1.1 (Release)"


def report(render_avg=2.0, mem=0, font=False, height=1080, build=BUILD):
    return {"schema": ft.SCHEMA, "build": build, "driver": "Direct3D",
            "window": {"width": height * 16 // 9, "height": height},
            "frames": 60, "wall_seconds": 1.0, "wall_fps": 60.0,
            "render_ms": {"avg": render_avg, "p50": render_avg, "p95": render_avg,
                          "p99": render_avg, "max": render_avg},
            "present_ms": {"avg": 1.0, "max": 2.0},
            "slowest_section": {"name": "render", "avg_ms": render_avg},
            "memory_bitmap_draws": mem, "font_memory_bitmap": font,
            "ok": mem == 0, "failure": None}


def fake_runner(rep=None, rc=0, write=True, stdout="selftest: shutdown 50 ms\n"):
    def runner(command, cwd, env, timeout_s):
        if write and rep is not None:
            path = [a for a in command if a.startswith("--selftest-report=")][0]
            Path(path.split("=", 1)[1]).write_text(json.dumps(rep))
        return rc, stdout, ""
    return runner


class RunOnceTests(unittest.TestCase):
    def run_case(self, runner):
        with tempfile.TemporaryDirectory() as d:
            run_dir = Path(d)
            rt = run_dir / "rt"
            rt.mkdir()
            return ft.run_once(run_dir, rt, "ds1edit.exe", "act1_town", 60, 5, runner)

    def test_all_good(self):
        r = self.run_case(fake_runner(report()))
        self.assertEqual(r["problems"], [])
        self.assertEqual(r["shutdown_ms"], 50)

    def test_missing_report_is_a_problem(self):
        r = self.run_case(fake_runner(report(), write=False))
        self.assertIn("no report written", r["problems"])

    def test_nonzero_exit(self):
        r = self.run_case(fake_runner(report(), rc=3))
        self.assertIn("exit code 3", r["problems"])

    def test_timeout(self):
        r = self.run_case(fake_runner(None, rc=None, write=False))
        self.assertTrue(r["timed_out"])
        self.assertTrue(any("timed out" in p for p in r["problems"]))

    def test_memory_bitmap_draws(self):
        r = self.run_case(fake_runner(report(mem=4)))
        self.assertTrue(any("memory-bitmap" in p for p in r["problems"]))

    def test_memory_font(self):
        r = self.run_case(fake_runner(report(font=True)))
        self.assertTrue(any("font" in p for p in r["problems"]))

    def test_corrupt_report(self):
        def runner(command, cwd, env, timeout_s):
            path = [a for a in command if a.startswith("--selftest-report=")][0]
            Path(path.split("=", 1)[1]).write_text("{not json")
            return 0, "", ""
        r = self.run_case(runner)
        self.assertTrue(any("unreadable" in p for p in r["problems"]))


def runs_of(*reports, problems=None):
    return [{"report": r, "problems": list(problems or []), "exit_code": 0,
             "shutdown_ms": 40} for r in reports]


class JudgeTests(unittest.TestCase):
    def test_all_pass(self):
        c = ft.judge_case("act1_town", "opengl", (1920, 1080),
                          runs_of(report(5.0), report(6.0), report(5.5)))
        self.assertEqual(c["status"], "PASS")
        self.assertAlmostEqual(c["summary"]["render_avg_ms"], 5.5)

    def test_threshold_breach_at_1080p(self):
        c = ft.judge_case("act1_town", "opengl", (1920, 1080),
                          runs_of(report(20.0), report(21.0), report(19.0)))
        self.assertEqual(c["status"], "FAIL")
        self.assertTrue(any("16.7" in p for p in c["problems"]))

    def test_threshold_not_applied_below_1080p(self):
        c = ft.judge_case("act1_town", "opengl", (1280, 720),
                          runs_of(report(20.0, height=720)))
        self.assertEqual(c["status"], "PASS")

    def test_no_reports_is_a_failure_not_a_pass(self):
        c = ft.judge_case("act1_town", "opengl", (1280, 720),
                          runs_of(None, None))
        self.assertEqual(c["status"], "FAIL")

    def test_run_problem_fails_the_case(self):
        c = ft.judge_case("a", "d", (1280, 720),
                          runs_of(report(), problems=["exit code 1"]))
        self.assertEqual(c["status"], "FAIL")


def baseline(reviewed=True, host=None, build=BUILD, render=10.0):
    return {"reviewed": reviewed, "host": host or dict(HOST),
            "cases": {"a|d|1920x1080": {"build": build, "render_avg_ms": render}}}


def case(render):
    c = ft.judge_case("a", "d", (1920, 1080), runs_of(report(render)))
    return c


class BaselineTests(unittest.TestCase):
    def test_no_baseline(self):
        c = case(5.0)
        head = ft.compare_baseline([c], None, HOST)
        self.assertFalse(head["present"])
        self.assertEqual(c["baseline"], "none")

    def test_regression_fails_when_comparable(self):
        c = case(14.0)   # +40% over 10.0, still under the 16.7 budget
        ft.compare_baseline([c], baseline(), HOST)
        self.assertEqual(c["baseline"], "regressed")
        self.assertEqual(c["status"], "FAIL")

    def test_within_tolerance_is_ok(self):
        c = case(12.0)
        ft.compare_baseline([c], baseline(), HOST)
        self.assertEqual(c["baseline"], "ok")
        self.assertEqual(c["status"], "PASS")

    def test_different_gpu_is_incomparable_not_fail(self):
        c = case(15.0)
        other = dict(HOST, gpu="GPU B")
        ft.compare_baseline([c], baseline(host=other), HOST)
        self.assertEqual(c["baseline"], "incomparable")
        self.assertEqual(c["status"], "PASS")

    def test_different_build_is_incomparable(self):
        c = case(15.0)
        ft.compare_baseline([c], baseline(build="2.0.0 (Debug)"), HOST)
        self.assertEqual(c["baseline"], "incomparable")

    def test_unreviewed_baseline_never_fails(self):
        c = case(15.0)
        ft.compare_baseline([c], baseline(reviewed=False), HOST)
        self.assertEqual(c["baseline"], "unreviewed")
        self.assertEqual(c["status"], "PASS")

    def test_missing_entry(self):
        c = ft.judge_case("zzz", "d", (1920, 1080), runs_of(report(5.0)))
        ft.compare_baseline([c], baseline(), HOST)
        self.assertEqual(c["baseline"], "no-entry")


class MatrixTests(unittest.TestCase):
    def args(self, d, exe):
        return types.SimpleNamespace(
            exe=str(exe), game_dir="G", maps="act1_town", drivers="opengl",
            frames=30, runs=2, size="1920x1080", timeout=5, out_dir=str(Path(d) / "out"),
            assets_dir=str(Path(d) / "assets"), baseline=str(Path(d) / "none.json"),
            profile=False, profile_on_fail=False, symbols_dir=None,
            write_baseline=None)

    def setup_exe(self, d):
        exe = Path(d) / "bin" / "ds1edit.exe"
        exe.parent.mkdir()
        exe.write_bytes(b"MZ")
        (Path(d) / "assets").mkdir()
        (Path(d) / "assets" / "act1_town.ini").write_text("x")
        return exe

    def test_all_pass_writes_summary(self):
        with tempfile.TemporaryDirectory() as d:
            exe = self.setup_exe(d)
            summary, table, out = ft.run_matrix(self.args(d, exe),
                                                runner=fake_runner(report(3.0)), host=HOST)
            self.assertEqual(summary["verdict"], "PASS")
            data = json.loads((out / "summary.json").read_text())
            self.assertEqual(data["schema"], ft.SUMMARY_SCHEMA)
            self.assertIn("act1_town|opengl|1920x1080", table)
            ini = (out / "runs" / "act1_town-opengl-1920x1080-r1" / "rt" / "Ds1edit.ini").read_text()
            self.assertIn("full_screen = NO", ini)
            self.assertIn("use_pd2 = no", ini)   # never auto-detect PD2 into a baseline
            cfg = (out / "runs" / "act1_town-opengl-1920x1080-r1" / "rt" / "allegro5.cfg").read_text()
            self.assertIn("driver=opengl", cfg)

    def test_profile_on_fail_only_profiles_failures(self):
        calls = []
        def profiler(cmd, rt, run_dir, env, timeout, symbols):
            calls.append(cmd)
            return {"status": "unavailable", "message": "fake"}
        with tempfile.TemporaryDirectory() as d:
            exe = self.setup_exe(d)
            a = self.args(d, exe)
            a.profile_on_fail = True
            ok, _, _ = ft.run_matrix(a, runner=fake_runner(report(3.0)),
                                     profiler=profiler, host=HOST)
            self.assertEqual(calls, [])
            bad, _, _ = ft.run_matrix(a, runner=fake_runner(report(30.0)),
                                      profiler=profiler, host=HOST)
            self.assertEqual(len(calls), 1)
            self.assertEqual(bad["cases"][0]["profile"]["status"], "unavailable")
            self.assertEqual(bad["verdict"], "FAIL")

    def test_write_baseline_is_unreviewed(self):
        with tempfile.TemporaryDirectory() as d:
            exe = self.setup_exe(d)
            summary, _, _ = ft.run_matrix(self.args(d, exe),
                                          runner=fake_runner(report(3.0)), host=HOST)
            path = Path(d) / "b.json"
            ft.write_baseline(path, summary)
            data = json.loads(path.read_text())
            self.assertIs(data["reviewed"], False)
            self.assertIn("act1_town|opengl|1920x1080", data["cases"])


class FakeRun:
    """Stand-in for subprocess.run keyed on the tool name."""

    def __init__(self, table):
        self.table = table
        self.calls = []

    def __call__(self, cmd, **kw):
        self.calls.append(cmd)
        name = Path(cmd[0]).stem.lower()
        verb = cmd[1] if len(cmd) > 1 else ""
        handler = self.table.get(name) or self.table.get((name, verb))
        if handler is None:
            raise FileNotFoundError(cmd[0])
        return handler(cmd)


def proc(rc=0, out="", err=""):
    return subprocess.CompletedProcess([], rc, out, err)


@unittest.skipUnless(sys.platform == "win32",
                     "applies-to=windows-x86: WPR path is Windows-only")
class WprProfileTests(unittest.TestCase):
    def go(self, which, run):
        with tempfile.TemporaryDirectory() as d:
            res = ft.profile_case(["ds1edit.exe"], Path(d), Path(d), {}, 5, None,
                                  which=which, run=run)
            hot = Path(d) / "hotspots.txt"
            return res, hot.read_text() if hot.exists() else None

    def test_wpr_missing(self):
        saved = ft.WPT_DIR
        ft.WPT_DIR = str(Path(tempfile.gettempdir()) / "no-such-wpt-dir")
        try:
            run = FakeRun({})
            res, _ = self.go(lambda n: None, run)
        finally:
            ft.WPT_DIR = saved
        self.assertEqual(res["status"], "unavailable")
        self.assertIn("wpr.exe not found", res["message"])
        self.assertEqual(run.calls, [])

    def test_wpr_refusing(self):
        run = FakeRun({"wpr": lambda c: proc(1, "", "Access is denied (elevation)")})
        res, _ = self.go(lambda n: "C:/x/" + n + ".exe", run)
        self.assertEqual(res["status"], "unavailable")
        self.assertIn("refused", res["message"])
        self.assertIn("elevated", res["message"])
        self.assertEqual(len(run.calls), 1, "must not run the case when wpr refuses")

    def test_success_ranks_hotspots(self):
        text = ("ds1edit.exe!wpreview_draw_tiles 1\nds1edit.exe!wpreview_draw_tiles 2\n"
                "nvoglv32.dll!glDrawElements 3\n")
        run = FakeRun({"wpr": lambda c: proc(0), "ds1edit": lambda c: proc(0),
                       "xperf": lambda c: proc(0, text)})
        res, hot = self.go(lambda n: "C:/x/" + n + ".exe", run)
        self.assertEqual(res["status"], "ok")
        self.assertTrue(res["trace"].endswith("trace.etl"))
        self.assertIn("ds1edit.exe!wpreview_draw_tiles", hot.splitlines()[2])
        self.assertIn("nvoglv32.dll!glDrawElements", hot)

    def test_stop_failure_is_reported(self):
        def wpr(c):
            return proc(1, "", "stop failed") if c[1] == "-stop" else proc(0)
        run = FakeRun({"wpr": wpr, "ds1edit": lambda c: proc(0)})
        res, _ = self.go(lambda n: "C:/x/" + n + ".exe", run)
        self.assertEqual(res["status"], "failed")


class HotspotParseTests(unittest.TestCase):
    def test_perf_report_lines(self):
        text = ("  40.00%  ds1edit  ds1edit  [.] wpreview_draw_tiles\n"
                "  10.00%  ds1edit  libGL    [.] glDrawElements\n")
        rows = ft.top_hotspots_from_text(text)
        self.assertEqual(rows[0], ("wpreview_draw_tiles", 40.0))

    def test_empty(self):
        self.assertEqual(ft.top_hotspots_from_text(""), [])


class SizeTests(unittest.TestCase):
    def test_parse(self):
        self.assertEqual(ft.parse_size("1920x1080"), (1920, 1080))
        with self.assertRaises(ValueError):
            ft.parse_size("big")


if __name__ == "__main__":
    unittest.main()
