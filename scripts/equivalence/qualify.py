"""Fail-closed release qualification: bind every required result to one candidate.

``qualify(profile, candidate, results) -> ds1edit.qualification/1``

* ``profile``   -- ``ds1edit.release-profile/1`` (``test/equivalence/profiles/``):
  the approved requirements, each mapped to required case IDs per lane and
  platform with finite coverage dimensions.
* ``candidate`` -- ``ds1edit.candidate/1``: the candidate run ID, source commit
  and canonical source-state digest, build identity, each platform's package
  and executable hashes, and the approved contract/baseline/adapter/profile
  digests the evidence must have been produced against.
* ``results``   -- evidence objects: ``ds1edit.case/1`` cases, ``ds1edit.case-set/1``
  and ``ds1edit.package_inspection/1`` containers (only their ``cases`` are
  read; summary counters and container statuses are ignored), plus optional
  ``ds1edit.review/1`` dispositions and ``ds1edit.defects/1`` defect lists.

Readiness is derived from the individual required cases only. A required
(case ID, lane, platform) that is absent, duplicated, not at its declared
success outcome, bound to a different source/executable/package/contract/
baseline/adapter, short of its coverage dimensions, or awaiting review blocks
release. Optional results are reported but never stand in for required ones.
Structurally invalid evidence raises :class:`contracts.ValidationError`.

``source_state_digest(root)`` hashes the declared build inputs of a checkout
(names plus content, line endings normalised by git where available) so that
the qualification record can be found again from a tagged checkout.

CLI::

    python -m scripts.equivalence.qualify --profile PATH --candidate PATH \\
        --results-dir PATH --out PATH

Reads every top-level ``*.json`` in the results directory. Exit 0 ready,
1 not ready, 2 invalid evidence. Standard library only.
"""

from __future__ import annotations

import argparse
import copy
import fnmatch
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

if __package__ in (None, ""):  # pragma: no cover - direct script execution
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
else:
    from . import contracts as C

QUALIFICATION_SCHEMA = "ds1edit.qualification/1"
PROFILE_SCHEMA = "ds1edit.release-profile/1"
CANDIDATE_SCHEMA = "ds1edit.candidate/1"
PLATFORM_MANIFEST_SCHEMA = "ds1edit.candidate-platform/1"
CASE_SET_SCHEMA = "ds1edit.case-set/1"
PACKAGE_INSPECTION_SCHEMA = "ds1edit.package_inspection/1"
REVIEW_SCHEMA = "ds1edit.review/1"
DEFECTS_SCHEMA = "ds1edit.defects/1"
SOURCE_STATE_SCHEMA = "ds1edit.source-state/1"

LANES = ("portable", "corpus", "live", "native-package", "native-workflow",
         "sanitizer", "performance")
KINDS = ("suite", "comparison", "corpus", "negative_control", "package",
         "workflow", "performance", "report")
BINDINGS = ("source", "executable", "package")
REFERENCES = ("contract", "baseline", "adapter")
COVERAGE_KEYS = ("min_distinct_inputs", "complete_accounting", "features")
#: Checks qualify() always applies to the candidate as a whole (requirement 10).
GLOBAL_CHECKS = ("profile_identity", "candidate_identity", "package_hashes",
                 "defects")
#: Performance comparisons that need no maintainer disposition.
REVIEWED_PERFORMANCE = ("baseline", "within_budget")
DISPOSITION_KINDS = ("performance", "defect")

#: Reason code -> report category.
CATEGORIES = {
    "missing": "missing",
    "status_mismatch": "mismatching",
    "not_run": "unexecuted", "unavailable": "unexecuted",
    "partial": "partial", "uncovered_axes": "partial",
    "duplicate": "duplicate",
    "candidate_platform": "stale", "stale_source": "stale",
    "stale_executable": "stale", "stale_package": "stale",
    "stale_contract": "stale", "stale_baseline": "stale", "stale_adapter": "stale",
    "zero_inputs": "coverage_gaps", "capped": "coverage_gaps",
    "incomplete_accounting": "coverage_gaps", "missing_features": "coverage_gaps",
    "control_not_detected": "coverage_gaps",
    "invalid_expected_rejection": "invalid_declarations",
    "required_flag": "invalid_declarations",
    "unreviewed_performance": "unreviewed",
}
CATEGORY_NAMES = ("missing", "mismatching", "stale", "partial", "unexecuted",
                  "duplicate", "coverage_gaps", "invalid_declarations",
                  "unreviewed")

_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_WILDCARD = re.compile(r"[*?\[\]]")


def key(case_id: str, lane: str, platform: str) -> str:
    """Stable name of one required result: ``id@lane/platform``."""
    return "%s@%s/%s" % (case_id, lane, platform)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _str_list(obj, field, where, allowed=None, nonempty=False):
    val = C._need(obj, field, list, where)
    if nonempty and not val:
        raise C.ValidationError("%s.%s is empty" % (where, field))
    for v in val:
        if not isinstance(v, str) or not v:
            raise C.ValidationError("%s.%s must hold non-empty strings" % (where, field))
        if allowed is not None and v not in allowed:
            raise C.ValidationError("%s.%s: %r is not one of %s"
                                    % (where, field, v, ", ".join(allowed)))
    if len(set(val)) != len(val):
        raise C.ValidationError("%s.%s has duplicates" % (where, field))
    return val


def _opt_hex(obj, field, where):
    val = obj.get(field)
    if val is not None and (not isinstance(val, str) or not _HEX64.match(val)):
        raise C.ValidationError("%s.%s is not a SHA-256 hex digest" % (where, field))
    return val


# ---------------------------------------------------------------------------
# Profile
# ---------------------------------------------------------------------------

def _default_binding(lane: str) -> list:
    return ["package"] if lane in ("native-package", "native-workflow") \
        else ["source", "executable"]


def validate_profile(profile: dict) -> dict:
    w = "profile"
    if C._need(profile, "schema", str, w) != PROFILE_SCHEMA:
        raise C.ValidationError("profile schema is %r, expected %r"
                                % (profile.get("schema"), PROFILE_SCHEMA))
    C._nonempty_str(profile, "id", w)
    plats = _str_list(profile, "platforms", w, nonempty=True)
    reqs = C._need(profile, "requirements", list, w)
    if not reqs:
        raise C.ValidationError("profile.requirements is empty")
    rids = set()
    for i, r in enumerate(reqs):
        rw = "profile.requirements[%d]" % i
        rid = C._nonempty_str(r, "id", rw)
        if rid in rids:
            raise C.ValidationError("%s duplicates requirement %r" % (rw, rid))
        rids.add(rid)
        C._nonempty_str(r, "title", rw)
        cases = C._need(r, "cases", list, rw)
        checks = _str_list(r, "checks", rw, GLOBAL_CHECKS) if "checks" in r else []
        if not cases and not checks:
            raise C.ValidationError("%s requires nothing" % rw)
        for j, c in enumerate(cases):
            cw = "%s.cases[%d]" % (rw, j)
            C._nonempty_str(c, "id", cw)
            if C._nonempty_str(c, "lane", cw) not in LANES:
                raise C.ValidationError("%s.lane must be one of %s" % (cw, ", ".join(LANES)))
            for p in _str_list(c, "platforms", cw, nonempty=True):
                if p not in plats:
                    raise C.ValidationError("%s names platform %r outside the profile"
                                            % (cw, p))
            if C._nonempty_str(c, "kind", cw) not in KINDS:
                raise C.ValidationError("%s.kind must be one of %s" % (cw, ", ".join(KINDS)))
            if c.get("success_outcome", "pass") not in C.SUCCESS_OUTCOMES:
                raise C.ValidationError("%s.success_outcome must be one of %s"
                                        % (cw, ", ".join(C.SUCCESS_OUTCOMES)))
            if "binding" in c:
                _str_list(c, "binding", cw, BINDINGS, nonempty=True)
            if "reference" in c:
                _str_list(c, "reference", cw, REFERENCES)
            cov = c.get("coverage", {})
            if not isinstance(cov, dict) or set(cov) - set(COVERAGE_KEYS):
                raise C.ValidationError("%s.coverage may only declare %s"
                                        % (cw, ", ".join(COVERAGE_KEYS)))
            if "features" in cov:
                _str_list(cov, "features", cw + ".coverage", nonempty=True)
            if "min_distinct_inputs" in cov:
                n = C._need(cov, "min_distinct_inputs", int, cw + ".coverage")
                if n < 1:
                    raise C.ValidationError("%s.coverage.min_distinct_inputs < 1" % cw)
    C._check_digest(profile, w)
    return profile


def load_profile(path: Path) -> dict:
    return validate_profile(C.load_json_object(Path(path)))


def expand_profile(profile: dict) -> list:
    """Every required (id, lane, platform) entry, in profile order."""
    out, by_key = [], {}
    for r in profile["requirements"]:
        for c in r["cases"]:
            for p in c["platforms"]:
                cid = c["id"].replace("{platform}", p)
                e = {
                    "key": key(cid, c["lane"], p), "id": cid, "lane": c["lane"],
                    "platform": p, "kind": c["kind"],
                    "success_outcome": c.get("success_outcome", "pass"),
                    "binding": list(c.get("binding", _default_binding(c["lane"]))),
                    "reference": list(c.get("reference", [])),
                    "coverage": copy.deepcopy(c.get("coverage", {})),
                }
                prev = by_key.get(e["key"])
                if prev is None:
                    e["requirements"] = [r["id"]]
                    by_key[e["key"]] = e
                    out.append(e)
                    continue
                if {k: v for k, v in prev.items() if k != "requirements"} != e:
                    raise C.ValidationError("profile declares %s twice with different "
                                            "terms" % e["key"])
                if r["id"] not in prev["requirements"]:
                    prev["requirements"].append(r["id"])
    return out


# ---------------------------------------------------------------------------
# Candidate
# ---------------------------------------------------------------------------

def validate_candidate(candidate: dict) -> dict:
    w = "candidate"
    if C._need(candidate, "schema", str, w) != CANDIDATE_SCHEMA:
        raise C.ValidationError("candidate schema is %r, expected %r"
                                % (candidate.get("schema"), CANDIDATE_SCHEMA))
    C._nonempty_str(candidate, "candidate_run_id", w)
    C._nonempty_str(candidate, "repository", w)
    src = C._need(candidate, "source", dict, w)
    C._nonempty_str(src, "commit", "candidate.source")
    C._hex64(src, "source_state_digest", "candidate.source")
    if "built_from_source_state_digest" not in src:
        raise C.ValidationError("candidate.source lacks built_from_source_state_digest")
    _opt_hex(src, "built_from_source_state_digest", "candidate.source")
    C._need(src, "dirty", bool, "candidate.source")
    _opt_hex(src, "dirty_sha256", "candidate.source")
    C._need(candidate, "build", dict, w)
    plats = C._need(candidate, "platforms", dict, w)
    if not plats:
        raise C.ValidationError("candidate.platforms is empty")
    for p, v in plats.items():
        pw = "candidate.platforms.%s" % p
        pkg = C._need(v, "package", dict, pw)
        C._nonempty_str(pkg, "name", pw + ".package")
        C._hex64(pkg, "sha256", pw + ".package")
        C._need(pkg, "size", int, pw + ".package")
        exes = C._need(v, "executables", dict, pw)
        for name in exes:
            C._hex64(exes, name, pw + ".executables")
    ref = C._need(candidate, "reference", dict, w)
    _opt_hex(ref, "baseline_digest", "candidate.reference")
    for field in ("contracts", "adapters"):
        m = C._need(ref, field, dict, "candidate.reference")
        for name in m:
            C._hex64(m, name, "candidate.reference.%s" % field)
    C._hex64(candidate, "profile_digest", w)
    C._check_digest(candidate, w)
    return candidate


# ---------------------------------------------------------------------------
# Evidence
# ---------------------------------------------------------------------------

def validate_case(case: dict, where: str = "case") -> dict:
    """Strict ``ds1edit.case/1`` shape (package B's schema)."""
    if C._need(case, "schema", str, where) != C.CASE_SCHEMA:
        raise C.ValidationError("%s schema is %r, expected %r"
                                % (where, case.get("schema"), C.CASE_SCHEMA))
    C._nonempty_str(case, "id", where)
    C._nonempty_str(case, "lane", where)
    C._need(case, "platform", str, where)
    C._need(case, "required", bool, where)
    if C._need(case, "status", str, where) not in C.STATUSES:
        raise C.ValidationError("%s.status %r is not one of %s"
                                % (where, case["status"], ", ".join(C.STATUSES)))
    if "success_outcome" not in case:
        raise C.ValidationError("%s is missing required field 'success_outcome'" % where)
    if case["success_outcome"] not in C.SUCCESS_OUTCOMES + (None,):
        raise C.ValidationError("%s.success_outcome %r is not valid"
                                % (where, case["success_outcome"]))
    C._need(case, "coverage", dict, where)
    C._need(case, "failures", list, where)
    C._need(case, "provenance", dict, where)
    C._need(case, "limitations", list, where)
    return case


def _validate_disposition(d: dict, where: str) -> dict:
    did = C._nonempty_str(d, "id", where)
    if _WILDCARD.search(did) or did.strip() != did:
        raise C.ValidationError("%s.id %r is not an exact case/issue ID" % (where, did))
    kind = C._nonempty_str(d, "kind", where)
    if kind not in DISPOSITION_KINDS:
        raise C.ValidationError("%s.kind %r cannot be dispositioned (only %s); required "
                                "mismatches are never excused"
                                % (where, kind, ", ".join(DISPOSITION_KINDS)))
    if kind == "performance":
        C._nonempty_str(d, "platform", where)
    elif d.get("platform") is not None:
        C._nonempty_str(d, "platform", where)
    C._nonempty_str(d, "rationale", where)
    C._nonempty_str(d, "reviewer", where)
    _str_list(d, "evidence", where, nonempty=True)
    return d


def _validate_defect(d: dict, where: str) -> dict:
    did = C._nonempty_str(d, "id", where)
    if _WILDCARD.search(did):
        raise C.ValidationError("%s.id %r is not exact" % (where, did))
    if C._nonempty_str(d, "status", where) not in ("open", "resolved"):
        raise C.ValidationError("%s.status must be open or resolved" % where)
    C._need(d, "data_loss", bool, where)
    C._nonempty_str(d, "summary", where)
    return d


def _collect(results: list):
    """Split evidence objects into cases, dispositions and defects."""
    if not isinstance(results, list):
        raise C.ValidationError("results must be a list of evidence objects")
    cases, disps, defects, artifacts = [], [], [], []
    for i, obj in enumerate(results):
        w = "results[%d]" % i
        if not isinstance(obj, dict):
            raise C.ValidationError("%s is not an object" % w)
        schema = obj.get("schema")
        artifacts.append({"index": i, "schema": schema,
                          "payload_sha256": C.payload_sha256(obj)})
        if schema == C.CASE_SCHEMA:
            cases.append(validate_case(obj, w))
        elif schema in (CASE_SET_SCHEMA, PACKAGE_INSPECTION_SCHEMA):
            # Only the individual cases count; container status/totals are ignored.
            for j, c in enumerate(C._need(obj, "cases", list, w)):
                cases.append(validate_case(c, "%s.cases[%d]" % (w, j)))
        elif schema == REVIEW_SCHEMA:
            for j, d in enumerate(C._need(obj, "dispositions", list, w)):
                disps.append(_validate_disposition(d, "%s.dispositions[%d]" % (w, j)))
        elif schema == DEFECTS_SCHEMA:
            for j, d in enumerate(C._need(obj, "defects", list, w)):
                defects.append(_validate_defect(d, "%s.defects[%d]" % (w, j)))
        else:
            raise C.ValidationError("%s has unsupported evidence schema %r" % (w, schema))
    return cases, disps, defects, artifacts


# ---------------------------------------------------------------------------
# Evaluation of one required entry
# ---------------------------------------------------------------------------

def _reason(code: str, detail: str) -> dict:
    return {"code": code, "detail": detail}


def _find_disposition(disps, kind, did, platform=None):
    for d in disps:
        if d["kind"] == kind and d["id"] == did and \
                (platform is None or d.get("platform") == platform):
            return d
    return None


def _source_bound(state, src) -> bool:
    if not isinstance(state, dict):
        return False
    if "source_state_digest" in state:
        return state["source_state_digest"] == src["source_state_digest"]
    return (state.get("commit") == src["commit"]
            and bool(state.get("dirty")) == src["dirty"]
            and state.get("dirty_sha256") == src["dirty_sha256"])


def _evaluate(entry, case, candidate, disps, used):
    reasons = []
    p = entry["platform"]
    plat = candidate["platforms"].get(p)
    prov = case["provenance"]
    cand_prov = prov.get("candidate") if isinstance(prov.get("candidate"), dict) else {}
    cov = case["coverage"]
    want = entry["success_outcome"]
    status = case["status"]

    if not case["required"]:
        reasons.append(_reason("required_flag", "case is marked optional but the "
                               "profile requires it"))

    # -- status -----------------------------------------------------------
    if status in C.SUCCESS_OUTCOMES:
        if status != want or case["success_outcome"] != want:
            reasons.append(_reason("invalid_expected_rejection",
                                   "status %r / declared success %r; profile requires %r"
                                   % (status, case["success_outcome"], want)))
    elif status == "mismatch":
        n = cov.get("failures_total", len(case["failures"]))
        reasons.append(_reason("status_mismatch", "mismatch (%s failures)" % n))
    else:
        reasons.append(_reason(status, "required case is %s" % status))

    # -- candidate binding ------------------------------------------------
    if plat is None:
        reasons.append(_reason("candidate_platform",
                               "candidate has no %s build" % p))
    else:
        pkg_sha = plat["package"]["sha256"]
        if "source" in entry["binding"] and \
                not _source_bound(cand_prov.get("source_state"), candidate["source"]):
            reasons.append(_reason("stale_source", "case source state %s is not candidate "
                                   "%s" % (json.dumps(cand_prov.get("source_state"),
                                                      sort_keys=True),
                                           candidate["source"]["source_state_digest"])))
        if "executable" in entry["binding"]:
            exe = (cand_prov.get("executable") or {}).get("sha256")
            if exe not in plat["executables"].values():
                reasons.append(_reason("stale_executable",
                                       "executable %s is not a %s candidate executable"
                                       % (exe, p)))
        claimed = set()
        if prov.get("archive_sha256") is not None:
            claimed.add(prov["archive_sha256"])
        for pk in cand_prov.get("packages") or []:
            claimed.add(pk.get("sha256") if isinstance(pk, dict) else pk)
        if entry["lane"] == "native-package":
            claimed.add(case.get("input_sha256"))
        if claimed - {pkg_sha}:
            reasons.append(_reason("stale_package", "package %s is not the qualified %s "
                                   "package %s" % (sorted(map(str, claimed - {pkg_sha})),
                                                   p, pkg_sha)))
        elif "package" in entry["binding"] and pkg_sha not in claimed:
            reasons.append(_reason("stale_package", "case is not bound to package %s"
                                   % pkg_sha))

    # -- reference identities ---------------------------------------------
    ref = candidate["reference"]
    if "contract" in entry["reference"]:
        w = ref["contracts"].get(entry["id"])
        if w is None or case.get("contract_digest") != w:
            reasons.append(_reason("stale_contract", "contract %s, approved %s"
                                   % (case.get("contract_digest"), w)))
    got_base = case.get("baseline_digest")
    if "baseline" in entry["reference"] or got_base is not None:
        w = ref.get("baseline_digest")
        if w is None or got_base != w:
            reasons.append(_reason("stale_baseline", "baseline %s, approved %s"
                                   % (got_base, w)))
    if "adapter" in entry["reference"]:
        w = ref["adapters"].get(entry["id"])
        got = (prov.get("reference") or {}).get("adapter_digest") \
            if isinstance(prov.get("reference"), dict) else None
        if w is None or got != w:
            reasons.append(_reason("stale_adapter", "adapter %s, approved %s" % (got, w)))

    # -- coverage dimensions ----------------------------------------------
    spec = entry["coverage"]
    need = spec.get("min_distinct_inputs", 1)
    n = cov.get("distinct_inputs_compared")
    if not isinstance(n, int) or isinstance(n, bool) or n < need:
        reasons.append(_reason("zero_inputs", "%r distinct inputs, need >= %d" % (n, need)))
    uncovered = cov.get("uncovered_required_axes") or []
    if uncovered:
        reasons.append(_reason("uncovered_axes", "uncovered required axes: %s"
                               % ", ".join(map(str, uncovered))))
    if cov.get("truncated") or cov.get("capped"):
        reasons.append(_reason("capped", "coverage was capped (truncated=%r, capped=%r)"
                               % (cov.get("truncated"), cov.get("capped"))))
    if spec.get("complete_accounting"):
        en, ac, un = (cov.get("inputs_enumerated"), cov.get("inputs_accounted"),
                      cov.get("unclassified", 0))
        if not (isinstance(en, int) and not isinstance(en, bool) and en > 0
                and ac == en and un == 0):
            reasons.append(_reason("incomplete_accounting",
                                   "enumerated %r, accounted %r, unclassified %r"
                                   % (en, ac, un)))
    if spec.get("features"):
        have = cov.get("features_covered") or []
        missing = sorted(set(spec["features"]) - set(have))
        if missing:
            reasons.append(_reason("missing_features", "features not covered: %s"
                                   % ", ".join(missing)))
    if entry["kind"] == "negative_control" and cov.get("control_detected") is not True:
        reasons.append(_reason("control_not_detected", "the wrong-model control was not "
                               "shown to be detected"))
    if entry["kind"] == "performance":
        comp = cov.get("comparison")
        if comp not in REVIEWED_PERFORMANCE:
            d = _find_disposition(disps, "performance", entry["id"], p)
            if d is None:
                reasons.append(_reason("unreviewed_performance",
                                       "performance comparison %r has no review "
                                       "disposition" % comp))
            else:
                used.append(d)
    return reasons


# ---------------------------------------------------------------------------
# qualify()
# ---------------------------------------------------------------------------

def qualify(profile: dict, candidate: dict, results: list) -> dict:
    """Aggregate *results* against *profile* for *candidate*; never trusts totals."""
    validate_profile(profile)
    validate_candidate(candidate)
    cases, disps, defects, artifacts = _collect(results)
    entries = expand_profile(profile)
    req_keys = {e["key"] for e in entries}

    by_key = {}
    for c in cases:
        by_key.setdefault(key(c["id"], c["lane"], c["platform"]), []).append(c)

    used = []
    required, blocking = [], []
    cats = {n: [] for n in CATEGORY_NAMES}
    totals = {}
    limits = []
    coverage = {}
    for e in entries:
        found = by_key.get(e["key"], [])
        case = found[0] if len(found) == 1 else None
        if not found:
            reasons = [_reason("missing", "no result for this required case")]
            status = "missing"
        elif len(found) > 1:
            reasons = [_reason("duplicate", "%d results claim this case" % len(found))]
            status = "duplicate"
        else:
            reasons = _evaluate(e, case, candidate, disps, used)
            status = case["status"]
            coverage[e["key"]] = case["coverage"]
            for lim in case["limitations"]:
                if isinstance(lim, str) and lim not in limits:
                    limits.append(lim)
        ok = not reasons
        required.append({
            "key": e["key"], "id": e["id"], "lane": e["lane"],
            "platform": e["platform"], "kind": e["kind"],
            "requirements": e["requirements"], "status": status,
            "success_outcome": e["success_outcome"], "qualifies": ok,
            "reasons": reasons,
            "case_sha256": C.payload_sha256(case) if case is not None else None,
        })
        t = totals.setdefault("%s/%s" % (e["lane"], e["platform"]),
                              {"required": 0, "qualifying": 0, "blocking": 0,
                               "statuses": {}})
        t["required"] += 1
        t["qualifying" if ok else "blocking"] += 1
        t["statuses"][status] = t["statuses"].get(status, 0) + 1
        if not ok:
            blocking.append({"key": e["key"], "requirements": e["requirements"],
                             "reasons": reasons})
            for r in reasons:
                cat = cats[CATEGORIES[r["code"]]]
                if e["key"] not in cat:
                    cat.append(e["key"])

    optional = []
    for k, found in sorted(by_key.items()):
        if k not in req_keys:
            for c in found:
                optional.append({"key": k, "status": c["status"],
                                 "required_flag": c["required"],
                                 "qualifies_alone": c["status"] in C.SUCCESS_OUTCOMES
                                 and c["status"] == c["success_outcome"]})

    # -- candidate-wide checks (requirement 10) -----------------------------
    glob = []
    if candidate["profile_digest"] != profile["digest"]:
        glob.append(_reason("stale_profile", "candidate approved profile %s; evaluating "
                            "profile %s" % (candidate["profile_digest"], profile["digest"])))
    src = candidate["source"]
    if src["built_from_source_state_digest"] != src["source_state_digest"]:
        glob.append(_reason("unattested_build", "packages were built from %r, not the "
                            "candidate source state %s"
                            % (src["built_from_source_state_digest"],
                               src["source_state_digest"])))
    absent = [p for p in profile["platforms"] if p not in candidate["platforms"]]
    if absent:
        glob.append(_reason("candidate_missing_platform",
                            "candidate lacks packages for %s" % ", ".join(absent)))
    open_defects = []
    for d in defects:
        if d["status"] != "open":
            continue
        open_defects.append(d["id"])
        if d["data_loss"]:
            glob.append(_reason("data_loss_defect", "%s: %s" % (d["id"], d["summary"])))
            continue
        disp = _find_disposition(disps, "defect", d["id"])
        if disp is None:
            glob.append(_reason("unresolved_defect", "%s: %s" % (d["id"], d["summary"])))
        else:
            used.append(disp)

    ready = not blocking and not glob
    reqs = []
    blocked_keys = {b["key"] for b in blocking}
    for r in profile["requirements"]:
        mine = [x["key"] for x in required if r["id"] in x["requirements"]]
        bad = [k for k in mine if k in blocked_keys]
        # A binding requirement holds only when the whole candidate is ready.
        r_ready = ready if r.get("checks") else not bad
        reqs.append({"id": r["id"], "title": r["title"], "spec": r.get("spec"),
                     "ready": r_ready, "required": len(mine), "blocking": bad,
                     "checks": list(r.get("checks", []))})

    used_ids = {id(d) for d in used}
    report = {
        "schema": QUALIFICATION_SCHEMA,
        "ready": ready,
        "profile": {"id": profile["id"], "digest": profile["digest"]},
        "candidate_run_id": candidate["candidate_run_id"],
        "commit": src["commit"],
        "source_state_digest": src["source_state_digest"],
        "package_hashes": {p: v["package"]["sha256"]
                           for p, v in sorted(candidate["platforms"].items())},
        "reference": candidate["reference"],
        "candidate": candidate,
        "required_ids": [e["key"] for e in entries],
        "requirements": reqs,
        "required": required,
        "totals": totals,
        "blocking": blocking,
        "global_reasons": glob,
        **cats,
        "optional": optional,
        "coverage": coverage,
        "limits": limits,
        "review_dispositions": [d for d in disps if id(d) in used_ids],
        "unused_dispositions": [d for d in disps if id(d) not in used_ids],
        "open_defects": open_defects,
        "input_artifacts": artifacts,
        "results": copy.deepcopy(results),
    }
    return C.seal(report)


# ---------------------------------------------------------------------------
# Source identity
# ---------------------------------------------------------------------------

INPUT_DIRS = ("src", "assets/ui", "assets/editor", "cmake", "test", "scripts")
INPUT_FILES = ("CMakeLists.txt", "CMakePresets.json", "vcpkg.json",
               "Ds1edit.ini.sample", "allegro5.cfg", "tools/d2mpq.py",
               "LICENSE", "NOTICE", "README.md")
#: CMake's ``file(GLOB DS1EDIT_AREA_INIS assets/*.ini)`` -- top level only.
INPUT_GLOBS = ("assets/*.ini",)
#: Generated at runtime and excluded by the install rule; never inputs.
GENERATED = ("assets/editor/pal*.bin", "assets/editor/cmap*.bin",
             "assets/editor/gamma.dat", "assets/editor/version")
EXCLUDED_PARTS = (".git", "__pycache__")
EXCLUDED_SUFFIXES = (".pyc", ".pyo")
#: Metadata describing a source state must not change the state it describes.
METADATA_PREFIX = "release/qualification/"
CMAKE_SCANNED = ("CMakeLists.txt", "test/CMakeLists.txt")
_CMAKE_REF = re.compile(r"\$\{CMAKE_SOURCE_DIR\}/([^\s\"'()$;]*)")


def is_source_input(rel: str) -> bool:
    """True when the repository-relative POSIX path is a declared build input."""
    parts = rel.split("/")
    if any(p in EXCLUDED_PARTS for p in parts) or rel.endswith(EXCLUDED_SUFFIXES):
        return False
    if rel.startswith(METADATA_PREFIX):
        return False
    if any(fnmatch.fnmatchcase(rel, g) for g in GENERATED):
        return False
    if rel in INPUT_FILES:
        return True
    if any(fnmatch.fnmatchcase(rel, g) and rel.count("/") == g.count("/")
           for g in INPUT_GLOBS):
        return True
    return any(rel.startswith(d + "/") for d in INPUT_DIRS)


def _declared_reference(rel: str) -> bool:
    rel = rel.rstrip("/")
    if not rel or rel == "bin" or rel.startswith("bin/"):
        return True                       # build output directory, not an input
    if rel in INPUT_GLOBS or rel in INPUT_FILES or is_source_input(rel):
        return True
    if any(rel == d or rel.startswith(d + "/") for d in INPUT_DIRS):
        return True
    roots = INPUT_DIRS + INPUT_FILES + INPUT_GLOBS
    return any(r.startswith(rel + "/") for r in roots)   # an ancestor, e.g. assets/


def _check_cmake_references(root: Path) -> None:
    files = [root / f for f in CMAKE_SCANNED]
    for d in ("cmake", "test/cmake"):
        if (root / d).is_dir():
            files += sorted((root / d).rglob("*.cmake"))
    for f in files:
        if not f.is_file():
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in _CMAKE_REF.finditer(text):
            if not _declared_reference(m.group(1)):
                raise C.ValidationError(
                    "%s reads ${CMAKE_SOURCE_DIR}/%s, which is not a declared "
                    "source-state input" % (f.relative_to(root).as_posix(), m.group(1)))


def _git(root: Path, *args, data=None):
    try:
        r = subprocess.run(["git", "-C", str(root), *args], input=data,
                           capture_output=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return r.stdout if r.returncode == 0 else None


def _git_top(root: Path):
    out = _git(root, "rev-parse", "--show-toplevel")
    if out is None:
        return False
    try:
        return Path(out.decode("utf-8").strip()).resolve() == root.resolve()
    except OSError:
        return False


def _blob_id(data: bytes) -> str:
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def source_inputs(root: Path) -> list:
    """Sorted ``[relative path, git blob id]`` for every declared input."""
    root = Path(root)
    if not (root / "CMakeLists.txt").is_file() or not (root / "src").is_dir():
        raise C.ValidationError("%s is not a DS1Edit source tree" % root)
    _check_cmake_references(root)
    if _git_top(root):
        listed = _git(root, "ls-files", "-z", "-c", "-o", "--exclude-standard")
        if listed is None:
            raise C.ValidationError("git ls-files failed in %s" % root)
        names = sorted({n for n in listed.decode("utf-8").split("\0")
                        if n and is_source_input(n) and (root / n).is_file()})
        if any("\n" in n for n in names):
            raise C.ValidationError("an input path contains a newline")
        # hash-object applies the checkout's eol conversion, so a Windows
        # autocrlf checkout and a Linux checkout of one commit agree.
        out = _git(root, "hash-object", "--stdin-paths",
                   data=("\n".join(names) + "\n").encode("utf-8")) if names else b""
        if out is None:
            raise C.ValidationError("git hash-object failed in %s" % root)
        ids = out.decode("ascii").split()
        if len(ids) != len(names):
            raise C.ValidationError("git hash-object returned %d ids for %d files"
                                    % (len(ids), len(names)))
        return [[n, i] for n, i in zip(names, ids)]
    names = []
    for top in (root,):
        for dirpath, dirnames, filenames in os.walk(top):
            dirnames[:] = [d for d in dirnames if d not in EXCLUDED_PARTS]
            for fn in filenames:
                rel = (Path(dirpath) / fn).relative_to(root).as_posix()
                if is_source_input(rel):
                    names.append(rel)
    return [[n, _blob_id((root / n).read_bytes())] for n in sorted(names)]


def source_state_digest(root: Path) -> str:
    """Canonical digest of the declared build inputs of the checkout at *root*."""
    return C.payload_sha256({"schema": SOURCE_STATE_SCHEMA,
                             "files": source_inputs(Path(root))})


# ---------------------------------------------------------------------------
# Candidate manifests and release verification
# ---------------------------------------------------------------------------

def platform_manifest(platform: str, package: Path, executables: dict,
                      run_id: str, source: dict, build: dict) -> dict:
    """Per-platform candidate manifest a build job uploads beside its package.

    ``source`` is ``{commit, source_state_digest, dirty, dirty_sha256}`` computed
    by the same job that built the package; ``executables`` maps names to files.
    """
    package = Path(package)
    return C.seal({
        "schema": PLATFORM_MANIFEST_SCHEMA, "platform": platform,
        "candidate_run_id": str(run_id), "source": dict(source),
        "build": dict(build),
        "package": {"name": package.name, "sha256": sha256_file(package),
                    "size": package.stat().st_size},
        "executables": {n: sha256_file(Path(p)) for n, p in sorted(executables.items())},
    })


def combine_candidate(manifests: list, repository: str, profile_digest: str,
                      reference: dict) -> dict:
    """One ``ds1edit.candidate/1`` from per-platform manifests of a single run."""
    if not manifests:
        raise C.ValidationError("no platform manifests")
    first = manifests[0]
    plats = {}
    for m in manifests:
        if m.get("schema") != PLATFORM_MANIFEST_SCHEMA:
            raise C.ValidationError("not a platform manifest: %r" % m.get("schema"))
        C._check_digest(m, "platform manifest")
        for f in ("candidate_run_id", "source"):
            if m[f] != first[f]:
                raise C.ValidationError("platform manifests disagree on %s" % f)
        if m["platform"] in plats:
            raise C.ValidationError("two manifests for %s" % m["platform"])
        plats[m["platform"]] = {"package": m["package"], "executables": m["executables"],
                                "build": m["build"]}
    src = first["source"]
    return validate_candidate(C.seal({
        "schema": CANDIDATE_SCHEMA, "candidate_run_id": first["candidate_run_id"],
        "repository": repository,
        "source": {"commit": src["commit"],
                   "source_state_digest": src["source_state_digest"],
                   "built_from_source_state_digest": src["source_state_digest"],
                   "dirty": bool(src.get("dirty")), "dirty_sha256": src.get("dirty_sha256")},
        "build": {"per_platform": True},
        "platforms": plats, "reference": reference, "profile_digest": profile_digest,
    }))


def verify_artifacts(candidate: dict, artifacts: dict) -> list:
    """Problems with downloaded packages ``{platform: path}``; empty when exact."""
    problems = []
    for p, v in sorted(candidate["platforms"].items()):
        want = v["package"]
        path = artifacts.get(p)
        if path is None or not Path(path).is_file():
            problems.append("%s: package %s was not supplied" % (p, want["name"]))
            continue
        path = Path(path)
        if path.name != want["name"]:
            problems.append("%s: file %s is not the qualified %s" % (p, path.name, want["name"]))
        got = sha256_file(path)
        if got != want["sha256"]:
            problems.append("%s: %s sha256 %s is not the qualified %s"
                            % (p, path.name, got, want["sha256"]))
    for p in sorted(set(artifacts) - set(candidate["platforms"])):
        problems.append("%s: unexpected artifact %s" % (p, artifacts[p]))
    return problems


def verify_release_record(record: dict, profile: dict, root: Path,
                          artifacts: dict) -> list:
    """Everything that stops a reviewed record from releasing these artifacts."""
    problems = []
    if not isinstance(record, dict) or record.get("schema") != QUALIFICATION_SCHEMA:
        return ["record is not a %s report" % QUALIFICATION_SCHEMA]
    if record.get("digest") != C.canonical_digest(record):
        problems.append("record digest does not match its content")
    if record.get("ready") is not True:
        problems.append("record is not ready")
    try:
        again = qualify(profile, record["candidate"], record["results"])
    except (C.ValidationError, KeyError, TypeError) as exc:
        return problems + ["record evidence is invalid: %s" % exc]
    if not again["ready"]:
        why = [b["key"] for b in again["blocking"][:10]] +             [g["code"] for g in again["global_reasons"]]
        problems.append("recomputed qualification is not ready: %s" % ", ".join(why))
    if again["required"] != record.get("required"):
        problems.append("record's per-case results differ from their recomputation")
    if record.get("profile", {}).get("digest") != profile.get("digest"):
        problems.append("record was qualified against another profile")
    try:
        here = source_state_digest(root)
    except C.ValidationError as exc:
        problems.append("cannot compute source state: %s" % exc)
    else:
        if here != record.get("source_state_digest"):
            problems.append("checkout source state %s is not the qualified %s"
                            % (here, record.get("source_state_digest")))
    problems += verify_artifacts(record["candidate"], artifacts)
    return problems


# ---------------------------------------------------------------------------
# Helpers for turning suite runs into cases
# ---------------------------------------------------------------------------

def suite_case(case_id: str, platform: str, *, ran: int, failed: int, skipped: int,
               returncode: int, candidate_provenance: dict, log_sha256: str | None,
               lane: str = "portable", limitations=()) -> dict:
    """A ``ds1edit.case/1`` for a whole test-suite run (CTest, unittest).

    Fails closed: any failure or nonzero exit is ``mismatch``, zero tests is
    ``not_run``, and any skip is ``partial``.
    """
    if returncode != 0 or failed:
        status = "mismatch"
    elif ran <= 0:
        status = "not_run"
    elif skipped:
        status = "partial"
    else:
        status = "pass"
    return {
        "schema": C.CASE_SCHEMA, "id": case_id, "lane": lane, "platform": platform,
        "required": True, "status": status, "success_outcome": "pass",
        "contract_digest": None, "baseline_digest": None,
        "input_sha256": log_sha256, "expected_sha256": None, "actual_sha256": None,
        "coverage": {"distinct_inputs_compared": max(ran, 0), "tests_run": ran,
                     "tests_failed": failed, "tests_skipped": skipped,
                     "returncode": returncode, "uncovered_required_axes": []},
        "failures": ([{"kind": "suite", "detail": "%d failed, exit %d" % (failed, returncode)}]
                     if status == "mismatch" else []),
        "provenance": {"candidate": candidate_provenance},
        "limitations": ["counts tests, not distinct behaviour inputs"] + list(limitations),
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def load_results_dir(path: Path):
    path = Path(path)
    if not path.is_dir():
        raise C.ValidationError("results directory %s does not exist" % path)
    results, files = [], []
    for f in sorted(path.glob("*.json")):
        obj = C.load_json_object(f)
        results.append(obj)
        files.append({"path": f.name, "schema": obj.get("schema"),
                      "sha256": sha256_file(f), "payload_sha256": C.payload_sha256(obj)})
    return results, files


def _write(report: dict, out: Path) -> None:
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(report, fh, indent=1, sort_keys=True)
        fh.write("\n")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="python -m scripts.equivalence.qualify",
                                 description=__doc__.split("\n\n")[0])
    ap.add_argument("--profile", required=True, type=Path)
    ap.add_argument("--candidate", required=True, type=Path)
    ap.add_argument("--results-dir", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    try:
        a = ap.parse_args(argv)
    except SystemExit as exc:
        return 2 if exc.code else 0
    try:
        profile = load_profile(a.profile)
        candidate = validate_candidate(C.load_json_object(a.candidate))
        results, files = load_results_dir(a.results_dir)
        report = dict(qualify(profile, candidate, results))
        report["input_artifacts"] = files
        report["input_files"] = {"profile": sha256_file(a.profile),
                                 "candidate": sha256_file(a.candidate)}
        report = C.seal(report)
    except C.ValidationError as exc:
        report = C.seal({"schema": QUALIFICATION_SCHEMA, "ready": False,
                         "invalid_evidence": str(exc)})
        _write(report, a.out)
        print("invalid evidence: %s" % exc, file=sys.stderr)
        print("ready: false (invalid evidence) -> %s" % a.out)
        return 2
    _write(report, a.out)
    n = len(report["required"])
    ok = sum(1 for r in report["required"] if r["qualifies"])
    print("ready: %s  required %d, qualifying %d, blocking %d -> %s"
          % (str(report["ready"]).lower(), n, ok, n - ok, a.out))
    for g in report["global_reasons"]:
        print("  candidate: %s: %s" % (g["code"], g["detail"]))
    for cat in CATEGORY_NAMES:
        if report[cat]:
            print("  %-20s %d" % (cat, len(report[cat])))
    return 0 if report["ready"] else 1


if __name__ == "__main__":
    sys.exit(main())
