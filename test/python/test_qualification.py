"""Fail-closed release qualification (package F task 3).

Every test here builds evidence in memory or under an owned temporary
directory; nothing reads game data, the network, or the user's build tree.

    python -m unittest discover -s test/python -p test_qualification.py -v
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
from contextlib import redirect_stdout, redirect_stderr
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import qualify as Q  # noqa: E402

RELEASE_PROFILE = REPO / "test" / "equivalence" / "profiles" / "release-core.json"
PLATS = ["windows-x86", "linux-x86_64"]


def H(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


SRC = H("source-tree")
COMMIT = "a" * 40


# ---------------------------------------------------------------------------
# Authored fixtures
# ---------------------------------------------------------------------------

def unit_profile() -> dict:
    """A small profile exercising every case kind the release profile uses."""
    return C.seal({
        "schema": Q.PROFILE_SCHEMA,
        "id": "unit-core",
        "platforms": list(PLATS),
        "requirements": [
            {"id": "R1", "title": "portable suites", "spec": "unit",
             "cases": [{"id": "portable.ctest", "lane": "portable",
                        "platforms": list(PLATS), "kind": "suite",
                        "binding": ["source"]}]},
            {"id": "R2", "title": "corpus accounting", "spec": "unit",
             "cases": [{"id": "corpus.ds1.read-all", "lane": "corpus",
                        "platforms": ["windows-x86"], "kind": "corpus",
                        "coverage": {"complete_accounting": True}}]},
            {"id": "R3", "title": "writer features", "spec": "unit",
             "cases": [
                 {"id": "ds1.save.game", "lane": "live",
                  "platforms": ["windows-x86"], "kind": "comparison",
                  "reference": ["contract", "baseline", "adapter"],
                  "coverage": {"features": ["objects", "paths"]}},
                 {"id": "control.ds1.save.game.omitted-field", "lane": "live",
                  "platforms": ["windows-x86"], "kind": "negative_control"}]},
            {"id": "R7", "title": "rejections", "spec": "unit",
             "cases": [{"id": "ds1.reject.truncated", "lane": "portable",
                        "platforms": list(PLATS), "kind": "comparison",
                        "success_outcome": "rejected_expected"}]},
            {"id": "R8", "title": "packages", "spec": "unit",
             "cases": [{"id": "package.{platform}.archive",
                        "lane": "native-package", "platforms": list(PLATS),
                        "kind": "package"}]},
            {"id": "R9", "title": "performance", "spec": "unit",
             "cases": [{"id": "perf.first-open", "lane": "performance",
                        "platforms": ["windows-x86"], "kind": "performance"}]},
            {"id": "R10", "title": "binding", "spec": "unit", "cases": [],
             "checks": list(Q.GLOBAL_CHECKS)},
        ],
    })


def make_candidate(profile: dict, platforms=PLATS, reference=None) -> dict:
    ref = {"baseline_digest": H("baseline"), "contracts": {}, "adapters": {}}
    for e in Q.expand_profile(profile):
        if "contract" in e["reference"]:
            ref["contracts"][e["id"]] = H("contract:" + e["id"])
        if "adapter" in e["reference"]:
            ref["adapters"][e["id"]] = H("adapter:" + e["id"])
    if reference:
        ref.update(reference)
    return C.seal({
        "schema": Q.CANDIDATE_SCHEMA,
        "candidate_run_id": "1001",
        "repository": "owner/d2-ds1-edit",
        "source": {"commit": COMMIT, "source_state_digest": SRC,
                   "built_from_source_state_digest": SRC,
                   "dirty": False, "dirty_sha256": None},
        "build": {"config": "Release", "compiler": "unit"},
        "platforms": {p: {
            "package": {"name": "DS1Edit-9.9.9-%s.zip" % p,
                        "sha256": H("package:" + p), "size": 10},
            "executables": {"ds1edit": H("exe:" + p), "probe": H("probe:" + p)},
        } for p in platforms},
        "reference": ref,
        "profile_digest": profile["digest"],
    })


def passing_case(entry: dict, candidate: dict) -> dict:
    """A qualifying ds1edit.case/1 for one expanded required entry."""
    p = entry["platform"]
    plat = candidate["platforms"][p]
    src = candidate["source"]
    cov = {"distinct_inputs_compared": 3, "uncovered_required_axes": []}
    spec = entry["coverage"]
    if spec.get("complete_accounting"):
        cov.update(inputs_enumerated=5, inputs_accounted=5, unclassified=0)
    if spec.get("features"):
        cov["features_covered"] = list(spec["features"])
    if entry["kind"] == "negative_control":
        cov["control_detected"] = True
    if entry["kind"] == "performance":
        cov["comparison"] = "baseline"
    ref = candidate["reference"]
    case = {
        "schema": C.CASE_SCHEMA, "id": entry["id"], "lane": entry["lane"],
        "platform": p, "required": True,
        "status": entry["success_outcome"],
        "success_outcome": entry["success_outcome"],
        "contract_digest": ref["contracts"].get(entry["id"]),
        "baseline_digest": ref["baseline_digest"]
        if "baseline" in entry["reference"] else None,
        "input_sha256": H("in:" + entry["id"]),
        "expected_sha256": H("exp"), "actual_sha256": H("exp"),
        "coverage": cov, "failures": [], "limitations": [],
        "provenance": {
            "candidate": {
                "source_state": {"commit": src["commit"], "dirty": False,
                                 "dirty_sha256": None,
                                 "source_state_digest": src["source_state_digest"]},
                "executable": {"sha256": plat["executables"]["ds1edit"]},
                "build": candidate["build"], "packages": []},
            "reference": {"adapter_digest": ref["adapters"].get(entry["id"])},
        },
    }
    if "package" in entry["binding"]:
        case["provenance"]["candidate"]["packages"] = [dict(plat["package"])]
    if entry["kind"] == "package":
        case["input_sha256"] = plat["package"]["sha256"]
        case["provenance"] = {"tool": "package_check/1",
                              "archive_sha256": plat["package"]["sha256"]}
    return case


def passing_results(profile: dict, candidate: dict) -> list:
    return [passing_case(e, candidate) for e in Q.expand_profile(profile)]


def find(results, cid, platform=None):
    for r in results:
        if r.get("id") == cid and (platform is None or r.get("platform") == platform):
            return r
    raise KeyError(cid)


def reason_codes(report, cid, lane, platform):
    k = Q.key(cid, lane, platform)
    for e in report["required"]:
        if e["key"] == k:
            return {r["code"] for r in e["reasons"]}
    raise KeyError(k)


def global_codes(report):
    return {r["code"] for r in report["global_reasons"]}


class Base(unittest.TestCase):
    def setUp(self):
        self.profile = unit_profile()
        self.candidate = make_candidate(self.profile)
        self.results = passing_results(self.profile, self.candidate)

    def run_q(self, results=None, candidate=None, profile=None):
        return Q.qualify(profile or self.profile, candidate or self.candidate,
                         self.results if results is None else results)

    def assertBlocked(self, report, cid, lane, platform, code, category=None):
        self.assertFalse(report["ready"])
        self.assertIn(code, reason_codes(report, cid, lane, platform))
        if category:
            self.assertIn(Q.key(cid, lane, platform), report[category])


# ---------------------------------------------------------------------------
# The complete fixture and per-field corruption
# ---------------------------------------------------------------------------

class CompleteFixture(Base):
    def test_complete_profile_qualifies(self):
        report = self.run_q()
        self.assertTrue(report["ready"], json.dumps(report["blocking"], indent=1))
        self.assertEqual(report["schema"], Q.QUALIFICATION_SCHEMA)
        self.assertEqual(report["profile"]["digest"], self.profile["digest"])
        self.assertEqual(report["candidate_run_id"], "1001")
        self.assertEqual(report["source_state_digest"], SRC)
        self.assertEqual(report["blocking"], [])
        n = len(Q.expand_profile(self.profile))
        self.assertEqual(sum(t["required"] for t in report["totals"].values()), n)
        self.assertEqual(sum(t["qualifying"] for t in report["totals"].values()), n)
        self.assertEqual(report["digest"], C.canonical_digest(report))
        self.assertEqual(len(report["input_artifacts"]), len(self.results))
        self.assertTrue(all(r["ready"] for r in report["requirements"]))
        self.assertEqual({p: v["package"]["sha256"] for p, v in
                          self.candidate["platforms"].items()},
                         report["package_hashes"])

    def test_each_single_corruption_fails(self):
        live = ("ds1.save.game", "live", "windows-x86")

        def edit(cid, platform, fn):
            def apply(results):
                fn(find(results, cid, platform))
            return apply

        corruptions = {
            "status->mismatch": edit("portable.ctest", "linux-x86_64",
                                     lambda c: c.update(status="mismatch")),
            "status->not_run": edit("portable.ctest", "windows-x86",
                                    lambda c: c.update(status="not_run")),
            "source digest": edit("portable.ctest", "windows-x86",
                                  lambda c: c["provenance"]["candidate"]["source_state"]
                                  .update(source_state_digest=H("other"), commit="b" * 40)),
            "executable": edit(*live[::2], lambda c: c["provenance"]["candidate"]
                               ["executable"].update(sha256=H("rebuilt"))),
            "package": edit("package.linux-x86_64.archive", "linux-x86_64",
                            lambda c: c.update(input_sha256=H("rezipped"))),
            "contract": edit(*live[::2], lambda c: c.update(contract_digest=H("c2"))),
            "baseline": edit(*live[::2], lambda c: c.update(baseline_digest=H("b2"))),
            "adapter": edit(*live[::2], lambda c: c["provenance"]["reference"]
                            .update(adapter_digest=H("a2"))),
            "platform": edit("portable.ctest", "linux-x86_64",
                             lambda c: c.update(platform="macos-arm64")),
            "lane": edit(*live[::2], lambda c: c.update(lane="portable")),
            "inputs": edit(*live[::2], lambda c: c["coverage"]
                           .update(distinct_inputs_compared=0)),
            "feature": edit(*live[::2], lambda c: c["coverage"]
                            .update(features_covered=["objects"])),
            "control": edit("control.ds1.save.game.omitted-field", "windows-x86",
                            lambda c: c["coverage"].update(control_detected=False)),
            "accounting": edit("corpus.ds1.read-all", "windows-x86",
                               lambda c: c["coverage"].update(inputs_accounted=4)),
        }
        for name, apply in corruptions.items():
            with self.subTest(corruption=name):
                results = copy.deepcopy(self.results)
                apply(results)
                self.assertFalse(self.run_q(results)["ready"], name)

    def test_candidate_corruptions_fail(self):
        def reseal(fn):
            c = copy.deepcopy(dict(self.candidate))
            fn(c)
            return C.seal(c)

        bad = {
            "profile digest": reseal(lambda c: c.update(profile_digest=H("old"))),
            "package hash": reseal(lambda c: c["platforms"]["windows-x86"]["package"]
                                   .update(sha256=H("other zip"))),
            "built-from": reseal(lambda c: c["source"].update(
                built_from_source_state_digest=H("older tree"))),
            "platform set": reseal(lambda c: c["platforms"].pop("linux-x86_64")),
        }
        for name, cand in bad.items():
            with self.subTest(candidate=name):
                self.assertFalse(self.run_q(candidate=cand)["ready"], name)


# ---------------------------------------------------------------------------
# Empty, missing, duplicate, unexpected
# ---------------------------------------------------------------------------

class Accounting(Base):
    def test_empty_result_set(self):
        report = self.run_q([])
        self.assertFalse(report["ready"])
        n = len(Q.expand_profile(self.profile))
        self.assertEqual(len(report["missing"]), n)
        self.assertEqual(sum(t["qualifying"] for t in report["totals"].values()), 0)
        self.assertFalse(any(r["ready"] for r in report["requirements"]
                             if r["id"] != "R10"))

    def test_missing_platform(self):
        results = [r for r in self.results
                   if not (r["id"] == "portable.ctest" and r["platform"] == "linux-x86_64")]
        report = self.run_q(results)
        self.assertBlocked(report, "portable.ctest", "portable", "linux-x86_64",
                           "missing", "missing")
        self.assertEqual(report["totals"]["portable/linux-x86_64"]["qualifying"],
                         report["totals"]["portable/linux-x86_64"]["required"] - 1)

    def test_missing_lane_is_not_satisfied_by_another_lane(self):
        find(self.results, "ds1.save.game")["lane"] = "portable"
        report = self.run_q()
        self.assertBlocked(report, "ds1.save.game", "live", "windows-x86",
                           "missing", "missing")
        self.assertIn(Q.key("ds1.save.game", "portable", "windows-x86"),
                      [o["key"] for o in report["optional"]])

    def test_missing_feature(self):
        find(self.results, "ds1.save.game")["coverage"]["features_covered"] = ["objects"]
        report = self.run_q()
        self.assertBlocked(report, "ds1.save.game", "live", "windows-x86",
                           "missing_features", "coverage_gaps")
        self.assertIn("paths", json.dumps(report["blocking"]))

    def test_mismatched_case_id(self):
        find(self.results, "corpus.ds1.read-all")["id"] = "corpus.ds1.read_all"
        report = self.run_q()
        self.assertBlocked(report, "corpus.ds1.read-all", "corpus", "windows-x86",
                           "missing", "missing")

    def test_duplicate_case_ids_block_even_when_both_pass(self):
        self.results.append(copy.deepcopy(find(self.results, "perf.first-open")))
        report = self.run_q()
        self.assertBlocked(report, "perf.first-open", "performance", "windows-x86",
                           "duplicate", "duplicate")

    def test_optional_pass_cannot_replace_required_missing(self):
        removed = find(self.results, "portable.ctest", "windows-x86")
        self.results.remove(removed)
        extra = copy.deepcopy(removed)
        extra["id"] = "portable.ctest.optional-sweep"
        extra["required"] = False
        self.results.append(extra)
        report = self.run_q()
        self.assertBlocked(report, "portable.ctest", "portable", "windows-x86",
                           "missing", "missing")
        self.assertEqual(len(report["optional"]), 1)

    def test_case_marked_optional_does_not_satisfy_required_entry(self):
        find(self.results, "perf.first-open")["required"] = False
        report = self.run_q()
        self.assertBlocked(report, "perf.first-open", "performance", "windows-x86",
                           "required_flag", "invalid_declarations")


# ---------------------------------------------------------------------------
# Statuses
# ---------------------------------------------------------------------------

class Statuses(Base):
    def test_required_not_run_partial_unavailable_and_mismatch(self):
        cats = {"not_run": "unexecuted", "unavailable": "unexecuted",
                "partial": "partial", "mismatch": "mismatching"}
        for status, category in cats.items():
            with self.subTest(status=status):
                results = copy.deepcopy(self.results)
                find(results, "corpus.ds1.read-all")["status"] = status
                report = self.run_q(results)
                self.assertBlocked(report, "corpus.ds1.read-all", "corpus",
                                   "windows-x86", status if status != "mismatch"
                                   else "status_mismatch", category)

    def test_uncovered_required_axis_is_partial(self):
        find(self.results, "ds1.save.game")["coverage"]["uncovered_required_axes"] = ["layers"]
        report = self.run_q()
        self.assertBlocked(report, "ds1.save.game", "live", "windows-x86",
                           "uncovered_axes", "partial")

    def test_unavailable_case_from_compare_blocks(self):
        from scripts.equivalence import compare as CMP
        self.results.remove(find(self.results, "ds1.save.game"))
        self.results.append(CMP.unavailable_case("ds1.save.game", "no live member",
                                                 lane="live", platform="windows-x86"))
        report = self.run_q()
        self.assertBlocked(report, "ds1.save.game", "live", "windows-x86",
                           "unavailable", "unexecuted")

    def test_rejection_where_pass_is_required_is_invalid_declaration(self):
        c = find(self.results, "portable.ctest", "windows-x86")
        c["status"] = c["success_outcome"] = "rejected_expected"
        report = self.run_q()
        self.assertBlocked(report, "portable.ctest", "portable", "windows-x86",
                           "invalid_expected_rejection", "invalid_declarations")

    def test_undeclared_success_outcome_is_invalid_declaration(self):
        c = find(self.results, "ds1.reject.truncated", "linux-x86_64")
        c["success_outcome"] = "pass"           # claims pass semantics, reports a rejection
        report = self.run_q()
        self.assertBlocked(report, "ds1.reject.truncated", "portable", "linux-x86_64",
                           "invalid_expected_rejection", "invalid_declarations")

    def test_expected_rejection_case_reporting_pass_does_not_qualify(self):
        c = find(self.results, "ds1.reject.truncated", "windows-x86")
        c["status"] = c["success_outcome"] = "pass"
        report = self.run_q()
        self.assertFalse(report["ready"])
        self.assertTrue(reason_codes(report, "ds1.reject.truncated", "portable",
                                     "windows-x86"))

    def test_unknown_status_is_invalid_evidence(self):
        find(self.results, "perf.first-open")["status"] = "ok"
        with self.assertRaises(C.ValidationError):
            self.run_q()

    def test_case_without_schema_is_invalid_evidence(self):
        del find(self.results, "perf.first-open")["schema"]
        with self.assertRaises(C.ValidationError):
            self.run_q()


# ---------------------------------------------------------------------------
# Identity / freshness
# ---------------------------------------------------------------------------

class Identity(Base):
    def test_stale_source(self):
        st = find(self.results, "portable.ctest", "linux-x86_64")["provenance"]["candidate"]["source_state"]
        st.update(source_state_digest=H("yesterday"), commit="c" * 40)
        self.assertBlocked(self.run_q(), "portable.ctest", "portable", "linux-x86_64",
                           "stale_source", "stale")

    def test_source_state_without_digest_matches_commit_and_dirty_digest(self):
        st = find(self.results, "portable.ctest", "linux-x86_64")["provenance"]["candidate"]["source_state"]
        st.pop("source_state_digest")
        self.assertTrue(self.run_q()["ready"])
        st["dirty"], st["dirty_sha256"] = True, H("local edits")
        self.assertBlocked(self.run_q(), "portable.ctest", "portable", "linux-x86_64",
                           "stale_source", "stale")

    def test_stale_executable(self):
        find(self.results, "ds1.save.game")["provenance"]["candidate"]["executable"]["sha256"] = H("x")
        self.assertBlocked(self.run_q(), "ds1.save.game", "live", "windows-x86",
                           "stale_executable", "stale")

    def test_executable_from_other_platform_is_stale(self):
        exe = self.candidate["platforms"]["linux-x86_64"]["executables"]["ds1edit"]
        find(self.results, "ds1.save.game")["provenance"]["candidate"]["executable"]["sha256"] = exe
        self.assertBlocked(self.run_q(), "ds1.save.game", "live", "windows-x86",
                           "stale_executable", "stale")

    def test_stale_package(self):
        c = find(self.results, "package.windows-x86.archive")
        c["input_sha256"] = c["provenance"]["archive_sha256"] = H("rebuilt zip")
        self.assertBlocked(self.run_q(), "package.windows-x86.archive", "native-package",
                           "windows-x86", "stale_package", "stale")

    def test_foreign_package_in_candidate_provenance_is_stale(self):
        find(self.results, "ds1.save.game")["provenance"]["candidate"]["packages"] = [
            {"name": "x.zip", "sha256": H("someone else's zip")}]
        self.assertBlocked(self.run_q(), "ds1.save.game", "live", "windows-x86",
                           "stale_package", "stale")

    def test_stale_contract_baseline_adapter(self):
        for field, code in (("contract", "stale_contract"), ("baseline", "stale_baseline"),
                            ("adapter", "stale_adapter")):
            with self.subTest(field=field):
                results = copy.deepcopy(self.results)
                c = find(results, "ds1.save.game")
                if field == "contract":
                    c["contract_digest"] = H("v2 contract")
                elif field == "baseline":
                    c["baseline_digest"] = H("mixed tile tree")
                else:
                    c["provenance"]["reference"]["adapter_digest"] = H("adapter v0")
                self.assertBlocked(self.run_q(results), "ds1.save.game", "live",
                                   "windows-x86", code, "stale")

    def test_reference_identity_absent_from_candidate_is_stale(self):
        cand = dict(self.candidate)
        cand["reference"] = dict(cand["reference"], contracts={})
        report = self.run_q(candidate=C.seal(cand))
        self.assertBlocked(report, "ds1.save.game", "live", "windows-x86",
                           "stale_contract", "stale")

    def test_stale_profile_identity(self):
        cand = C.seal(dict(self.candidate, profile_digest=H("previous profile")))
        report = self.run_q(candidate=cand)
        self.assertFalse(report["ready"])
        self.assertIn("stale_profile", global_codes(report))

    def test_unattested_build(self):
        cand = copy.deepcopy(dict(self.candidate))
        cand["source"]["built_from_source_state_digest"] = None
        report = self.run_q(candidate=C.seal(cand))
        self.assertFalse(report["ready"])
        self.assertIn("unattested_build", global_codes(report))

    def test_candidate_missing_platform(self):
        cand = C.seal(dict(self.candidate, platforms={
            "windows-x86": self.candidate["platforms"]["windows-x86"]}))
        report = self.run_q(candidate=cand)
        self.assertIn("candidate_missing_platform", global_codes(report))
        self.assertBlocked(report, "portable.ctest", "portable", "linux-x86_64",
                           "candidate_platform", "stale")

    def test_tampered_candidate_digest_is_invalid(self):
        cand = dict(self.candidate)
        cand["candidate_run_id"] = "2002"            # digest not resealed
        with self.assertRaises(C.ValidationError):
            self.run_q(candidate=cand)

    def test_tampered_profile_digest_is_invalid(self):
        prof = copy.deepcopy(dict(self.profile))
        prof["requirements"][0]["cases"][0]["platforms"] = ["windows-x86"]
        with self.assertRaises(C.ValidationError):
            self.run_q(profile=prof)


# ---------------------------------------------------------------------------
# Totals and summary counters
# ---------------------------------------------------------------------------

class Totals(Base):
    def test_forged_case_set_totals_are_ignored(self):
        find(self.results, "corpus.ds1.read-all")["status"] = "mismatch"
        forged = {"schema": Q.CASE_SET_SCHEMA, "status": "pass", "ready": True,
                  "passed": 999, "failed": 0, "totals": {"qualifying": 999},
                  "cases": self.results}
        report = self.run_q([forged])
        self.assertFalse(report["ready"])
        self.assertIn(Q.key("corpus.ds1.read-all", "corpus", "windows-x86"),
                      report["mismatching"])
        n = len(Q.expand_profile(self.profile))
        self.assertEqual(sum(t["qualifying"] for t in report["totals"].values()), n - 1)

    def test_package_inspection_status_is_not_trusted(self):
        win = find(self.results, "package.windows-x86.archive")
        self.results.remove(win)
        win["status"] = "mismatch"
        inspection = {"schema": Q.PACKAGE_INSPECTION_SCHEMA, "status": "pass",
                      "platform": "windows-x86", "cases": [win]}
        report = self.run_q(self.results + [inspection])
        self.assertBlocked(report, "package.windows-x86.archive", "native-package",
                           "windows-x86", "status_mismatch", "mismatching")

    def test_unknown_evidence_schema_is_invalid(self):
        with self.assertRaises(C.ValidationError):
            self.run_q(self.results + [{"schema": "ds1edit.golden/1", "passed": 6}])

    def test_capped_corpus(self):
        find(self.results, "corpus.ds1.read-all")["coverage"]["truncated"] = 100
        self.assertBlocked(self.run_q(), "corpus.ds1.read-all", "corpus", "windows-x86",
                           "capped", "coverage_gaps")

    def test_incomplete_or_empty_accounting(self):
        for cov in ({"inputs_accounted": 4}, {"unclassified": 1},
                    {"inputs_enumerated": 0, "inputs_accounted": 0},
                    {"inputs_enumerated": None}):
            with self.subTest(cov=cov):
                results = copy.deepcopy(self.results)
                find(results, "corpus.ds1.read-all")["coverage"].update(cov)
                self.assertBlocked(self.run_q(results), "corpus.ds1.read-all",
                                   "corpus", "windows-x86", "incomplete_accounting",
                                   "coverage_gaps")

    def test_zero_distinct_inputs(self):
        for value in (0, None):
            with self.subTest(value=value):
                results = copy.deepcopy(self.results)
                cov = find(results, "perf.first-open")["coverage"]
                if value is None:
                    del cov["distinct_inputs_compared"]
                else:
                    cov["distinct_inputs_compared"] = value
                self.assertBlocked(self.run_q(results), "perf.first-open",
                                   "performance", "windows-x86", "zero_inputs",
                                   "coverage_gaps")

    def test_absent_negative_control_detection(self):
        for value in (False, None):
            with self.subTest(value=value):
                results = copy.deepcopy(self.results)
                cov = find(results, "control.ds1.save.game.omitted-field")["coverage"]
                if value is None:
                    del cov["control_detected"]
                else:
                    cov["control_detected"] = value
                self.assertBlocked(self.run_q(results),
                                   "control.ds1.save.game.omitted-field", "live",
                                   "windows-x86", "control_not_detected",
                                   "coverage_gaps")


# ---------------------------------------------------------------------------
# Defects, performance and review dispositions
# ---------------------------------------------------------------------------

def disposition(**kw):
    d = {"id": "perf.first-open", "platform": "windows-x86", "kind": "performance",
         "rationale": "first-open regressed by the new MPQ index; accepted for 2.2",
         "evidence": ["tmp/perf/run-17/summary.json"], "reviewer": "maintainer"}
    d.update(kw)
    return d


class Reviews(Base):
    def review(self, *disps):
        return {"schema": Q.REVIEW_SCHEMA, "dispositions": list(disps)}

    def defects(self, *items):
        return {"schema": Q.DEFECTS_SCHEMA, "defects": list(items)}

    def test_unreviewed_performance_regression_blocks(self):
        for comparison in ("unreviewed", "regression", None):
            with self.subTest(comparison=comparison):
                results = copy.deepcopy(self.results)
                cov = find(results, "perf.first-open")["coverage"]
                if comparison is None:
                    del cov["comparison"]
                else:
                    cov["comparison"] = comparison
                self.assertBlocked(self.run_q(results), "perf.first-open",
                                   "performance", "windows-x86",
                                   "unreviewed_performance", "unreviewed")

    def test_exact_performance_disposition_accepts(self):
        find(self.results, "perf.first-open")["coverage"]["comparison"] = "regression"
        report = self.run_q(self.results + [self.review(disposition())])
        self.assertTrue(report["ready"], report["blocking"])
        self.assertEqual(report["review_dispositions"][0]["id"], "perf.first-open")

    def test_disposition_for_other_platform_does_not_apply(self):
        find(self.results, "perf.first-open")["coverage"]["comparison"] = "regression"
        report = self.run_q(self.results + [self.review(disposition(platform="linux-x86_64"))])
        self.assertFalse(report["ready"])

    def test_wildcard_and_unexplained_dispositions_are_invalid(self):
        for bad in (disposition(id="perf.*"), disposition(id="perf.first-open?"),
                    disposition(rationale=""), disposition(evidence=[]),
                    disposition(kind="mismatch"), disposition(reviewer="")):
            with self.subTest(bad=bad):
                with self.assertRaises(C.ValidationError):
                    self.run_q(self.results + [self.review(bad)])

    def test_dispositions_cannot_excuse_a_required_mismatch(self):
        find(self.results, "corpus.ds1.read-all")["status"] = "mismatch"
        d = disposition(id="corpus.ds1.read-all", kind="defect", platform=None)
        report = self.run_q(self.results + [self.review(d)])
        self.assertFalse(report["ready"])

    def test_unresolved_defect_blocks(self):
        report = self.run_q(self.results + [self.defects(
            {"id": "BUG-12", "status": "open", "data_loss": False,
             "summary": "object path order differs after save"})])
        self.assertFalse(report["ready"])
        self.assertIn("unresolved_defect", global_codes(report))

    def test_resolved_defect_does_not_block(self):
        report = self.run_q(self.results + [self.defects(
            {"id": "BUG-12", "status": "resolved", "data_loss": True,
             "summary": "fixed", "resolution": "commit abc"})])
        self.assertTrue(report["ready"], report["global_reasons"])

    def test_reviewed_non_data_loss_defect_is_accepted(self):
        report = self.run_q(self.results + [
            self.defects({"id": "BUG-12", "status": "open", "data_loss": False,
                          "summary": "cosmetic"}),
            self.review(disposition(id="BUG-12", kind="defect", platform=None))])
        self.assertTrue(report["ready"], report["global_reasons"])

    def test_data_loss_defect_cannot_be_dispositioned(self):
        report = self.run_q(self.results + [
            self.defects({"id": "BUG-9", "status": "open", "data_loss": True,
                          "summary": "save truncates paths"}),
            self.review(disposition(id="BUG-9", kind="defect", platform=None))])
        self.assertFalse(report["ready"])
        self.assertIn("data_loss_defect", global_codes(report))


# ---------------------------------------------------------------------------
# Source identity and artifact hashes
# ---------------------------------------------------------------------------

class SourceDigest(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix="ds1edit qualify src "))
        self.addCleanup(shutil.rmtree, self.root, True)
        files = {
            "CMakeLists.txt": "install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION .)\n"
                              "file(GLOB X \"${CMAKE_SOURCE_DIR}/assets/*.ini\")\n"
                              "copy ${CMAKE_SOURCE_DIR}/bin/assets\n",
            "CMakePresets.json": "{}", "vcpkg.json": "{}",
            "LICENSE": "MIT", "NOTICE": "n", "README.md": "r",
            "Ds1edit.ini.sample": "s", "allegro5.cfg": "c",
            "src/main.c": "int main(void){return 0;}\n",
            "src/core/ds1.c": "/* ds1 */\n",
            "assets/act1_town.ini": "x", "assets/ui/cursor.png": "png",
            "assets/editor/obj.txt": "obj", "assets/editor/ds1edit.dt1": "dt1",
            "cmake/copy.cmake": "", "test/test_x.c": "", "scripts/s.py": "",
            "tools/d2mpq.py": "",
        }
        for rel, text in files.items():
            p = self.root / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8")

    def digest(self):
        return Q.source_state_digest(self.root)

    def test_digest_is_stable_and_hex(self):
        d = self.digest()
        self.assertRegex(d, r"^[0-9a-f]{64}$")
        self.assertEqual(d, self.digest())

    def test_source_edit_changes_digest(self):
        before = self.digest()
        (self.root / "src/core/ds1.c").write_text("/* ds1 v2 */\n", encoding="utf-8")
        self.assertNotEqual(before, self.digest())

    def test_rename_and_new_input_change_digest(self):
        before = self.digest()
        (self.root / "src/core/ds1.c").rename(self.root / "src/core/ds1b.c")
        renamed = self.digest()
        self.assertNotEqual(before, renamed)
        (self.root / "assets/act9.ini").write_text("y", encoding="utf-8")
        self.assertNotEqual(renamed, self.digest())

    def test_qualification_metadata_and_outputs_do_not_change_digest(self):
        before = self.digest()
        meta = self.root / "release" / "qualification"
        meta.mkdir(parents=True)
        (meta / (before + ".json")).write_text("{}", encoding="utf-8")
        (meta / "README.md").write_text("doc", encoding="utf-8")
        for rel in ("assets/editor/pal0.bin", "assets/editor/cmap2_0_0_x.bin",
                    "assets/editor/gamma.dat", "assets/editor/version",
                    "scripts/__pycache__/s.cpython-313.pyc", "tmp/out.json",
                    "bin/ds1edit.exe", "docs/notes.md", "assets/tiles/act1/x.dt1"):
            p = self.root / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b"generated")
        self.assertEqual(before, self.digest())

    def test_undeclared_cmake_dependency_is_rejected(self):
        with open(self.root / "CMakeLists.txt", "a", encoding="utf-8") as fh:
            fh.write("install(FILES ${CMAKE_SOURCE_DIR}/docs/manual.pdf DESTINATION .)\n")
        with self.assertRaises(C.ValidationError):
            self.digest()

    def test_not_a_source_tree(self):
        with self.assertRaises(C.ValidationError):
            Q.source_state_digest(self.root / "src")


class ArtifactHashes(Base):
    def setUp(self):
        super().setUp()
        self.dir = Path(tempfile.mkdtemp(prefix="ds1edit qualify artifacts "))
        self.addCleanup(shutil.rmtree, self.dir, True)
        plats = {}
        for p in PLATS:
            f = self.dir / ("DS1Edit-9.9.9-%s.zip" % p)
            f.write_bytes(b"package bytes for " + p.encode())
            plats[p] = {"package": {"name": f.name, "sha256": Q.sha256_file(f),
                                    "size": f.stat().st_size},
                        "executables": {"ds1edit": H("exe:" + p)}}
        cand = dict(self.candidate, platforms=plats)
        self.candidate = C.seal(cand)
        self.results = passing_results(self.profile, self.candidate)

    def artifacts(self):
        return {p: self.dir / ("DS1Edit-9.9.9-%s.zip" % p) for p in PLATS}

    def test_exact_artifacts_verify(self):
        self.assertEqual(Q.verify_artifacts(self.candidate, self.artifacts()), [])

    def test_rebuilt_zip_bytes_fail(self):
        arts = self.artifacts()
        arts["windows-x86"].write_bytes(b"rebuilt from the same tag, different bytes")
        problems = Q.verify_artifacts(self.candidate, arts)
        self.assertTrue(any("windows-x86" in p and "sha256" in p for p in problems), problems)

    def test_missing_or_renamed_artifact_fails(self):
        arts = self.artifacts()
        del arts["linux-x86_64"]
        self.assertTrue(Q.verify_artifacts(self.candidate, arts))
        arts = self.artifacts()
        moved = self.dir / "renamed.zip"
        arts["windows-x86"].rename(moved)
        arts["windows-x86"] = moved
        self.assertTrue(Q.verify_artifacts(self.candidate, arts))

    def test_release_record_round_trip_and_tamper(self):
        report = self.run_q()
        self.assertTrue(report["ready"], report["blocking"])
        root = Path(tempfile.mkdtemp(prefix="ds1edit qualify root "))
        self.addCleanup(shutil.rmtree, root, True)
        src = root / "src"
        src.mkdir()
        (src / "main.c").write_text("x", encoding="utf-8")
        (root / "CMakeLists.txt").write_text("", encoding="utf-8")
        digest = Q.source_state_digest(root)
        cand = copy.deepcopy(dict(self.candidate))
        cand["source"]["source_state_digest"] = digest
        cand["source"]["built_from_source_state_digest"] = digest
        cand = C.seal(cand)
        record = Q.qualify(self.profile, cand, passing_results(self.profile, cand))
        self.assertTrue(record["ready"], record["blocking"])
        self.assertEqual(Q.verify_release_record(record, self.profile, root,
                                                 self.artifacts()), [])

        tampered = copy.deepcopy(record)
        tampered["results"][0]["status"] = "mismatch"
        self.assertTrue(Q.verify_release_record(C.seal(tampered), self.profile, root,
                                                self.artifacts()))
        unsealed = copy.deepcopy(record)
        unsealed["ready"] = True
        unsealed["candidate_run_id"] = "999"
        self.assertTrue(Q.verify_release_record(unsealed, self.profile, root,
                                                self.artifacts()))
        (src / "main.c").write_text("changed after qualification", encoding="utf-8")
        self.assertTrue(Q.verify_release_record(record, self.profile, root,
                                                self.artifacts()))

    def test_not_ready_record_is_refused(self):
        root = Path(tempfile.mkdtemp(prefix="ds1edit qualify root "))
        self.addCleanup(shutil.rmtree, root, True)
        (root / "src").mkdir()
        (root / "CMakeLists.txt").write_text("", encoding="utf-8")
        record = self.run_q([])
        problems = Q.verify_release_record(record, self.profile, root, self.artifacts())
        self.assertTrue(any("not ready" in p for p in problems), problems)


# ---------------------------------------------------------------------------
# The real release profile
# ---------------------------------------------------------------------------

class ReleaseProfile(unittest.TestCase):
    def setUp(self):
        self.profile = Q.load_profile(RELEASE_PROFILE)

    def test_maps_all_ten_requirements_on_three_platforms(self):
        ids = [r["id"] for r in self.profile["requirements"]]
        self.assertEqual(ids, ["R%d" % i for i in range(1, 11)])
        self.assertEqual(sorted(self.profile["platforms"]),
                         ["linux-x86_64", "macos-arm64", "windows-x86"])
        for r in self.profile["requirements"]:
            self.assertTrue(r["cases"] or r.get("checks"), r["id"])
            self.assertTrue(r.get("spec"), r["id"])
        entries = Q.expand_profile(self.profile)
        lanes = {e["lane"] for e in entries}
        for lane in ("portable", "corpus", "live", "native-package",
                     "native-workflow", "sanitizer", "performance"):
            self.assertIn(lane, lanes)
        for p in self.profile["platforms"]:
            self.assertTrue(any(e["platform"] == p and e["lane"] == "portable"
                                for e in entries), p)
            self.assertTrue(any(e["platform"] == p and e["lane"] == "native-package"
                                for e in entries), p)
        self.assertTrue(any(e["kind"] == "negative_control" for e in entries))
        self.assertTrue(any(e["coverage"].get("complete_accounting") for e in entries))

    def test_names_writer_features_visual_maps_and_package_checks(self):
        entries = Q.expand_profile(self.profile)
        feats = set()
        for e in entries:
            feats.update(e["coverage"].get("features", []))
        for f in ("tile-layers", "orientations", "objects", "paths", "groups-tags",
                  "format-upgrade", "edited-values", "act1", "act5", "dcc", "dc6",
                  "dt1-rle", "cof-priority"):
            self.assertIn(f, feats)
        ids = {e["id"] for e in entries}
        for m in ("act1_town", "act1_cave", "act2_desert", "act3_jungle",
                  "act4_fortress", "act5_town"):
            self.assertIn("visual.core." + m, ids)
        self.assertIn("package.macos-arm64.macos_app", ids)
        self.assertNotIn("package.windows-x86.macos_app", ids)
        self.assertIn("palette-nearest-113c", ids)

    def test_empty_results_are_not_ready(self):
        cand = make_candidate(self.profile, platforms=self.profile["platforms"])
        report = Q.qualify(self.profile, cand, [])
        self.assertFalse(report["ready"])
        self.assertEqual(len(report["missing"]), len(Q.expand_profile(self.profile)))

    def test_profile_is_satisfiable_by_complete_evidence(self):
        cand = make_candidate(self.profile, platforms=self.profile["platforms"])
        report = Q.qualify(self.profile, cand, passing_results(self.profile, cand))
        self.assertTrue(report["ready"], report["blocking"][:5])


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

class Cli(Base):
    def setUp(self):
        super().setUp()
        self.dir = Path(tempfile.mkdtemp(prefix="ds1edit qualify cli "))
        self.addCleanup(shutil.rmtree, self.dir, True)
        self.prof = self.dir / "profile.json"
        self.cand = self.dir / "candidate.json"
        self.res = self.dir / "results dir"
        self.res.mkdir()
        self.prof.write_text(json.dumps(self.profile), encoding="utf-8")
        self.cand.write_text(json.dumps(self.candidate), encoding="utf-8")
        self.out = self.dir / "out" / "qualification.json"

    def write_results(self, results):
        (self.res / "cases.json").write_text(json.dumps(
            {"schema": Q.CASE_SET_SCHEMA, "cases": results}), encoding="utf-8")

    def cli(self):
        buf = io.StringIO()
        with redirect_stdout(buf), redirect_stderr(buf):
            code = Q.main(["--profile", str(self.prof), "--candidate", str(self.cand),
                           "--results-dir", str(self.res), "--out", str(self.out)])
        return code, json.loads(self.out.read_text(encoding="utf-8"))

    def test_ready_exits_zero(self):
        self.write_results(self.results)
        code, report = self.cli()
        self.assertEqual(code, 0)
        self.assertTrue(report["ready"])
        self.assertEqual(report["input_artifacts"][0]["path"], "cases.json")
        self.assertRegex(report["input_artifacts"][0]["sha256"], "^[0-9a-f]{64}$")

    def test_not_ready_exits_one(self):
        self.write_results(self.results[1:])
        code, report = self.cli()
        self.assertEqual(code, 1)
        self.assertFalse(report["ready"])

    def test_empty_results_dir_exits_one(self):
        code, report = self.cli()
        self.assertEqual(code, 1)
        self.assertFalse(report["ready"])

    def test_invalid_evidence_exits_two(self):
        (self.res / "junk.json").write_text("{not json", encoding="utf-8")
        code, report = self.cli()
        self.assertEqual(code, 2)
        self.assertFalse(report["ready"])
        self.assertIn("invalid_evidence", report)

    def test_missing_candidate_exits_two(self):
        self.cand.unlink()
        self.write_results(self.results)
        code, report = self.cli()
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main()
