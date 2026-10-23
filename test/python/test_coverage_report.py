"""Tests for scripts/equivalence/coverage_report.py on synthetic reports.

No compiler or coverage tool is needed: the reports are hand-built gcov JSON
and llvm-cov export documents.
"""

import gzip
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts.equivalence import coverage_report as cr  # noqa: E402

SRC = Path("/repo")  # not on disk: production_files is always passed explicitly


def gcov(files, version="1", gcc="13.2.0", cwd="/repo"):
    return {"format_version": version, "gcc_version": gcc,
            "current_working_directory": cwd, "files": files}


def gfile(name, lines):
    """lines: {lineno: count | (count, [branch counts])}"""
    out = []
    for n, v in lines.items():
        entry = {"line_number": n}
        if isinstance(v, tuple):
            entry["count"] = v[0]
            entry["branches"] = [{"count": b, "throw": False, "fallthrough": False}
                                 for b in v[1]]
        else:
            entry["count"] = v
            entry["branches"] = []
        out.append(entry)
    return {"file": name, "lines": out, "functions": []}


def blob(doc):
    return json.dumps(doc).encode()


def agg(reports, production, **kw):
    kw.setdefault("fmt", "gcov-json")
    return cr.aggregate(reports, SRC, production_files=production, **kw)


class GcovAggregation(unittest.TestCase):
    def test_counts_lines_and_branches(self):
        rep = gcov([gfile("src/core/a.c", {1: 3, 2: 0, 3: (5, [4, 0])})])
        r = agg([("a", blob(rep))], ["src/core/a.c"])
        self.assertEqual(r["status"], "complete")
        t = r["totals"]
        self.assertEqual((t["lines"]["total"], t["lines"]["covered"]), (3, 2))
        self.assertEqual((t["branches"]["total"], t["branches"]["covered"]), (2, 1))
        self.assertEqual(t["branches"]["percent"], 50.0)
        self.assertEqual(r["tool"]["format"], "gcov-json")
        self.assertEqual(r["tool"]["format_versions"], ["1"])
        self.assertIn("13.2.0", r["tool"]["producer"])

    def test_zero_branches_is_not_applicable_not_a_percentage(self):
        rep = gcov([gfile("src/core/a.c", {1: 1, 2: 1})])
        r = agg([("a", blob(rep))], ["src/core/a.c"])
        self.assertIsNone(r["totals"]["branches"]["percent"])
        self.assertEqual(r["modules"][0]["branches"]["total"], 0)
        self.assertIsNone(r["modules"][0]["branches"]["percent"])

    def test_branches_present_but_none_executed_is_zero_percent(self):
        rep = gcov([gfile("src/core/a.c", {1: (1, [0, 0])})])
        r = agg([("a", blob(rep))], ["src/core/a.c"])
        self.assertEqual(r["totals"]["branches"]["percent"], 0.0)
        self.assertEqual(r["totals"]["branches"]["total"], 2)

    def test_duplicate_identical_report_counted_once_and_disclosed(self):
        rep = blob(gcov([gfile("src/core/a.c", {1: 2})]))
        r = agg([("first.json", rep), ("second.json", rep)], ["src/core/a.c"])
        self.assertEqual(r["modules"][0]["lines"]["covered"], 1)
        self.assertEqual(len(r["reports"]), 1)
        self.assertEqual(r["duplicate_reports"][0]["path"], "second.json")
        self.assertEqual(r["duplicate_reports"][0]["duplicate_of"], "first.json")

    def test_different_reports_for_one_file_merge_their_counts(self):
        a = blob(gcov([gfile("src/core/a.c", {1: 1, 2: 0})]))
        b = blob(gcov([gfile("src/core/a.c", {1: 0, 2: 4})]))
        r = agg([("a", a), ("b", b)], ["src/core/a.c"])
        self.assertEqual(r["modules"][0]["lines"], {"total": 2, "covered": 2, "percent": 100.0})

    def test_excluded_unity_stubs_tests_generated_are_not_production(self):
        rep = gcov([
            gfile("src/core/a.c", {1: 1}),
            gfile("test/unity/unity.c", {1: 1}),
            gfile("test/test_a.c", {1: 1}),
            gfile("src/core/a_stubs.c", {1: 1}),
            gfile("src/gen/generated/x.c", {1: 1}),
        ])
        r = agg([("a", blob(rep))], ["src/core/a.c", "src/core/a_stubs.c"])
        self.assertEqual([m["file"] for m in r["modules"]], ["src/core/a.c"])
        self.assertEqual(r["totals"]["lines"]["total"], 1)
        excluded = {f for e in r["exclusions"] for f in e["files"]}
        self.assertEqual(excluded, {"test/unity/unity.c", "test/test_a.c",
                                    "src/core/a_stubs.c", "src/gen/generated/x.c"})
        for e in r["exclusions"]:
            self.assertTrue(e["reason"])
        self.assertEqual(r["status"], "complete")

    def test_uninstrumented_production_module_is_disclosed_not_covered(self):
        rep = gcov([gfile("src/core/a.c", {1: 1})])
        r = agg([("a", blob(rep))], ["src/core/a.c", "src/ui/edit_window.c"])
        self.assertEqual(r["status"], "partial")
        self.assertEqual(r["uninstrumented_modules"], ["src/ui/edit_window.c"])
        mod = [m for m in r["modules"] if m["file"] == "src/ui/edit_window.c"][0]
        self.assertEqual(mod["status"], "uninstrumented")
        self.assertIsNone(mod["lines"])
        # It adds no covered lines and no pretend 100%.
        self.assertEqual(r["totals"]["lines"]["total"], 1)
        self.assertEqual(r["production_modules"], 2)
        self.assertEqual(r["instrumented_modules"], 1)

    def test_system_header_is_outside_production(self):
        rep = gcov([gfile("src/core/a.c", {1: 1}), gfile("/usr/include/stdio.h", {1: 1})])
        r = agg([("a", blob(rep))], ["src/core/a.c"])
        self.assertEqual(r["outside_production"], ["/usr/include/stdio.h"])
        self.assertEqual(r["totals"]["lines"]["total"], 1)

    def test_absent_optional_report_listed_and_run_continues(self):
        with tempfile.TemporaryDirectory() as d:
            ok = Path(d) / "ok.json"
            ok.write_bytes(blob(gcov([gfile("src/core/a.c", {1: 1})])))
            r = agg([ok, Path(d) / "gone.json"], ["src/core/a.c"])
        self.assertEqual(r["status"], "complete")
        self.assertEqual(len(r["missing_reports"]), 1)
        self.assertFalse(r["missing_reports"][0]["required"])

    def test_absent_required_report_makes_everything_unavailable(self):
        with tempfile.TemporaryDirectory() as d:
            ok = Path(d) / "ok.json"
            ok.write_bytes(blob(gcov([gfile("src/core/a.c", {1: 1})])))
            gone = Path(d) / "gone.json"
            r = agg([ok], ["src/core/a.c"], required=[gone])
        self.assertEqual(r["status"], "unavailable")
        self.assertIsNone(r["totals"])
        self.assertTrue(r["missing_reports"][0]["required"])
        self.assertIn("gone.json", r["status_reason"])

    def test_no_reports_at_all_is_unavailable_not_zero_percent(self):
        r = agg([], ["src/core/a.c"])
        self.assertEqual(r["status"], "unavailable")
        self.assertIsNone(r["totals"])

    def test_unknown_gcov_format_version_is_refused(self):
        rep = gcov([gfile("src/core/a.c", {1: 1})], version="9")
        with self.assertRaises(cr.CoverageError) as cm:
            agg([("a", blob(rep))], ["src/core/a.c"])
        self.assertIn("format_version", str(cm.exception))

    def test_garbage_report_is_an_error_not_an_empty_pass(self):
        with self.assertRaises(cr.CoverageError):
            agg([("a", b"not json")], ["src/core/a.c"])
        with self.assertRaises(cr.CoverageError):
            agg([("a", b"")], ["src/core/a.c"])

    def test_concatenated_documents_and_gzip(self):
        d1 = blob(gcov([gfile("src/core/a.c", {1: 1})]))
        d2 = blob(gcov([gfile("src/core/b.c", {1: 0})]))
        r = agg([("multi", d1 + b"\n" + d2)], ["src/core/a.c", "src/core/b.c"])
        self.assertEqual(r["totals"]["lines"], {"total": 2, "covered": 1, "percent": 50.0})
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "x.gcov.json.gz"
            p.write_bytes(gzip.compress(d1))
            r = agg([p], ["src/core/a.c"])
        self.assertEqual(r["status"], "complete")

    def test_paths_normalised_relative_and_windows_style(self):
        rep = gcov([gfile("src\\core\\..\\core\\a.c", {1: 1})], cwd="/repo")
        r = agg([("a", blob(rep))], ["src/core/a.c"])
        self.assertEqual(r["modules"][0]["file"], "src/core/a.c")
        self.assertEqual(r["modules"][0]["status"], "instrumented")

    def test_percent_is_never_derived_from_counts_of_suites(self):
        # Two reports that both say nothing ran: 0 covered of 2, not 100%.
        a = blob(gcov([gfile("src/core/a.c", {1: 0, 2: 0})]))
        r = agg([("a", a)], ["src/core/a.c"])
        self.assertEqual(r["totals"]["lines"]["percent"], 0.0)


def llvm(files, version="2.0.1"):
    return {"type": "llvm.coverage.json.export", "version": version,
            "data": [{"files": files}]}


class LlvmExport(unittest.TestCase):
    def test_segments_branches_and_uninstrumented(self):
        f = {"filename": "/repo/src/core/a.c",
             # line 1 executed twice (to line 3); line 4 region count 0
             "segments": [[1, 1, 2, True, True, False],
                          [3, 2, 0, False, False, False],
                          [4, 1, 0, True, True, False],
                          [5, 1, 0, False, False, False]],
             "branches": [[2, 5, 2, 9, 7, 0, 0, 0, 4]]}
        r = agg([("l", blob(llvm([f])))], ["src/core/a.c", "src/core/b.c"],
                fmt="llvm-export")
        mod = [m for m in r["modules"] if m["file"] == "src/core/a.c"][0]
        self.assertEqual(mod["lines"]["covered"], 3)   # lines 1-3
        self.assertEqual(mod["lines"]["total"], 4)     # + line 4 uncovered
        self.assertEqual(mod["branches"], {"total": 2, "covered": 1, "percent": 50.0})
        self.assertEqual(r["status"], "partial")
        self.assertEqual(r["uninstrumented_modules"], ["src/core/b.c"])
        self.assertEqual(r["tool"]["format"], "llvm-export")
        self.assertEqual(r["tool"]["format_versions"], ["2.0.1"])

    def test_wrong_document_type_and_version_refused(self):
        with self.assertRaises(cr.CoverageError):
            agg([("l", blob({"type": "other"}))], [], fmt="llvm-export")
        with self.assertRaises(cr.CoverageError):
            agg([("l", blob(llvm([], version="3.0.0")))], [], fmt="llvm-export")

    def test_gcov_document_is_not_accepted_as_llvm(self):
        with self.assertRaises(cr.CoverageError):
            agg([("g", blob(gcov([])))], [], fmt="llvm-export")


class Cli(unittest.TestCase):
    def test_cli_writes_json_and_exit_status_reflects_completeness(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            (d / "src" / "core").mkdir(parents=True)
            (d / "src" / "core" / "a.c").write_text("int x;\n")
            (d / "src" / "core" / "b.c").write_text("int y;\n")
            rep = d / "r.json"
            rep.write_bytes(blob(gcov([gfile(str(d / "src/core/a.c").replace("\\", "/"),
                                             {1: 1})], cwd=str(d).replace("\\", "/"))))
            out = d / "out" / "cov.json"
            rc = cr.main(["--format", "gcov-json", "--report", str(rep),
                          "--source-root", str(d), "--out", str(out)])
            data = json.loads(out.read_text())
            self.assertEqual(rc, 1)  # b.c uninstrumented -> partial
            self.assertEqual(data["status"], "partial")
            self.assertEqual(data["uninstrumented_modules"], ["src/core/b.c"])
            rc = cr.main(["--format", "gcov-json", "--report", str(rep),
                          "--require-report", str(d / "nope.json"),
                          "--source-root", str(d), "--out", str(out)])
            self.assertEqual(rc, 1)
            self.assertEqual(json.loads(out.read_text())["status"], "unavailable")

    def test_cli_bad_exclude_and_bad_report_exit_2(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            bad = d / "bad.json"
            bad.write_text("{{")
            self.assertEqual(cr.main(["--format", "gcov-json", "--report", str(bad),
                                      "--source-root", str(d), "--out", str(d / "o.json")]), 2)
            self.assertEqual(cr.main(["--format", "gcov-json", "--exclude", "noreason",
                                      "--source-root", str(d), "--out", str(d / "o.json")]), 2)


if __name__ == "__main__":
    unittest.main()
