"""Tests for scripts/equivalence/benchmarks.py.

Real subprocesses (the running Python interpreter with tiny -c programs) and
fake observers; no game data and no editor build.
"""

import json
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.equivalence import benchmarks as bm  # noqa: E402

PY = sys.executable
FAST = [PY, "-c", "pass"]


class FakeSession:
    def __init__(self, parent):
        self.parent = parent

    def stop(self):
        return self.parent.next_metrics()


class FakeObserver:
    scope = "fake per-process peak"

    def __init__(self, series=None, available=True, reason="", returns_none=False):
        self.series = list(series or [])
        self._available = available
        self._reason = reason
        self.returns_none = returns_none
        self.calls = 0

    def available(self):
        return self._available, self._reason

    def start(self, proc):
        return FakeSession(self)

    def next_metrics(self):
        if self.returns_none:
            return None
        v = self.series[min(self.calls, len(self.series) - 1)]
        self.calls += 1
        return {"peak_working_set_bytes": v, "peak_pagefile_bytes": v + 10,
                "peak_handle_count": 50}


class Clock:
    """Deterministic clock: each reading advances 0.5 s per call pair."""

    def __init__(self):
        self.t = 0.0

    def __call__(self):
        self.t += 0.25
        return self.t


class SampleAccounting(unittest.TestCase):
    def test_counts_and_median(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 5, 2, FakeObserver([1000] * 20))
        self.assertEqual(rec["schema"], bm.SCHEMA)
        self.assertEqual(rec["status"], "pass")
        self.assertEqual(len(rec["samples"]), 5)
        self.assertEqual(len(rec["warmup_samples"]), 2)
        self.assertEqual(rec["cold_samples"], [])
        walls = sorted(s["wall_s"] for s in rec["samples"])
        self.assertEqual(rec["stats"]["n"], 5)
        self.assertEqual(rec["stats"]["median"], walls[2])
        self.assertEqual(rec["stats"]["min"], walls[0])
        self.assertEqual(rec["stats"]["max"], walls[-1])
        self.assertEqual(rec["comparison"]["status"], "unreviewed")

    def test_warmups_are_not_in_the_distribution_and_cold_is_separate(self):
        seen = []
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 3, 2, FakeObserver([1] * 20), cold_first=True,
                                      setup=lambda i, ph: seen.append(ph))
        self.assertEqual(seen, ["cold", "warmup", "warmup", "measured", "measured", "measured"])
        self.assertEqual(len(rec["cold_samples"]), 1)
        self.assertEqual(rec["stats"]["n"], 3)
        self.assertEqual(rec["cold_stats"]["n"], 1)
        self.assertIn("not flushed", rec["classification"]["cold"])

    def test_argument_validation(self):
        with tempfile.TemporaryDirectory() as d:
            for bad in (dict(command="python -c pass"), dict(command=[]), dict(repetitions=0),
                        dict(warmups=-1), dict(cwd=Path(d) / "nope")):
                kw = dict(command=FAST, cwd=d, repetitions=1, warmups=0)
                kw.update(bad)
                with self.assertRaises(bm.BenchmarkError):
                    bm.measure_workload(kw["command"], kw["cwd"], kw["repetitions"],
                                        kw["warmups"], None)

    def test_summarize_and_trend_helpers(self):
        s = bm.summarize([1.0, 2.0, 3.0, 4.0])
        self.assertEqual(s["median"], 2.5)
        self.assertIsNone(bm.summarize([]))
        self.assertIsNone(bm.summarize([5.0])["stdev"])


class ObserverHandling(unittest.TestCase):
    def test_unavailable_observer_is_not_zero_growth(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 3, 0, FakeObserver(available=False, reason="no psapi"))
        self.assertEqual(rec["status"], "partial")
        self.assertFalse(rec["resources"]["observed"])
        self.assertEqual(rec["resources"]["reason"], "no psapi")
        self.assertIsNone(rec["resources"]["series"])
        self.assertIsNone(rec["resources"]["trend"])
        self.assertIn("resource_observer_unavailable", rec["flags"])
        self.assertNotIn("growth", json.dumps(rec["resources"]))

    def test_no_observer_at_all(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 2, 0, None)
        self.assertEqual(rec["status"], "partial")
        self.assertFalse(rec["resources"]["observed"])

    def test_observer_that_returns_nothing_is_missing_not_zero(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 3, 0, FakeObserver(returns_none=True))
        self.assertEqual(rec["status"], "partial")
        self.assertFalse(rec["resources"]["observed"])
        self.assertIn("resource_observation_missing", rec["flags"])
        self.assertEqual(rec["resources"]["series"]["peak_working_set_bytes"], [])

    def test_monotonically_increasing_series_is_flagged(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 6, 0,
                                      FakeObserver([100, 110, 120, 130, 140, 150]))
        self.assertIn("monotonic_increase:peak_working_set_bytes", rec["flags"])
        t = rec["resources"]["trend"]["peak_working_set_bytes"]
        self.assertTrue(t["monotonic_increase"])
        self.assertEqual(t["growth"], 50)
        self.assertGreater(t["slope_per_run"], 0)
        self.assertEqual(rec["status"], "pass")  # flagged for review, not invented as a failure

    def test_flat_and_noisy_series_are_not_flagged(self):
        flat = bm.resource_trend([100] * 8)
        noisy = bm.resource_trend([100, 90, 110, 95, 105, 98])
        short = bm.resource_trend([1, 2, 3])
        self.assertFalse(flat["monotonic_increase"])
        self.assertFalse(noisy["monotonic_increase"])
        self.assertFalse(short["monotonic_increase"])   # too few runs to claim a trend

    def test_partial_observation_is_disclosed(self):
        class Flaky(FakeObserver):
            def next_metrics(self):
                self.calls += 1
                return None if self.calls % 2 else {"peak_working_set_bytes": 5}
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 4, 0, Flaky([5]))
        self.assertIn("resource_observation_incomplete", rec["flags"])
        self.assertEqual(rec["status"], "partial")


class Failures(unittest.TestCase):
    def test_timeout_kills_process_and_reports(self):
        cleaned = []
        with tempfile.TemporaryDirectory() as d:
            t0 = time.time()
            rec = bm.measure_workload([PY, "-c", "import time; time.sleep(60)"], d, 2, 0,
                                      FakeObserver([1] * 5), timeout_s=0.4,
                                      cleanup=lambda: cleaned.append(1))
        self.assertLess(time.time() - t0, 20)
        self.assertEqual(rec["status"], "timeout")
        self.assertTrue(rec["cleanup"]["child_terminated"])
        self.assertTrue(rec["cleanup"]["ran"])
        self.assertEqual(cleaned, [1])
        self.assertEqual(rec["samples"], [])
        self.assertIn("timed out", rec["failures"][0]["reason"])

    def test_nonzero_exit_is_command_failed(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload([PY, "-c", "import sys; sys.stderr.write('boom'); sys.exit(3)"],
                                      d, 2, 0, FakeObserver([1] * 5))
        self.assertEqual(rec["status"], "command_failed")
        self.assertIn("exit status 3", rec["failures"][0]["reason"])
        self.assertIn("boom", rec["failures"][0]["reason"])

    def test_missing_executable_is_command_failed(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload([str(Path(d) / "no-such-program.exe")], d, 1, 0, None)
        self.assertEqual(rec["status"], "command_failed")
        self.assertIn("launch failed", rec["failures"][0]["reason"])

    def test_wrong_output_despite_good_timing(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 3, 0, FakeObserver([1] * 5),
                                      check=lambda oc, i, ph: (False, "image is blank"))
        self.assertEqual(rec["status"], "output_mismatch")
        self.assertEqual(rec["outputs"]["mismatches"], 1)
        self.assertEqual(rec["samples"], [])           # the fast run is not accepted as a sample
        self.assertIn("image is blank", rec["failures"][0]["reason"])

    def test_checker_exception_is_a_mismatch(self):
        def boom(oc, i, ph):
            raise ValueError("bad checker")
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 1, 0, None, check=boom)
        self.assertEqual(rec["status"], "output_mismatch")

    def test_output_changing_between_cycles_is_a_mismatch(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "out"
            script = ("import pathlib,time,sys;"
                      "p=pathlib.Path(sys.argv[1]);p.mkdir(exist_ok=True);"
                      "(p/'a.txt').write_text(str(time.time_ns()))")
            chk = bm.OutputChecker(out, min_files=1, stable=True)
            rec = bm.measure_workload([PY, "-c", script, str(out)], d, 3, 0, None,
                                      check=chk, setup=chk.prepare)
        self.assertEqual(rec["status"], "output_mismatch")
        self.assertIn("differs", rec["failures"][0]["reason"])

    def test_stable_output_passes_and_missing_output_fails(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "out"
            writer = ("import pathlib,sys;p=pathlib.Path(sys.argv[1]);p.mkdir(exist_ok=True);"
                      "(p/'a.txt').write_text('same')")
            chk = bm.OutputChecker(out, min_files=1)
            ok = bm.measure_workload([PY, "-c", writer, str(out)], d, 3, 1, None,
                                     check=chk, setup=chk.prepare)
            self.assertEqual(ok["status"], "partial")      # passes; observer is None
            self.assertEqual(ok["outputs"]["checked_runs"], 4)
            self.assertEqual(ok["outputs"]["mismatches"], 0)
            chk2 = bm.OutputChecker(Path(d) / "other", min_files=1)
            bad = bm.measure_workload(FAST, d, 1, 0, None, check=chk2, setup=chk2.prepare)
            self.assertEqual(bad["status"], "output_mismatch")

    def test_png_checker_rejects_non_png_and_wrong_dimensions(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "o"
            out.mkdir()
            (out / "x.png").write_bytes(b"not a png at all")
            ok, detail = bm.OutputChecker(out)(None, 0, "measured")
            self.assertFalse(ok)
            png = (bm.PNG_SIG + (13).to_bytes(4, "big") + b"IHDR" + (4).to_bytes(4, "big")
                   + (5).to_bytes(4, "big") + b"\x08\x06\x00\x00\x00")
            (out / "x.png").write_bytes(png)
            self.assertTrue(bm.OutputChecker(out, png_dims=(4, 5))(None, 0, "measured")[0])
            self.assertFalse(bm.OutputChecker(out, png_dims=(9, 9))(None, 0, "measured")[0])


class Identity(unittest.TestCase):
    def ident(self, sha="aaa", cfg="Release", **build):
        return {"host_class": "win|x86|cpuA",
                "build": {"config": cfg, "exe_sha256": sha, "compiler": "MSVC 19.44",
                          "sanitizers": False, "coverage": False, **build}}

    def test_changed_build_identity_invalidates_the_run(self):
        calls = {"n": 0}

        def probe():
            calls["n"] += 1
            return self.ident("aaa" if calls["n"] <= 3 else "bbb")   # exe rebuilt mid-run
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 6, 0, FakeObserver([1] * 9), identity_probe=probe)
        self.assertEqual(rec["status"], "identity_changed")
        self.assertTrue(rec["identity"]["changed"])
        self.assertEqual(rec["failures"][0]["diff"]["build"]["before"]["exe_sha256"], "aaa")
        self.assertLess(len(rec["samples"]), 6)

    def test_stable_identity_passes(self):
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 3, 0, FakeObserver([1] * 9),
                                      identity_probe=lambda: self.ident())
        self.assertEqual(rec["status"], "pass")
        self.assertEqual(rec["identity"]["before"], rec["identity"]["after"])

    def test_instrumented_and_non_release_builds_are_refused(self):
        with tempfile.TemporaryDirectory() as d:
            for ident in (self.ident(sanitizers=True), self.ident(coverage=True),
                          self.ident(cfg="Debug")):
                rec = bm.measure_workload(FAST, d, 2, 0, None, identity_probe=lambda i=ident: i)
                self.assertEqual(rec["status"], "refused")
                self.assertEqual(rec["samples"], [])
                self.assertEqual(rec["warmup_samples"], [])

    def test_build_identity_reads_cmake_cache_flags(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            exe = d / "x.exe"
            exe.write_bytes(b"MZ")
            (d / "b").mkdir()
            (d / "b" / "CMakeCache.txt").write_text(
                "DS1EDIT_ENABLE_SANITIZERS:BOOL=ON\nDS1EDIT_ENABLE_COVERAGE:BOOL=OFF\n"
                "CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022\n")
            ident = bm.build_identity(exe, d / "b", None)
        self.assertTrue(ident["sanitizers"])
        self.assertFalse(ident["coverage"])
        self.assertIsNotNone(bm.identity_refusal({"build": ident}))
        self.assertEqual(len(ident["exe_sha256"]), 64)


class Cancellation(unittest.TestCase):
    def test_cancel_kills_child_and_runs_cleanup(self):
        cancel = threading.Event()
        cleaned = []
        threading.Timer(0.5, cancel.set).start()
        with tempfile.TemporaryDirectory() as d:
            t0 = time.time()
            rec = bm.measure_workload([PY, "-c", "import time; time.sleep(60)"], d, 5, 1,
                                      FakeObserver([1] * 9), cancel=cancel, timeout_s=50,
                                      cleanup=lambda: cleaned.append(True))
        self.assertLess(time.time() - t0, 20)
        self.assertEqual(rec["status"], "cancelled")
        self.assertTrue(rec["cleanup"]["child_terminated"])
        self.assertEqual(cleaned, [True])
        self.assertEqual(rec["samples"], [])

    def test_cleanup_runs_even_when_setup_raises(self):
        cleaned = []

        def bad_setup(i, ph):
            raise RuntimeError("no disk")
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(RuntimeError):
                bm.measure_workload(FAST, d, 1, 0, None, setup=bad_setup,
                                    cleanup=lambda: cleaned.append(1))
        self.assertEqual(cleaned, [1])

    def test_cancelled_before_start_runs_nothing(self):
        cancel = threading.Event()
        cancel.set()
        with tempfile.TemporaryDirectory() as d:
            rec = bm.measure_workload(FAST, d, 3, 1, None, cancel=cancel)
        self.assertEqual(rec["status"], "cancelled")
        self.assertEqual(rec["samples"] + rec["warmup_samples"], [])


class Comparison(unittest.TestCase):
    def rec(self, med, sha="aaa", cfg="Release", wid="w", inputs=None, host="h"):
        return {"workload_id": wid, "command": ["C:/a/ds1edit.exe", "x"], "inputs": inputs or {},
                "repetitions": 10, "warmups": 3, "status": "pass",
                "stats": {"median": med, "min": med - 0.1, "max": med + 0.1, "n": 10},
                "identity": {"before": {"host_class": host,
                                        "build": {"config": cfg, "compiler": "MSVC 19.44",
                                                  "exe_sha256": sha}}}}

    def test_like_for_like_is_unreviewed_never_pass_or_fail(self):
        c = bm.compare_records(self.rec(1.0), self.rec(2.0, sha="bbb"))
        self.assertEqual(c["status"], "unreviewed")
        self.assertEqual(c["median_ratio"], 2.0)
        self.assertFalse(c["distributions_overlap"])
        self.assertNotIn(c["status"], ("pass", "fail", "regression"))

    def test_different_host_workload_inputs_or_config_are_incomparable(self):
        base = self.rec(1.0)
        for other in (self.rec(1.0, host="other"), self.rec(1.0, wid="x"),
                      self.rec(1.0, cfg="Debug"), self.rec(1.0, inputs={"h": 1})):
            self.assertEqual(bm.compare_records(base, other)["status"], "incomparable")

    def test_failed_or_empty_record_is_incomparable(self):
        bad = self.rec(1.0)
        bad["status"] = "timeout"
        bad["stats"] = None
        c = bm.compare_records(self.rec(1.0), bad)
        self.assertEqual(c["status"], "incomparable")
        self.assertTrue(any("timeout" in r for r in c["reasons"]))


class ProfileFile(unittest.TestCase):
    def test_performance_core_profile_is_wellformed(self):
        p = json.loads((ROOT / "test/equivalence/profiles/performance-core.json").read_text())
        self.assertEqual(p["schema"], bm.PROFILE_SCHEMA)
        self.assertEqual((p["repetitions"], p["warmups"]), (10, 3))
        ids = [w["id"] for w in p["workloads"]]
        self.assertEqual(len(ids), len(set(ids)))
        for w in p["workloads"]:
            if w.get("status") == "not_implemented":
                self.assertTrue(w["reason"])
            else:
                self.assertIn(w["kind"], ("map-open-headless", "probe-decode", "editor-cli"))
        self.assertTrue(any(w.get("status") == "not_implemented" for w in p["workloads"]),
                        "unimplemented workloads must be declared, not omitted")

    def test_expand_workloads_reads_map_inis(self):
        p = json.loads((ROOT / "test/equivalence/profiles/performance-core.json").read_text())
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            (d / "assets").mkdir()
            for m in p["workloads"][0]["maps"]:
                (d / "assets" / f"{m}.ini").write_text(f"2 3 assets/tiles/{m}.ds1\n")
            specs = bm.expand_workloads(p, d, Path("probe.exe"))
        maps = [s for s in specs if s.get("kind") == "map-open-headless"]
        self.assertEqual(len(maps), len(p["workloads"][0]["maps"]))
        self.assertEqual(maps[0]["command"][1:4], [f"assets/tiles/{p['workloads'][0]['maps'][0]}.ds1", "2", "3"])
        self.assertTrue(any(s.get("status") == "not_run" for s in specs))


if __name__ == "__main__":
    unittest.main()
