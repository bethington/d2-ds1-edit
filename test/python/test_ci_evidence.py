"""CI evidence: CTest/JUnit results -> ds1edit.case/1 bound to one candidate.

Fakes only: JUnit XML written here, a hand-made candidate, no CTest, no build.
"""

from __future__ import annotations

import copy
import hashlib
import io
import json
import shutil
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from xml.sax.saxutils import escape

REPO = Path(__file__).resolve().parents[2]
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

from scripts.equivalence import ci_evidence as CI  # noqa: E402
from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import qualify as Q  # noqa: E402

RELEASE_PROFILE = REPO / "test" / "equivalence" / "profiles" / "release-core.json"
PLATFORMS = ("windows-x86", "linux-x86_64", "macos-arm64")
SRC = hashlib.sha256(b"source").hexdigest()
COMMIT = "c" * 40


def H(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def source_identity(**kw):
    s = {"commit": COMMIT, "source_state_digest": SRC, "dirty": False, "dirty_sha256": None}
    s.update(kw)
    return s


# ---------------------------------------------------------------------------
# Fake CTest JUnit
# ---------------------------------------------------------------------------

def junit_xml(tests: dict) -> str:
    """tests: name -> {"status": pass|fail|skipped, "out": str}."""
    parts = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<testsuite name="(empty)" tests="%d">' % len(tests)]
    for name, t in tests.items():
        status = t.get("status", "pass")
        parts.append('<testcase name="%s" classname="%s" time="0.1" status="%s">'
                     % (name, name, "notrun" if status == "skipped" else "run"))
        if status == "fail":
            parts.append('<failure message="Failed" type="Failure"/>')
        elif status == "skipped":
            parts.append('<skipped message="Skipped"/>')
        parts.append("<system-out>%s</system-out></testcase>" % escape(t.get("out", "")))
    parts.append("</testsuite>")
    return "\n".join(parts)


def unity_line(func, verdict="PASS"):
    return "C:\\src\\test\\test_x.c:12:%s:%s\n" % (func, verdict)


def py_line(method, verdict="ok", module="test.python.test_x.Case"):
    return "%s (%s.%s) ... %s\n" % (method, module, method, verdict)


def passing_output(rule_checks):
    """Per ctest name, the output a fully passing run would print."""
    out = {}
    for ch in rule_checks:
        buf = out.setdefault(ch.test, [])
        if ch.kind == "unity":
            buf.append(unity_line(ch.name))
        elif ch.kind == "py":
            buf.append(py_line(ch.name))
        elif ch.kind == "line":
            buf.append("%-28s pass\n" % ch.name)
    return out


def build_job_tests(platform, repo=REPO):
    """Every CTest a passing build job on *platform* would report."""
    tests = {}
    for rule in CI.RULES:
        if rule["job"] != "build" or platform not in rule["platforms"]:
            continue
        for test, lines in passing_output(CI.rule_checks(rule, repo)).items():
            tests.setdefault(test, {"status": "pass", "out": ""})["out"] += "".join(lines)
        for ch in CI.rule_checks(rule, repo):
            tests.setdefault(ch.test, {"status": "pass", "out": ""})
    tests.setdefault("py_a", {"status": "pass", "out": ""})
    tests["py_a"]["out"] += "Ran 7 tests in 0.1s\n\nOK\n"
    tests.setdefault("py_b", {"status": "pass", "out": ""})
    tests["py_b"]["out"] += "Ran 3 tests in 0.1s\n\nOK (skipped=0)\n"
    tests.setdefault("golden_core", {"status": "pass", "out": ""})
    for name, t in tests.items():
        if name.startswith("py_") and "Ran " not in t["out"]:
            t["out"] += "Ran 1 test in 0.1s\n\nOK\n"
    return tests


def manifest(platform):
    return {"platform": platform,
            "executables": {n: H("exe:%s:%s" % (platform, n)) for n in CI.known_executables()},
            "build": {"config": "Release", "compiler": "unit 1"}}


def ctx(platform="windows-x86", **kw):
    args = dict(platform=platform, source=source_identity(), manifest=manifest(platform),
                repo_root=REPO, replay_case=None)
    args.update(kw)
    return CI.Context(**args)


def build(platform="windows-x86", tests=None, **kw):
    tests = build_job_tests(platform) if tests is None else tests
    results = CI.parse_junit(junit_xml(tests))
    return CI.build_evidence(ctx(platform, **kw), results, job="build")


def by_id(evidence):
    return {c["id"]: c for c in evidence["cases"]}


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

class Parsing(unittest.TestCase):
    def test_junit_statuses_and_output(self):
        xml = junit_xml({"a": {"out": "hello <&>"}, "b": {"status": "fail", "out": "boom"},
                         "c": {"status": "skipped"}})
        r = CI.parse_junit(xml)
        self.assertEqual({k: v.status for k, v in r.items()},
                         {"a": "pass", "b": "fail", "c": "skipped"})
        self.assertEqual(r["a"].output, "hello <&>")

    def test_junit_from_a_file_and_malformed_or_duplicate_input(self):
        d = Path(tempfile.mkdtemp(prefix="ds1edit ci junit "))
        self.addCleanup(shutil.rmtree, d, True)
        f = d / "junit.xml"
        f.write_text(junit_xml({"a": {}}), encoding="utf-8")
        self.assertEqual(list(CI.parse_junit(f)), ["a"])
        with self.assertRaises(C.ValidationError):
            CI.parse_junit("<testsuite><testcase")
        with self.assertRaises(C.ValidationError):
            CI.parse_junit("<nope/>")
        with self.assertRaises(C.ValidationError):
            CI.parse_junit(d / "missing.xml")
        dup = ('<testsuite><testcase name="a" status="run"/>'
               '<testcase name="a" status="run"/></testsuite>')
        with self.assertRaises(C.ValidationError):
            CI.parse_junit(dup)
        self.assertEqual(CI.parse_junit("<testsuite/>"), {})

    def test_unity_results(self):
        out = (unity_line("test_a") + "noise\n" + unity_line("test_b", "FAIL")
               + "C:\\x.c:9:test_c:FAIL: Expected 1 Was 2\n"
               + "/home/r/test_x.c:7:test_d:IGNORE\n")
        self.assertEqual(CI.unity_results(out),
                         {"test_a": "PASS", "test_b": "FAIL", "test_c": "FAIL",
                          "test_d": "IGNORE"})

    def test_unittest_results_and_summary(self):
        out = (py_line("test_a") + py_line("test_b", "FAIL") + py_line("test_c", "ERROR")
               + py_line("test_d", "skipped 'why'") + py_line("test_e", "expected failure")
               + "\nRan 5 tests in 1.0s\n\nFAILED (failures=1, errors=1, skipped=1)\n")
        self.assertEqual(CI.unittest_results(out),
                         {"test_a": "ok", "test_b": "FAIL", "test_c": "ERROR",
                          "test_d": "skipped", "test_e": "expected failure"})
        self.assertEqual(CI.unittest_summary(out), (5, 1))
        self.assertEqual(CI.unittest_summary("Ran 1 test in 0.1s\n\nOK (skipped=2)\n"), (1, 2))
        self.assertEqual(CI.unittest_summary("nothing"), (0, 0))

    def test_unittest_result_with_a_docstring_line(self):
        out = ("test_a (m.C.test_a)\nDoes a thing ... ok\n"
               "test_b (m.C.test_b) ... ok\n")
        self.assertEqual(CI.unittest_results(out), {"test_a": "ok", "test_b": "ok"})

    def test_decoder_lines(self):
        out = ("dc6.rle.basic                pass\ndcc.sequence                 mismatch\n"
               "    transparency 1 != 0\ndcc.empty_direction          not_run\n")
        self.assertEqual(CI.decoder_lines(out),
                         {"dc6.rle.basic": "pass", "dcc.sequence": "mismatch",
                          "dcc.empty_direction": "not_run"})


# ---------------------------------------------------------------------------
# The mapping from the release profile to tests
# ---------------------------------------------------------------------------

class Mapping(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.profile = Q.load_profile(RELEASE_PROFILE)
        cls.entries = Q.expand_profile(cls.profile)

    def emitted(self):
        out = {}
        for rule in CI.RULES:
            for p in rule["platforms"]:
                out[(rule["id"], rule["lane"], p)] = rule
        return out

    def test_every_required_portable_and_sanitizer_entry_is_mapped_or_listed(self):
        emitted = self.emitted()
        for e in self.entries:
            if e["lane"] not in ("portable", "sanitizer"):
                continue
            k = (e["id"], e["lane"], e["platform"])
            self.assertTrue(k in emitted or e["id"] in CI.UNPROVEN,
                            "%s is neither proven by a CI test nor listed as unproven" % (k,))
            if k in emitted:
                self.assertNotIn(e["id"], CI.UNPROVEN)

    def test_unproven_entries_exist_in_the_profile_and_say_why(self):
        ids = {e["id"] for e in self.entries}
        self.assertEqual(sorted(CI.UNPROVEN), ["decoders-113c", "visual-values-113c"])
        for cid, why in CI.UNPROVEN.items():
            self.assertIn(cid, ids)
            self.assertTrue(len(why) > 20, cid)

    def test_rules_name_only_required_entries_on_required_platforms(self):
        req = {(e["id"], e["lane"], e["platform"]) for e in self.entries}
        for k in self.emitted():
            self.assertIn(k, req, "ci_evidence would emit %s, which the profile does not "
                                  "require" % (k,))

    def test_no_rule_claims_a_local_lane(self):
        for rule in CI.RULES:
            self.assertIn(rule["lane"], ("portable", "sanitizer"))
            self.assertIn(rule["job"], ("build", "sanitizer", "coverage"))

    def test_rule_ids_are_unique_per_platform(self):
        seen = set()
        for rule in CI.RULES:
            for p in rule["platforms"]:
                k = (rule["id"], rule["lane"], p)
                self.assertNotIn(k, seen)
                seen.add(k)

    def test_every_check_names_a_real_test_function(self):
        """A rename of a Unity/unittest test must break this, not the release."""
        text = {}
        for path in (REPO / "test").rglob("*.c"):
            text[path.name] = path.read_text(encoding="utf-8", errors="replace")
        pytext = "\n".join(p.read_text(encoding="utf-8", errors="replace")
                           for p in (REPO / "test" / "python").glob("test_*.py"))
        ctests = CI.registered_ctest_names(REPO)
        c_all = "\n".join(text.values())
        for rule in CI.RULES:
            for ch in CI.rule_checks(rule, REPO):
                if ch.kind == "unity":
                    self.assertIn("void %s(void)" % ch.name, c_all, ch)
                    self.assertRegex(c_all, r"(?:PKGC_RUN|RUN_TEST)\(%s\)" % ch.name, ch)
                elif ch.kind == "py":
                    self.assertIn("def %s(" % ch.name, pytext, ch)
                if rule["job"] == "build":
                    self.assertTrue(CI.ctest_is_registered(ch.test, ctests), ch)

    def test_ds1_matrix_features_come_from_the_fixtures_and_match_the_profile(self):
        want = next(c for r in self.profile["requirements"] for c in r["cases"]
                    if c["id"] == "ds1.format.matrix")["coverage"]["features"]
        got = CI.ds1_fixture_features(REPO)
        self.assertTrue(set(want) <= set(got), sorted(set(want) - set(got)))
        self.assertIn("v17", got)            # the fixtures cover more than the profile asks

    def test_ds1_matrix_features_are_empty_without_the_fixtures(self):
        d = Path(tempfile.mkdtemp(prefix="ds1edit ci nofix "))
        self.addCleanup(shutil.rmtree, d, True)
        self.assertEqual(CI.ds1_fixture_features(d), [])


# ---------------------------------------------------------------------------
# Building evidence
# ---------------------------------------------------------------------------

class BuildEvidence(unittest.TestCase):
    def test_a_passing_job_emits_every_mapped_case_and_all_validate(self):
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                ev = build(platform)
                self.assertEqual(ev["schema"], Q.CASE_SET_SCHEMA)
                self.assertEqual(ev["platform"], platform)
                want = sorted(r["id"] for r in CI.RULES
                              if r["job"] == "build" and platform in r["platforms"]
                              and r["type"] != "replay")
                got = sorted(c["id"] for c in ev["cases"] if c["id"] != "palette-nearest-113c")
                self.assertEqual(got, want)
                for c in ev["cases"]:
                    Q.validate_case(c)
                    self.assertEqual(c["platform"], platform)
                    self.assertTrue(c["required"])
                    if c["id"] == "palette-nearest-113c":      # needs the replay case
                        self.assertEqual(c["status"], "not_run")
                        continue
                    self.assertEqual(c["status"], "pass", (c["id"], c["failures"]))
                    self.assertEqual(c["success_outcome"], "pass")

    def test_cases_bind_to_the_jobs_source_and_executables(self):
        ev = build("linux-x86_64")
        m = manifest("linux-x86_64")
        for c in ev["cases"]:
            cand = c["provenance"]["candidate"]
            self.assertEqual(cand["source_state"]["source_state_digest"], SRC)
            self.assertEqual(cand["source_state"]["commit"], COMMIT)
            self.assertEqual(cand["packages"], [])
        dc = by_id(ev)["dc6.rle.basic"]
        self.assertEqual(dc["provenance"]["candidate"]["executable"],
                         {"name": "test_visual_reference_probe",
                          "sha256": m["executables"]["test_visual_reference_probe"]})
        self.assertNotIn("executable", by_id(ev)["editor.undo.tiles"]["provenance"]["candidate"])

    def test_an_executable_missing_from_the_candidate_stays_unbound(self):
        mf = manifest("windows-x86")
        del mf["executables"]["test_visual_reference_probe"]
        ev = build("windows-x86", manifest=mf)
        exe = by_id(ev)["dcc.sequence"]["provenance"]["candidate"]["executable"]
        self.assertIsNone(exe["sha256"])

    def test_portable_ctest_counts_every_test_and_any_failure_is_a_mismatch(self):
        tests = build_job_tests("windows-x86")
        ev = build("windows-x86", tests=tests)
        c = by_id(ev)["portable.ctest"]
        self.assertEqual(c["coverage"]["tests_run"], len(tests))
        tests["golden_core"]["status"] = "fail"
        c = by_id(build("windows-x86", tests=tests))["portable.ctest"]
        self.assertEqual(c["status"], "mismatch")
        self.assertIn("golden_core", json.dumps(c["failures"]))

    def test_portable_ctest_with_a_skipped_test_is_partial_and_empty_is_not_run(self):
        tests = build_job_tests("windows-x86")
        tests["ds1_save"]["status"] = "skipped"
        self.assertEqual(by_id(build("windows-x86", tests=tests))["portable.ctest"]["status"],
                         "partial")
        c = by_id(build("windows-x86", tests={}))["portable.ctest"]
        self.assertEqual(c["status"], "not_run")

    def test_local_only_stubs_skipped_for_want_of_a_runtime_do_not_block_portable_ctest(self):
        # CI has no game runtime: golden_core/display_selftest register as skip
        # stubs (test/cmake/golden.cmake). They are disclosed, not counted.
        tests = build_job_tests("windows-x86")
        n = len(tests)
        for name in ("golden_core", "display_selftest"):
            tests[name] = {"status": "skipped", "out": "SKIPPED %s: not configured" % name}
        c = by_id(build("windows-x86", tests=tests))["portable.ctest"]
        self.assertEqual(c["status"], "pass")
        self.assertEqual(c["coverage"]["tests_run"], n - 1)   # golden_core was in the n
        self.assertEqual(c["coverage"]["tests_skipped"], 0)
        self.assertEqual(c["coverage"]["local_only_skipped"], ["display_selftest", "golden_core"])
        self.assertTrue(any("display_selftest" in l for l in c["limitations"]))
        # a local-only test that did run and failed still fails the suite
        tests["golden_core"] = {"status": "fail", "out": "boom"}
        self.assertEqual(by_id(build("windows-x86", tests=tests))["portable.ctest"]["status"],
                         "mismatch")

    def test_python_unittest_counts_come_from_the_unittest_summaries(self):
        tests = build_job_tests("macos-arm64")
        c = by_id(build("macos-arm64", tests=tests))["portable.python-unittest"]
        self.assertEqual(c["status"], "pass")
        self.assertEqual(c["coverage"]["tests_run"],
                         sum(CI.unittest_summary(t["out"])[0] for n, t in tests.items()
                             if n.startswith("py_")))
        self.assertGreaterEqual(c["coverage"]["tests_run"], 10)
        self.assertEqual(c["coverage"]["tests_skipped"], 0)

    def test_python_unittest_skips_failures_and_missing_suites(self):
        tests = build_job_tests("linux-x86_64")
        tests["py_b"]["out"] = "Ran 3 tests in 0.1s\n\nOK (skipped=2)\n"
        self.assertEqual(by_id(build("linux-x86_64", tests=tests))
                         ["portable.python-unittest"]["status"], "partial")
        tests = build_job_tests("linux-x86_64")
        tests["py_a"]["status"] = "fail"
        self.assertEqual(by_id(build("linux-x86_64", tests=tests))
                         ["portable.python-unittest"]["status"], "mismatch")
        tests = build_job_tests("linux-x86_64")
        tests["py_unavailable"] = {"status": "fail", "out": "Python 3 was not found"}
        self.assertEqual(by_id(build("linux-x86_64", tests=tests))
                         ["portable.python-unittest"]["status"], "mismatch")
        tests = {k: v for k, v in build_job_tests("linux-x86_64").items()
                 if not k.startswith("py_")}
        self.assertEqual(by_id(build("linux-x86_64", tests=tests))
                         ["portable.python-unittest"]["status"], "not_run")
        tests = build_job_tests("linux-x86_64")
        tests["py_a"]["out"] = "no summary line"
        self.assertEqual(by_id(build("linux-x86_64", tests=tests))
                         ["portable.python-unittest"]["status"], "partial")

    def test_a_failing_unity_function_fails_only_the_cases_it_proves(self):
        tests = build_job_tests("windows-x86")
        out = tests["ds1_save"]["out"]
        bad = unity_line("test_short_write_is_reported_and_keeps_the_destination", "PASS")
        self.assertIn(bad, out)
        tests["ds1_save"]["out"] = out.replace(
            bad, unity_line("test_short_write_is_reported_and_keeps_the_destination",
                            "FAIL"))
        got = by_id(build("windows-x86", tests=tests))
        self.assertEqual(got["ds1.save.write-failure"]["status"], "mismatch")
        self.assertEqual(got["ds1.save.backup"]["status"], "pass")
        self.assertEqual(got["editor.undo.tiles"]["status"], "pass")

    def test_a_unity_function_that_never_ran_is_partial_and_a_missing_ctest_not_run(self):
        tests = build_job_tests("windows-x86")
        name = "test_second_save_keeps_previous_bytes_as_numbered_backup"
        tests["ds1_save"]["out"] = tests["ds1_save"]["out"].replace(unity_line(name), "")
        got = by_id(build("windows-x86", tests=tests))
        self.assertEqual(got["ds1.save.backup"]["status"], "partial")
        del tests["editor_undo"]
        got = by_id(build("windows-x86", tests=tests))
        self.assertEqual(got["editor.undo.tiles"]["status"], "not_run")
        self.assertEqual(got["editor.map-isolation"]["status"], "partial")

    def test_ignored_unity_function_is_not_a_pass(self):
        tests = build_job_tests("windows-x86")
        name = "test_map_slots_keep_independent_histories"
        tests["editor_undo"]["out"] = tests["editor_undo"]["out"].replace(
            unity_line(name), unity_line(name, "IGNORE"))
        self.assertEqual(by_id(build("windows-x86", tests=tests))
                         ["editor.map-isolation"]["status"], "partial")

    def test_a_failed_ctest_with_no_function_lines_is_a_mismatch_not_not_run(self):
        tests = build_job_tests("windows-x86")
        tests["ds1_save"] = {"status": "fail", "out": "crashed"}
        got = by_id(build("windows-x86", tests=tests))
        self.assertEqual(got["ds1.save.tmp"]["status"], "mismatch")

    def test_decoder_cases_are_read_line_by_line(self):
        tests = build_job_tests("windows-x86")
        t = tests["visual_reference_probe_decoder_cases"]
        t["out"] = t["out"].replace("dcc.sequence                 pass",
                                    "dcc.sequence                 mismatch")
        got = by_id(build("windows-x86", tests=tests))
        self.assertEqual(got["dcc.sequence"]["status"], "mismatch")
        self.assertEqual(got["dcc.single_frame"]["status"], "pass")
        t["out"] = t["out"].replace("dcc.sequence                 mismatch\n", "")
        self.assertEqual(by_id(build("windows-x86", tests=tests))["dcc.sequence"]["status"],
                         "not_run")

    def test_ds1_matrix_reports_the_fixture_features_only_when_the_tests_pass(self):
        got = by_id(build("windows-x86"))["ds1.format.matrix"]
        self.assertEqual(sorted(got["coverage"]["features_covered"]),
                         sorted(CI.ds1_fixture_features(REPO)))
        tests = build_job_tests("windows-x86")
        tests["ds1_cases"]["out"] = py_line("test_reader_matches_every_authored_expectation",
                                            "FAIL", "test.python.test_ds1_cases.Reader")
        got = by_id(build("windows-x86", tests=tests))["ds1.format.matrix"]
        self.assertEqual(got["status"], "mismatch")
        self.assertEqual(got["coverage"]["features_covered"], [])

    def test_export_images_features_follow_their_own_tests(self):
        got = by_id(build("macos-arm64"))["export.images"]
        self.assertEqual(sorted(got["coverage"]["features_covered"]),
                         sorted(["export-png", "export-apng", "delays", "transparency",
                                 "cancellation"]))
        tests = build_job_tests("macos-arm64")
        tests["export_progress"]["status"] = "fail"
        got = by_id(build("macos-arm64", tests=tests))["export.images"]
        self.assertEqual(got["status"], "mismatch")
        self.assertNotIn("cancellation", got["coverage"]["features_covered"])
        self.assertIn("export-png", got["coverage"]["features_covered"])

    def test_negative_controls_are_detected_only_by_a_passing_python_test(self):
        got = by_id(build("windows-x86"))
        ids = [i for i in got if i.startswith("control.")]
        self.assertEqual(len(ids), 9)
        for i in ids:
            self.assertTrue(got[i]["coverage"]["control_detected"], i)
        tests = build_job_tests("windows-x86")
        tests["py_export_images"]["out"] = tests["py_export_images"]["out"].replace(
            "test_priority_reversal_fails (test.python.test_x.Case.test_priority_reversal_fails)"
            " ... ok", "test_priority_reversal_fails (a.b.c) ... FAIL")
        got = by_id(build("windows-x86", tests=tests))
        c = got["control.compose.priority-reversal"]
        self.assertEqual(c["status"], "mismatch")
        self.assertFalse(c["coverage"]["control_detected"])
        self.assertTrue(got["control.export.wrong-delay"]["coverage"]["control_detected"])

    def test_controls_are_emitted_for_windows_only(self):
        self.assertFalse([c for c in build("linux-x86_64")["cases"]
                          if c["id"].startswith("control.")])

    def test_source_identity_must_carry_a_digest(self):
        with self.assertRaises(C.ValidationError):
            build("windows-x86", source={"commit": COMMIT})

    def test_adversarial_core_needs_all_its_ctests(self):
        tests = build_job_tests("windows-x86")
        tests["adversarial_codecs"]["status"] = "fail"
        self.assertEqual(by_id(build("windows-x86", tests=tests))["adversarial.core"]["status"],
                         "mismatch")


class ReplayCase(unittest.TestCase):
    def replay(self, **kw):
        case = {
            "schema": C.CASE_SCHEMA, "id": "palette-nearest-113c", "lane": "portable",
            "platform": "windows-amd64", "required": True, "status": "pass",
            "success_outcome": "pass", "contract_digest": H("c"), "baseline_digest": H("b"),
            "input_sha256": H("i"), "expected_sha256": H("e"), "actual_sha256": H("e"),
            "coverage": {"distinct_inputs_compared": 40, "uncovered_required_axes": []},
            "failures": [], "limitations": [],
            "provenance": {
                "candidate": {"source_state": {"commit": "d" * 40, "dirty": True,
                                               "dirty_sha256": H("dirty")},
                              "executable": {
                                  "name": "test_palette_reference_probe",
                                  "sha256": manifest("windows-x86")["executables"][
                                      "test_palette_reference_probe"]},
                              "build": {"config": "Release"}, "packages": []},
                "reference": {"adapter_digest": H("a")}},
        }
        case.update(kw)
        return case

    def test_the_replay_case_is_rebound_to_the_job_without_touching_its_verdict(self):
        ev = build("windows-x86", replay_case=self.replay())
        c = by_id(ev)["palette-nearest-113c"]
        self.assertEqual(c["platform"], "windows-x86")
        self.assertEqual(c["status"], "pass")
        self.assertEqual(c["contract_digest"], H("c"))
        self.assertEqual(c["provenance"]["reference"]["adapter_digest"], H("a"))
        self.assertEqual(c["provenance"]["candidate"]["source_state"]["source_state_digest"],
                         SRC)
        self.assertEqual(c["provenance"]["rebound"]["from_platform"], "windows-amd64")
        Q.validate_case(c)

    def test_a_mismatching_replay_stays_a_mismatch(self):
        c = by_id(build("linux-x86_64", replay_case=self.replay(status="mismatch")))[
            "palette-nearest-113c"]
        self.assertEqual(c["status"], "mismatch")

    def test_replay_of_another_probe_binary_is_left_stale(self):
        r = self.replay()
        r["provenance"]["candidate"]["executable"]["sha256"] = H("rebuilt elsewhere")
        c = by_id(build("windows-x86", replay_case=r))["palette-nearest-113c"]
        self.assertEqual(c["provenance"]["candidate"]["executable"]["sha256"],
                         H("rebuilt elsewhere"))

    def test_no_replay_case_is_not_run_and_a_failed_replay_test_is_a_mismatch(self):
        c = by_id(build("windows-x86"))["palette-nearest-113c"]
        self.assertEqual(c["status"], "not_run")
        tests = build_job_tests("windows-x86")
        tests["equivalence_palette_replay"] = {"status": "fail", "out": ""}
        c = by_id(build("windows-x86", tests=tests, replay_case=self.replay()))[
            "palette-nearest-113c"]
        self.assertEqual(c["status"], "mismatch")

    def test_a_replay_case_for_another_contract_is_refused(self):
        with self.assertRaises(C.ValidationError):
            build("windows-x86", replay_case=self.replay(id="decoders-113c"))


# ---------------------------------------------------------------------------
# Sanitizer and coverage lanes
# ---------------------------------------------------------------------------

def sanitizer_tests(**over):
    tests = {"adversarial_core": {}, "ds1_save": {}, "sanitizer_detector_control": {}}
    tests.update(over)
    return tests


class SanitizerAndCoverage(unittest.TestCase):
    def sanitizer(self, tests):
        return CI.build_evidence(ctx("linux-x86_64"), CI.parse_junit(junit_xml(tests)),
                                 job="sanitizer")

    def test_sanitizer_core_and_the_detector_control(self):
        ev = self.sanitizer(sanitizer_tests())
        got = by_id(ev)
        self.assertEqual(sorted(got), ["control.sanitizer.detector", "sanitizer.core"])
        for c in got.values():
            Q.validate_case(c)
            self.assertEqual((c["lane"], c["platform"], c["status"]),
                             ("sanitizer", "linux-x86_64", "pass"))
        self.assertTrue(got["control.sanitizer.detector"]["coverage"]["control_detected"])
        self.assertEqual(got["sanitizer.core"]["coverage"]["tests_run"], 3)

    def test_any_failing_test_under_the_sanitizers_is_a_mismatch(self):
        got = by_id(self.sanitizer(sanitizer_tests(ds1_save={"status": "fail"})))
        self.assertEqual(got["sanitizer.core"]["status"], "mismatch")

    def test_the_detector_must_have_run_and_caught_the_seeded_bug(self):
        got = by_id(self.sanitizer({"ds1_save": {}}))
        self.assertEqual(got["control.sanitizer.detector"]["status"], "not_run")
        self.assertFalse(got["control.sanitizer.detector"]["coverage"]["control_detected"])
        got = by_id(self.sanitizer(sanitizer_tests(
            sanitizer_detector_control={"status": "fail"})))
        self.assertEqual(got["control.sanitizer.detector"]["status"], "mismatch")

    def test_an_empty_sanitizer_run_is_not_run(self):
        got = by_id(self.sanitizer({}))
        self.assertEqual(got["sanitizer.core"]["status"], "not_run")

    def coverage(self, status, **extra):
        doc = {"schema": "ds1edit.coverage/1", "status": status, "status_reason": "r",
               "instrumented_modules": 12, "uninstrumented": ["src/a.c", "src/b.c"],
               "totals": {"lines": {"total": 100, "covered": 60, "percent": 60.0},
                          "branches": {"total": 10, "covered": 4, "percent": 40.0}}}
        doc.update(extra)
        return doc

    def cov_case(self, doc):
        ev = CI.build_coverage_evidence(ctx("linux-x86_64"), doc)
        self.assertEqual(len(ev["cases"]), 1)
        Q.validate_case(ev["cases"][0])
        return ev["cases"][0]

    def test_a_complete_or_partial_report_is_a_honest_pass_that_discloses_the_gaps(self):
        for status in ("complete", "partial"):
            c = self.cov_case(self.coverage(status))
            self.assertEqual((c["id"], c["lane"], c["status"]),
                             ("coverage.production", "sanitizer", "pass"))
            self.assertEqual(c["coverage"]["distinct_inputs_compared"], 12)
            self.assertEqual(c["coverage"]["lines_covered"], 60)
            self.assertEqual(c["coverage"]["uninstrumented_modules"], 2)
        self.assertTrue(any("uninstrumented" in l for l in
                            self.cov_case(self.coverage("partial"))["limitations"]))

    def test_an_unavailable_or_unreadable_report_is_unavailable(self):
        for doc in (self.coverage("unavailable", totals=None, instrumented_modules=0),
                    None, {"schema": "something/9"}):
            c = self.cov_case(doc)
            self.assertEqual(c["status"], "unavailable")
            self.assertEqual(c["coverage"]["distinct_inputs_compared"], 0)

    def test_a_report_with_no_instrumented_module_is_not_a_pass(self):
        c = self.cov_case(self.coverage("partial", instrumented_modules=0))
        self.assertNotEqual(c["status"], "pass")


# ---------------------------------------------------------------------------
# Against the real release profile and qualify()
# ---------------------------------------------------------------------------

def release_candidate(profile):
    ref = {"baseline_digest": H("baseline"), "contracts": {}, "adapters": {}}
    for e in Q.expand_profile(profile):
        if "contract" in e["reference"]:
            ref["contracts"][e["id"]] = H("c")
        if "adapter" in e["reference"]:
            ref["adapters"][e["id"]] = H("a")
    plats = {}
    for p in PLATFORMS:
        plats[p] = {"package": {"name": "DS1Edit-9.9.9-%s.zip" % p, "sha256": H("pkg:" + p),
                                "size": 1},
                    "executables": manifest(p)["executables"]}
    return C.seal({"schema": Q.CANDIDATE_SCHEMA, "candidate_run_id": "77",
                   "repository": "o/r",
                   "source": {"commit": COMMIT, "source_state_digest": SRC,
                              "built_from_source_state_digest": SRC,
                              "dirty": False, "dirty_sha256": None},
                   "build": {"per_platform": True}, "platforms": plats, "reference": ref,
                   "profile_digest": profile["digest"]})


class AgainstTheReleaseProfile(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.profile = Q.load_profile(RELEASE_PROFILE)
        cls.candidate = release_candidate(cls.profile)

    def evidence(self):
        results = []
        for p in PLATFORMS:
            results.append(build(p))
        results.append(CI.build_evidence(
            ctx("linux-x86_64"), CI.parse_junit(junit_xml(sanitizer_tests())),
            job="sanitizer"))
        results.append(CI.build_coverage_evidence(
            ctx("linux-x86_64"), SanitizerAndCoverage.coverage(None, "partial")))
        return results

    def test_every_emitted_case_qualifies_and_the_rest_is_exactly_the_unproven_set(self):
        report = Q.qualify(self.profile, self.candidate, self.evidence())
        by_status = {}
        for e in report["required"]:
            by_status.setdefault(e["qualifies"], []).append(e)
        # palette-nearest-113c needs the replay case (covered separately)
        emitted = {(r["id"], r["lane"], p) for r in CI.RULES for p in r["platforms"]}
        for e in report["required"]:
            k = (e["id"], e["lane"], e["platform"])
            if k in emitted and e["id"] != "palette-nearest-113c":
                self.assertTrue(e["qualifies"], (e["key"], e["reasons"]))
        never_in_ci = {e["key"] for e in report["required"] if not e["qualifies"]}
        for k in never_in_ci:
            lane = k.split("@")[1].split("/")[0]
            cid = k.split("@")[0]
            self.assertTrue(lane in ("corpus", "live", "native-package", "native-workflow",
                                     "performance")
                            or cid in CI.UNPROVEN or cid == "palette-nearest-113c", k)

    def test_the_report_is_not_ready_without_the_local_lanes(self):
        self.assertFalse(Q.qualify(self.profile, self.candidate, self.evidence())["ready"])

    def test_evidence_from_another_source_state_does_not_qualify(self):
        results = []
        for p in PLATFORMS:
            r = build(p, source=source_identity(source_state_digest=H("other tree")))
            results.append(r)
        report = Q.qualify(self.profile, self.candidate, results)
        e = next(x for x in report["required"] if x["key"].startswith(
            "portable.ctest@portable/windows-x86"))
        self.assertIn("stale_source", {r["code"] for r in e["reasons"]})


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

class Cli(unittest.TestCase):
    def setUp(self):
        self.d = Path(tempfile.mkdtemp(prefix="ds1edit ci cli "))
        self.addCleanup(shutil.rmtree, self.d, True)
        (self.d / "junit.xml").write_text(junit_xml(build_job_tests("linux-x86_64")),
                                          encoding="utf-8")
        (self.d / "source-identity.json").write_text(json.dumps(source_identity()),
                                                     encoding="utf-8")
        mf = manifest("linux-x86_64")
        (self.d / "candidate-linux-x86_64.json").write_text(json.dumps(mf), encoding="utf-8")
        self.replay = self.d / "case.json"
        self.replay.write_text(json.dumps(ReplayCase().replay()), encoding="utf-8")

    def run_cli(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            rc = CI.main([str(a) for a in args])
        return rc, out.getvalue(), err.getvalue()

    def test_build_subcommand_writes_a_case_set(self):
        out = self.d / "evidence" / "ci-linux-x86_64.json"
        rc, stdout, _ = self.run_cli(
            "build", "--platform", "linux-x86_64", "--junit", self.d / "junit.xml",
            "--source-identity", self.d / "source-identity.json",
            "--manifest", self.d / "candidate-linux-x86_64.json",
            "--replay-case", self.replay, "--out", out)
        self.assertEqual(rc, 0, stdout)
        doc = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual(doc["schema"], Q.CASE_SET_SCHEMA)
        self.assertTrue(any(c["id"] == "portable.ctest" for c in doc["cases"]))
        self.assertFalse(out.read_bytes().count(b"\r"))

    def test_failing_cases_still_write_the_evidence_and_exit_one(self):
        t = build_job_tests("linux-x86_64")
        t["golden_core"]["status"] = "fail"
        (self.d / "junit.xml").write_text(junit_xml(t), encoding="utf-8")
        out = self.d / "e.json"
        rc, _, _ = self.run_cli(
            "build", "--platform", "linux-x86_64", "--junit", self.d / "junit.xml",
            "--source-identity", self.d / "source-identity.json",
            "--manifest", self.d / "candidate-linux-x86_64.json", "--out", out)
        self.assertEqual(rc, 1)
        doc = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual({c["id"]: c["status"] for c in doc["cases"]}["portable.ctest"],
                         "mismatch")

    def test_unreadable_inputs_exit_two_without_evidence(self):
        out = self.d / "none.json"
        rc, _, err = self.run_cli(
            "build", "--platform", "linux-x86_64", "--junit", self.d / "nope.xml",
            "--source-identity", self.d / "source-identity.json",
            "--manifest", self.d / "candidate-linux-x86_64.json", "--out", out)
        self.assertEqual(rc, 2)
        self.assertFalse(out.exists())
        self.assertIn("nope.xml", err)

    def test_sanitizer_and_coverage_subcommands(self):
        (self.d / "san.xml").write_text(junit_xml(sanitizer_tests()), encoding="utf-8")
        out = self.d / "san.json"
        rc, stdout, _ = self.run_cli(
            "sanitizer", "--platform", "linux-x86_64", "--junit", self.d / "san.xml",
            "--source-identity", self.d / "source-identity.json", "--out", out)
        self.assertEqual(rc, 0, stdout)
        self.assertEqual(sorted(c["id"] for c in json.loads(out.read_text("utf-8"))["cases"]),
                         ["control.sanitizer.detector", "sanitizer.core"])
        cov = self.d / "coverage.json"
        cov.write_text(json.dumps(SanitizerAndCoverage.coverage(None, "partial")),
                       encoding="utf-8")
        out2 = self.d / "cov.json"
        rc, stdout, _ = self.run_cli(
            "coverage", "--platform", "linux-x86_64", "--coverage", cov,
            "--source-identity", self.d / "source-identity.json", "--out", out2)
        self.assertEqual(rc, 0, stdout)
        self.assertEqual(json.loads(out2.read_text("utf-8"))["cases"][0]["id"],
                         "coverage.production")

    def test_missing_coverage_file_writes_an_unavailable_case(self):
        out = self.d / "cov.json"
        rc, _, _ = self.run_cli(
            "coverage", "--platform", "linux-x86_64", "--coverage", self.d / "none.json",
            "--source-identity", self.d / "source-identity.json", "--out", out)
        self.assertEqual(rc, 1)
        self.assertEqual(json.loads(out.read_text("utf-8"))["cases"][0]["status"],
                         "unavailable")


if __name__ == "__main__":
    unittest.main()
