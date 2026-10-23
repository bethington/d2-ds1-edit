"""CI evidence: turn CTest results into ``ds1edit.case/1`` for the release gate.

A CI job that ran the portable test inventory (``ctest --output-junit``) calls

    python -m scripts.equivalence.ci_evidence build --platform P --junit J \\
        --source-identity source-identity.json --manifest candidate-P.json --out O

and gets one ``ds1edit.case-set/1`` holding a ``ds1edit.case/1`` for every
required release-core entry that a CTest or Python unittest can actually
prove, each bound to the source state and the executables of *that* job.
``sanitizer`` and ``coverage`` do the same for the instrumented Linux jobs.

What a case claims is exactly what the mapping in :data:`RULES` says. Nothing
here invents a pass:

* ``checks`` rules read named tests: a whole CTest (``ctest``), one Unity
  function (``unity``: a ``file:line:test_name:PASS`` line), one Python
  ``unittest`` method (``py``: a ``test_x (mod.Class.test_x) ... ok`` line) or
  one line of a runner's own report (``line``: ``<id>  pass``). Any failing
  check is a ``mismatch``; a check that never ran or was skipped leaves the
  case ``partial``; if none of them ran it is ``not_run``.
* ``portable.ctest`` is the whole inventory, ``portable.python-unittest`` the
  ``py_*`` suites with the counts printed by ``unittest`` itself.
* ``palette-nearest-113c`` is the replay's own case, re-attested to this job.
* Entries that no CI test proves are listed in :data:`UNPROVEN` and stay
  missing; the local lanes (corpus, live, native, performance) are never
  claimed.

Exit status: 0 every case passed, 1 some case did not (the evidence is still
written), 2 unreadable or invalid inputs (nothing is written). Standard
library only.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import re
import sys
from collections import namedtuple
from pathlib import Path
from xml.etree import ElementTree

if __package__ in (None, ""):  # pragma: no cover - direct script execution
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
    from scripts.equivalence import qualify as Q
else:
    from . import contracts as C
    from . import qualify as Q

TOOL = "ci_evidence/1"
ALL = ("windows-x86", "linux-x86_64", "macos-arm64")
WIN = ("windows-x86",)
LINUX = ("linux-x86_64",)

Check = namedtuple("Check", "kind test name")
TestResult = namedtuple("TestResult", "name status output")
Context = namedtuple("Context", "platform source manifest repo_root replay_case")


# ---------------------------------------------------------------------------
# Parsing CTest output
# ---------------------------------------------------------------------------

def parse_junit(source) -> dict:
    """``{test name: TestResult}`` from a ``ctest --output-junit`` file or text."""
    try:
        if isinstance(source, Path) or (isinstance(source, str)
                                        and not source.lstrip().startswith("<")):
            text = Path(source).read_text(encoding="utf-8", errors="replace")
        else:
            text = source
        root = ElementTree.fromstring(text)
    except (OSError, ElementTree.ParseError) as exc:
        raise C.ValidationError("cannot read CTest JUnit results %s: %s" % (source, exc))
    if root.tag not in ("testsuite", "testsuites"):
        raise C.ValidationError("not a JUnit document: <%s>" % root.tag)
    out = {}
    for tc in root.iter("testcase"):
        name = tc.get("name")
        if not name:
            raise C.ValidationError("JUnit testcase without a name")
        if name in out:
            raise C.ValidationError("JUnit results name test %r twice" % name)
        kids = {k.tag for k in tc}
        if kids & {"failure", "error"}:
            status = "fail"
        elif "skipped" in kids or tc.get("status") in ("notrun", "disabled"):
            status = "skipped"
        else:
            status = "pass"
        so = tc.find("system-out")
        out[name] = TestResult(name, status, (so.text or "") if so is not None else "")
    return out


_UNITY = re.compile(r"^.*?:\d+:(\w+):(PASS|FAIL|IGNORE)\b", re.M)
_UNITTEST = re.compile(
    r"^(\w+) \(([\w.]+)\)(?:\n[^\n]*?)? \.\.\. "
    r"(ok|FAIL|ERROR|skipped[^\n]*|expected failure|unexpected success)[ \t]*$", re.M)
_RAN = re.compile(r"^Ran (\d+) tests? in ", re.M)
_SKIPPED = re.compile(r"skipped=(\d+)")
_DECODER = re.compile(r"^(\S+)[ \t]+(pass|mismatch|not_run|unavailable)[ \t]*$", re.M)


def unity_results(output: str) -> dict:
    return {m.group(1): m.group(2) for m in _UNITY.finditer(output)}


def _dotted(method: str, path: str) -> str:
    """``Class.method`` from unittest's ``method (pkg.mod.Class.method)``."""
    parts = path.split(".")
    return ".".join(parts[-2:]) if len(parts) >= 2 else method


def unittest_results(output: str) -> dict:
    """``{Class.method: ok|FAIL|ERROR|skipped|...}`` from ``unittest -v`` output."""
    out = {}
    for m in _UNITTEST.finditer(output):
        v = m.group(3)
        out[_dotted(m.group(1), m.group(2))] = "skipped" if v.startswith("skipped") else v
    return out


_SKIP_REASON = re.compile(r"^skipped '(.*)'$")
_APPLIES_TO = re.compile(r"^applies-to=([a-z0-9_,-]+):")


def unittest_skips(output: str) -> dict:
    """``{Class.method: skip reason}`` for every skipped test."""
    out = {}
    for m in _UNITTEST.finditer(output):
        if m.group(3).startswith("skipped"):
            r = _SKIP_REASON.match(m.group(3))
            out[_dotted(m.group(1), m.group(2))] = r.group(1) if r else ""
    return out


def not_applicable_skips(output: str, platform: str) -> dict:
    """Skips that declare ``applies-to=<platforms>:`` and exclude *platform*.

    A skip is explained only by an explicit, expected platform condition in its
    reason (``unittest.skipUnless(sys.platform == "win32", "applies-to=windows-x86: ...")``).
    Any other skip, or one that claims to apply here, is not.
    """
    out = {}
    for name, reason in unittest_skips(output).items():
        m = _APPLIES_TO.match(reason)
        if m and platform not in m.group(1).split(","):
            out[name] = reason
    return out


def unittest_summary(output: str):
    """``(tests run, tests skipped)`` as ``unittest`` printed them; (0, 0) if absent."""
    ran = _RAN.search(output)
    if not ran:
        return 0, 0
    tail = output[ran.end():]
    sk = _SKIPPED.search(tail.split("\n\n", 2)[1] if "\n\n" in tail else tail)
    return int(ran.group(1)), int(sk.group(1)) if sk else 0


def decoder_lines(output: str) -> dict:
    return {m.group(1): m.group(2) for m in _DECODER.finditer(output)}


# ---------------------------------------------------------------------------
# The mapping: release-core entry -> the tests that prove it
# ---------------------------------------------------------------------------

def _ct(*tests):
    return [Check("ctest", t, "") for t in tests]


def _unity(test, *funcs):
    return [Check("unity", test, f) for f in funcs]


def _py(test, *methods):
    return [Check("py", test, m) for m in methods]


def _line(test, *ids):
    return [Check("line", test, i) for i in ids]


def _rule(cid, platforms, checks=(), *, lane="portable", job="build", type="checks",
          exe=None, features=None, control=False):
    return {"id": cid, "lane": lane, "platforms": tuple(platforms), "job": job,
            "type": type, "checks": list(checks), "exe": exe, "features": features,
            "control": control}


_MATRIX_PROOF = _py("ds1_cases", "ProbeIntegrationTests.test_reader_matches_every_authored_expectation")
_PROBE = "test_visual_reference_probe"
_DECODER_CTEST = "visual_reference_probe_decoder_cases"

RULES = (
    _rule("portable.ctest", ALL, type="all_ctests"),
    _rule("portable.python-unittest", ALL, type="python"),
    _rule("palette-nearest-113c", ALL, _ct("equivalence_palette_replay"), type="replay",
          exe="test_palette_reference_probe"),

    _rule("editor.undo.tiles", ALL, _ct("editor_undo")),
    _rule("editor.edits.objects-paths", ALL, _ct("object_path_edits")),
    _rule("editor.map-isolation", ALL,
          _unity("editor_undo", "test_map_slots_keep_independent_histories")
          + _unity("object_path_edits", "test_object_undo_is_per_map")),
    _rule("ds1.save.backup", ALL, _unity(
        "ds1_save", "test_second_save_keeps_previous_bytes_as_numbered_backup",
        "test_existing_backup_slot_is_left_alone",
        "test_failed_backup_rename_changes_nothing",
        "test_failed_publish_rolls_the_backup_back")),
    _rule("ds1.save.tmp", ALL, _unity(
        "ds1_save", "test_tmp_save_writes_beside_the_map_and_changes_nothing_else",
        "test_failed_tmp_save_leaves_map_and_original_alone")),
    _rule("ds1.save.write-failure", ALL, _unity(
        "ds1_save", "test_short_write_is_reported_and_keeps_the_destination",
        "test_failed_tile_byte_write_is_reported",
        "test_failed_close_is_reported_and_the_handle_released",
        "test_denied_open_is_reported",
        "test_failed_backup_rename_changes_nothing",
        "test_failed_publish_rolls_the_backup_back",
        "test_failed_publish_and_rollback_names_the_backup",
        "test_failed_tmp_save_leaves_map_and_original_alone",
        "test_failed_copy_on_save_keeps_the_original_name",
        "test_legacy_wrapper_returns_the_failure")),
    _rule("project.save-redirection", ALL,
          _ct("resource_resolution")
          + _unity("ds1_save",
                   "test_copy_on_save_writes_into_the_project_and_keeps_the_original",
                   "test_copy_on_save_parent_creation_failure_changes_nothing")
          + _unity("project", "test_redirect_rewrites_into_project",
                   "test_redirect_already_in_project_passes_through",
                   "test_redirect_handles_mpq_extracted_path",
                   "test_redirect_sibling_with_project_name_as_prefix_is_outside",
                   "test_redirect_project_root_itself_is_inside")),

    _rule("ds1.format.matrix", ALL, _ct("ds1_format"), exe="test_ds1_reference_probe",
          features="ds1_matrix"),
    _rule("dc6.rle.basic", ALL, _line(_DECODER_CTEST, "dc6.rle.basic"), exe=_PROBE),
    _rule("dc6.multi_direction", ALL, _line(_DECODER_CTEST, "dc6.multi_direction"),
          exe=_PROBE),
    _rule("dc6.clip_and_zero_copy", ALL, _line(_DECODER_CTEST, "dc6.clip_and_zero_copy"),
          exe=_PROBE),
    _rule("dcc.single_frame", ALL, _line(_DECODER_CTEST, "dcc.single_frame"), exe=_PROBE),
    _rule("dcc.sequence", ALL, _line(_DECODER_CTEST, "dcc.sequence"), exe=_PROBE),
    _rule("dcc.no_equal_cells", ALL, _line(_DECODER_CTEST, "dcc.no_equal_cells"),
          exe=_PROBE),
    _rule("dcc.empty_direction", ALL, _line(_DECODER_CTEST, "dcc.empty_direction"),
          exe=_PROBE),
    _rule("export.images", ALL, _ct("asset_export_smoke"), exe="test_asset_export_smoke",
          features={
              "export-png": _ct("visual_export_images"),
              "export-apng": _ct("visual_export_images"),
              "delays": _ct("visual_export_images"),
              "transparency": _ct("visual_export_images"),
              "cancellation": _ct("export_progress"),
          }),
    _rule("adversarial.core", ALL,
          _ct("adversarial_core", "adversarial_regressions", "adversarial_codecs"),
          exe="test_adversarial_probe"),

    # Wrong-model controls: each is shown detected by a Python test that feeds
    # the deliberately wrong model to the real comparison and requires a diff.
    _rule("control.palette-nearest-113c.constant", WIN, _py(
        "py_equivalence_controls", "ControlTests.test_constant_control_diverges"), control=True),
    _rule("control.palette-nearest-113c.channel-swap", WIN, _py(
        "py_equivalence_controls", "ControlTests.test_channel_swap_control_diverges"), control=True),
    _rule("control.palette-nearest-113c.last-tie", WIN, _py(
        "py_equivalence_controls", "ControlTests.test_last_tie_control_diverges_only_on_ties"),
        control=True),
    _rule("control.palette-nearest-113c.relative-index", WIN, _py(
        "py_equivalence_controls", "ControlTests.test_missing_offset_normalization_diverges"),
        control=True),
    _rule("control.visual-values-113c.seven-bit-mask", WIN, _py(
        "py_visual_values", "TileKeys.test_seven_bit_mask_control_fails"), control=True),
    _rule("control.decoders-113c.wrong-origin", WIN, _py(
        "py_decoder_cases", "Controls.test_wrong_origin"), control=True),
    _rule("control.decoders-113c.dropped-delta-reuse", WIN, _py(
        "py_decoder_cases", "Controls.test_dropped_delta_reuse"), control=True),
    _rule("control.compose.priority-reversal", WIN, _py(
        "py_export_images", "ExportImages.test_priority_reversal_fails"), control=True),
    _rule("control.export.wrong-delay", WIN, _py(
        "py_export_images", "ExportImages.test_wrong_delay_fails_serialized_and_reader_checks"),
        control=True),

    # Linux sanitizer job (ctest -L portable under ASan + UBSan).
    _rule("sanitizer.core", LINUX, lane="sanitizer", job="sanitizer", type="all_ctests"),
    _rule("control.sanitizer.detector", LINUX, _ct("sanitizer_detector_control"),
          lane="sanitizer", job="sanitizer", control=True),
    # Linux coverage job: the ds1edit.coverage/1 report, read by build_coverage_evidence.
    _rule("coverage.production", LINUX, lane="sanitizer", job="coverage", type="coverage"),
)

#: Required portable/sanitizer entries no CI test proves. They are never
#: emitted, so they stay "missing" until a local lane supplies them.
UNPROVEN = {
    "visual-values-113c": (
        "portable replay against the captured original needs a replay adapter and "
        "capture for the visual-values contract; the repository has neither (only "
        "palette-nearest has one), so no CI test proves it"),
    "decoders-113c": (
        "portable replay against the captured original needs a replay adapter and "
        "capture for the decoders contract; the repository has neither (the "
        "authored decoder fixtures prove dc6.*/dcc.*, not this reference "
        "comparison), so no CI test proves it"),
}


def known_executables() -> list:
    names = {r["exe"] for r in RULES if r["exe"]}
    names.add("ds1edit")
    return sorted(names)


def ds1_fixture_features(repo_root) -> list:
    """Profile features the authored DS1 fixture matrix really contains."""
    path = Path(repo_root) / "test" / "fixtures" / "make_ds1_fixtures.py"
    try:
        spec = importlib.util.spec_from_file_location("_ds1edit_ds1_fixtures", path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        feats = ["v%d" % v for v in mod.VERSIONS]
        cases = mod.build_cases()
        if any(c["expected"].get("status") != "error" and (c.get("truncate") is not None
                                                         or c.get("append"))
               for c in cases):
            feats.append("optional-tail")
        return feats
    except (OSError, AttributeError, KeyError, SyntaxError, TypeError, ImportError):
        return []


def _features(rule, repo_root):
    f = rule["features"]
    if f == "ds1_matrix":
        return {name: list(_MATRIX_PROOF) for name in ds1_fixture_features(repo_root)}
    return f or {}


def rule_checks(rule, repo_root) -> list:
    """Every named check a rule depends on (base checks, then feature checks)."""
    out = list(rule["checks"])
    for checks in _features(rule, repo_root).values():
        for ch in checks:
            if ch not in out:
                out.append(ch)
    return out


_ADD_TEST = re.compile(r"add_test\(\s*NAME\s+([A-Za-z0-9_]+)")
_VISUAL_TEST = re.compile(r"ds1edit_visual_add_test\(\s*([A-Za-z0-9_]+)")


def registered_ctest_names(repo_root) -> set:
    """CTest names registered by test/CMakeLists.txt and test/cmake/*.cmake."""
    root = Path(repo_root) / "test"
    names = set()
    files = [root / "CMakeLists.txt"] + sorted((root / "cmake").glob("*.cmake"))
    for f in files:
        if f.is_file():
            text = f.read_text(encoding="utf-8", errors="replace")
            names.update(_ADD_TEST.findall(text))
            names.update(_VISUAL_TEST.findall(text))
    for f in (root / "python").glob("test_*.py"):
        names.add("py_" + f.stem[len("test_"):])
    return names


def ctest_is_registered(name: str, names: set) -> bool:
    return name in names


# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------

def _outcome(check, results) -> str:
    """pass | fail | absent | ignored for one named check."""
    r = results.get(check.test)
    if r is None:
        return "absent"
    if check.kind == "ctest":
        return {"pass": "pass", "fail": "fail"}.get(r.status, "ignored")
    if r.status == "skipped":
        return "ignored"
    if check.kind == "unity":
        v = unity_results(r.output).get(check.name)
        mapped = {"PASS": "pass", "FAIL": "fail", "IGNORE": "ignored"}
    elif check.kind == "py":
        v = unittest_results(r.output).get(check.name)
        mapped = {"ok": "pass", "FAIL": "fail", "ERROR": "fail", "skipped": "ignored",
                  "expected failure": "ignored", "unexpected success": "fail"}
    else:
        v = decoder_lines(r.output).get(check.name)
        mapped = {"pass": "pass", "mismatch": "fail", "not_run": "absent",
                  "unavailable": "absent"}
    if v is None:
        return "fail" if r.status == "fail" else "absent"
    out = mapped[v]
    # A named check can't be better than the CTest that owns it: if that CTest
    # failed or timed out, the executable did not finish cleanly.
    return "fail" if out == "pass" and r.status == "fail" else out


def _describe(check) -> str:
    return "%s %s%s" % (check.kind, check.test, "::" + check.name if check.name else "")


def _sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8", errors="replace")).hexdigest()


def _provenance(rule, ctx) -> dict:
    mf = ctx.manifest or {}
    cand = {"source_state": dict(ctx.source), "build": copy.deepcopy(mf.get("build", {})),
            "packages": []}
    if rule["exe"]:
        cand["executable"] = {"name": rule["exe"],
                              "sha256": (mf.get("executables") or {}).get(rule["exe"])}
    return {"candidate": cand, "tool": TOOL}


def _make_case(rule, ctx, *, ran, failed, skipped, failures, outputs, tests,
               features_covered=None, limitations=()) -> dict:
    case = Q.suite_case(rule["id"], ctx.platform, ran=ran, failed=failed, skipped=skipped,
                        returncode=1 if failed else 0,
                        candidate_provenance=_provenance(rule, ctx)["candidate"],
                        log_sha256=_sha("".join(outputs)), lane=rule["lane"],
                        limitations=list(limitations))
    case["provenance"]["tool"] = TOOL
    cov = case["coverage"]
    cov["tests"] = sorted(tests)
    if failures:
        case["failures"] = [{"kind": "check", "detail": d} for d in failures[:50]]
    if features_covered is not None:
        cov["features_covered"] = sorted(features_covered)
    if rule["control"]:
        cov["control_detected"] = case["status"] == "pass"
    return case


def _eval_checks(rule, results, ctx) -> dict:
    checks = rule_checks(rule, ctx.repo_root)
    outcomes = {ch: _outcome(ch, results) for ch in checks}
    n = lambda v: sum(1 for o in outcomes.values() if o == v)  # noqa: E731
    failures = ["%s failed" % _describe(ch) for ch, o in outcomes.items() if o == "fail"]
    features = _features(rule, ctx.repo_root)
    covered = None
    if rule["features"] is not None:
        covered = [f for f, chs in features.items()
                   if all(outcomes[c] == "pass" for c in chs)]
    notes = ["%s did not run" % _describe(ch) for ch, o in outcomes.items()
             if o in ("absent", "ignored")]
    return _make_case(
        rule, ctx, ran=n("pass") + n("fail"), failed=n("fail"),
        skipped=n("absent") + n("ignored"), failures=failures,
        outputs=[results[c.test].output for c in checks if c.test in results],
        tests={c.test for c in checks}, features_covered=covered,
        limitations=["proved by %d named check(s)" % len(checks)] + notes[:5])


#: Tests that exist only for local lanes (a game runtime, a game install, PD2)
#: and register as "Not Run (Skipped)" stubs, or not at all, in CI. Skipped,
#: they are disclosed rather than counted; a skip of any other test is partial.
LOCAL_ONLY_TESTS = ("golden_core", "display_selftest", "perf_fps",
                    "pd2_inventory", "pd2_sweep", "pd2_render")


def _eval_all_ctests(rule, results, ctx) -> dict:
    local = sorted(n for n, r in results.items()
                   if r.status == "skipped" and n in LOCAL_ONLY_TESTS
                   and ("SKIPPED %s:" % n) in r.output)
    counted = {n: r for n, r in results.items() if n not in local}
    ran = [r for r in counted.values() if r.status in ("pass", "fail")]
    failed = [r for r in ran if r.status == "fail"]
    skipped = [r for r in counted.values() if r.status == "skipped"]
    notes = ["the whole CTest inventory of this job, %d test(s)" % len(counted)]
    if local:
        notes.append("not counted, skipped for want of a local runtime: %s"
                     % ", ".join(local))
    case = _make_case(
        rule, ctx, ran=len(ran), failed=len(failed), skipped=len(skipped),
        failures=["ctest %s failed" % r.name for r in failed],
        outputs=[r.output for r in ran], tests={r.name for r in results.values()},
        limitations=notes)
    if local:
        case["coverage"]["local_only_skipped"] = local
    return case


def _eval_python(rule, results, ctx) -> dict:
    suites = {n: r for n, r in results.items() if n.startswith("py_")}
    ran = skipped = 0
    failures, outputs = [], []
    failed = 0
    explained = {}
    for name, r in sorted(suites.items()):
        outputs.append(r.output)
        n, sk = unittest_summary(r.output)
        if r.status == "fail":
            failed += 1
            failures.append("ctest %s failed" % name)
        elif r.status == "skipped" or n == 0:
            skipped += 1                      # no unittest summary: not counted as run
        else:
            na = not_applicable_skips(r.output, ctx.platform)
            explained.update({"%s %s" % (name, k): v for k, v in na.items()})
            ran += n
            skipped += max(sk - len(na), 0)
    notes = ["unittest counts from %d py_* CTest suite(s)" % len(suites)]
    if explained:
        notes.append("%d test(s) skipped as not applicable on %s (declared applies-to): %s"
                     % (len(explained), ctx.platform, "; ".join(sorted(explained))[:400]))
    case = _make_case(
        rule, ctx, ran=ran, failed=failed, skipped=skipped, failures=failures,
        outputs=outputs, tests=set(suites), limitations=notes)
    if explained:
        case["coverage"]["not_applicable_skips"] = sorted(explained)
    return case


def _git_head(repo_root):
    out = Q._git(Path(repo_root), "rev-parse", "HEAD")
    return out.decode("ascii").strip() if out else None


def _eval_replay(rule, results, ctx) -> dict:
    ct = results.get("equivalence_palette_replay")
    rc = ctx.replay_case
    refuse = None
    if isinstance(rc, dict):
        st = ((rc.get("provenance") or {}).get("candidate") or {}).get("source_state") or {}
        head = _git_head(ctx.repo_root)
        if head is None or st.get("commit") != head:
            refuse = ("the replay ran on commit %s, not this checkout's HEAD %s"
                      % (st.get("commit"), head))
        elif st.get("dirty") is not False:
            refuse = "the replay ran on a dirty working tree"
    if refuse:
        case = _make_case(rule, ctx, ran=0, failed=0, skipped=1, failures=[], outputs=[],
                          tests={"equivalence_palette_replay"}, limitations=[refuse])
        case["failures"] = [{"kind": "not_run", "detail": refuse}]
        return case
    if rc is None or ct is None:
        why = ("no replay case.json was produced" if rc is None else
               "equivalence_palette_replay did not run")
        case = _make_case(rule, ctx, ran=0, failed=0, skipped=1, failures=[], outputs=[],
                          tests={"equivalence_palette_replay"}, limitations=[why])
        case["failures"] = [{"kind": "not_run", "detail": why}]
        return case
    if not isinstance(rc, dict) or rc.get("schema") != C.CASE_SCHEMA \
            or rc.get("id") != rule["id"]:
        raise C.ValidationError("replay case is not a %s for %s (got %r)"
                                % (C.CASE_SCHEMA, rule["id"],
                                   rc.get("id") if isinstance(rc, dict) else rc))
    case = copy.deepcopy(rc)
    prov = case.setdefault("provenance", {})
    cand = prov.setdefault("candidate", {})
    prov["rebound"] = {"by": TOOL, "from_platform": case.get("platform"),
                       "from_source_state": cand.get("source_state")}
    case["platform"] = ctx.platform
    case["lane"] = rule["lane"]
    cand["source_state"] = dict(ctx.source)
    if ct.status != "pass" and case.get("status") == "pass":
        case["status"] = "mismatch"
        case["failures"] = list(case.get("failures") or []) + [
            {"kind": "check", "detail": "ctest equivalence_palette_replay failed"}]
    return case


_EVALUATORS = {"checks": _eval_checks, "all_ctests": _eval_all_ctests,
               "python": _eval_python, "replay": _eval_replay}


def _check_source(source) -> dict:
    if not isinstance(source, dict):
        raise C.ValidationError("source identity must be an object")
    d = source.get("source_state_digest")
    if not isinstance(d, str) or not re.fullmatch(r"[0-9a-f]{64}", d):
        raise C.ValidationError("source identity lacks a source_state_digest")
    return source


def build_evidence(ctx: Context, results: dict, job: str = "build") -> dict:
    """The case-set a ``job`` ("build" or "sanitizer") proves on ``ctx.platform``."""
    _check_source(ctx.source)
    cases = []
    for rule in RULES:
        if rule["job"] == job and ctx.platform in rule["platforms"]:
            cases.append(_EVALUATORS[rule["type"]](rule, results, ctx))
    return {"schema": Q.CASE_SET_SCHEMA, "producer": TOOL, "platform": ctx.platform,
            "job": job, "cases": cases}


def build_coverage_evidence(ctx: Context, doc) -> dict:
    """``coverage.production`` from a ``ds1edit.coverage/1`` document (or None).

    A readable report with at least one instrumented module is the evidence the
    profile asks for (kind ``report``); partial coverage is disclosed in the
    case, never hidden. No readable report is ``unavailable``.
    """
    _check_source(ctx.source)
    rule = next(r for r in RULES if r["type"] == "coverage")
    readable = isinstance(doc, dict) and doc.get("schema") == "ds1edit.coverage/1" \
        and doc.get("status") in ("complete", "partial")
    mods = doc.get("instrumented_modules") if readable else 0
    mods = mods if isinstance(mods, int) and not isinstance(mods, bool) else 0
    case = _make_case(rule, ctx, ran=mods if readable else 0, failed=0, skipped=0,
                      failures=[], outputs=[json.dumps(doc, sort_keys=True)]
                      if readable else [], tests=set())
    cov = case["coverage"]
    cov.pop("tests", None)
    if not readable or mods <= 0:
        case["status"] = "unavailable"
        why = ("no readable ds1edit.coverage/1 report" if not readable else
               "the report has no instrumented production module")
        case["failures"] = [{"kind": "unavailable", "detail": why}]
        case["limitations"] = [why]
        cov["distinct_inputs_compared"] = 0
        return {"schema": Q.CASE_SET_SCHEMA, "producer": TOOL, "platform": ctx.platform,
                "job": "coverage", "cases": [case]}
    totals = doc.get("totals") or {}
    lines = totals.get("lines") or {}
    branches = totals.get("branches") or {}
    unins = doc.get("uninstrumented_modules") or []
    cov.update(report_status=doc["status"], lines_total=lines.get("total"),
               lines_covered=lines.get("covered"), lines_percent=lines.get("percent"),
               branches_total=branches.get("total"), branches_covered=branches.get("covered"),
               uninstrumented_modules=len(unins))
    case["limitations"] = ["coverage is %s: %s" % (doc["status"], doc.get("status_reason", "")),
                           "%d production module(s) uninstrumented" % len(unins),
                           "reports what the portable suites executed; not a threshold"]
    return {"schema": Q.CASE_SET_SCHEMA, "producer": TOOL, "platform": ctx.platform,
            "job": "coverage", "cases": [case]}


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _write(doc: dict, out: Path) -> None:
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(doc, fh, indent=1, sort_keys=True)
        fh.write("\n")


def _load(path: Path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise C.ValidationError("cannot read %s: %s" % (path, exc))


def _summary(doc: dict, out: Path) -> int:
    bad = [c for c in doc["cases"] if c["status"] != c["success_outcome"]]
    print("%s: %d case(s), %d not passing -> %s"
          % (doc["job"], len(doc["cases"]), len(bad), out))
    for c in bad:
        print("  %-48s %s" % (c["id"], c["status"]))
    return 1 if bad else 0


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(prog="python -m scripts.equivalence.ci_evidence",
                                 description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("build", "sanitizer", "coverage"):
        p = sub.add_parser(name)
        p.add_argument("--platform", required=True, choices=ALL)
        p.add_argument("--source-identity", required=True, type=Path)
        p.add_argument("--out", required=True, type=Path)
        p.add_argument("--repo-root", type=Path, default=Path("."))
        if name == "coverage":
            p.add_argument("--coverage", required=True, type=Path)
        else:
            p.add_argument("--junit", required=True, type=Path)
        if name == "build":
            p.add_argument("--manifest", required=True, type=Path)
            p.add_argument("--replay-case", type=Path)
    return ap


def main(argv=None) -> int:
    ap = build_parser()
    try:
        a = ap.parse_args(argv)
    except SystemExit as exc:
        return 2 if exc.code else 0
    try:
        source = _check_source(_load(a.source_identity))
        manifest = _load(a.manifest) if a.cmd == "build" else None
        replay = None
        if a.cmd == "build" and a.replay_case is not None:
            if a.replay_case.is_file():
                replay = _load(a.replay_case)
            else:
                print("no replay case at %s" % a.replay_case, file=sys.stderr)
        ctx = Context(a.platform, source, manifest, a.repo_root, replay)
        if a.cmd == "coverage":
            try:
                doc = _load(a.coverage)
            except C.ValidationError as exc:
                print("coverage report unreadable: %s" % exc, file=sys.stderr)
                doc = None
            evidence = build_coverage_evidence(ctx, doc)
        else:
            evidence = build_evidence(ctx, parse_junit(a.junit),
                                      "build" if a.cmd == "build" else "sanitizer")
    except C.ValidationError as exc:
        print("invalid input: %s" % exc, file=sys.stderr)
        return 2
    _write(evidence, a.out)
    return _summary(evidence, a.out)


if __name__ == "__main__":
    sys.exit(main())
