"""Compare a candidate's results with an independently captured reference.

``compare_case(contract, expected, actual) -> ds1edit.case/1``

* ``contract`` -- a ``ds1edit.contract/1`` object.
* ``expected`` -- a ``ds1edit.capture/1`` object (original-game results).
* ``actual``   -- the candidate's results::

      {"lane": "portable", "platform": "windows-x86",
       "provenance": {"source_state": ..., "executable": {"sha256": ...},
                      "build": {...}, "packages": [...]},
       "vectors": [{"id": ..., "output": {...}}, ...]}

CLI::

    python -m scripts.equivalence.compare --contract PATH --baseline PATH \\
        --capture PATH --actual PATH --out PATH

Exit 0 when the case reaches the contract's declared success outcome, 1 on a
mismatch, 2 when inputs are invalid/unavailable or the comparison is partial or
did not run. Standard library only.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

if __package__ in (None, ""):  # pragma: no cover - direct script execution
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
else:
    from . import contracts as C

#: At most this many failures are listed; the total is always in coverage.
MAX_LISTED_FAILURES = 64

_PROVENANCE_KEYS = ("source_state", "executable", "build", "packages")


def validate_actual(actual: dict) -> dict:
    """Refuse candidate results that lack provenance or have duplicate ids."""
    w = "actual"
    C._nonempty_str(actual, "lane", w)
    C._nonempty_str(actual, "platform", w)
    prov = C._need(actual, "provenance", dict, w)
    for k in _PROVENANCE_KEYS:
        if k not in prov:
            raise C.ValidationError("actual.provenance is missing %r" % k)
    exe = C._need(prov, "executable", dict, "actual.provenance")
    C._hex64(exe, "sha256", "actual.provenance.executable")
    C._need(prov, "packages", list, "actual.provenance")
    vecs = C._need(actual, "vectors", list, w)
    ids = set()
    for i, v in enumerate(vecs):
        vw = "actual.vectors[%d]" % i
        vid = C._nonempty_str(v, "id", vw)
        if vid in ids:
            raise C.ValidationError("%s duplicates vector id %r" % (vw, vid))
        ids.add(vid)
        C._need(v, "output", dict, vw)
    return actual


# ---------------------------------------------------------------------------
# Value comparison
# ---------------------------------------------------------------------------

def _first_json_difference(exp, act, path):
    """(path, expected, actual) of the first difference, or None."""
    if type(exp) is not type(act) and not (
            isinstance(exp, (int, float)) and isinstance(act, (int, float))
            and not isinstance(exp, bool) and not isinstance(act, bool)):
        return path, exp, act
    if isinstance(exp, dict):
        for k in sorted(set(exp) | set(act)):
            if k not in act:
                return "%s.%s" % (path, k), exp[k], None
            if k not in exp:
                return "%s.%s" % (path, k), None, act[k]
            d = _first_json_difference(exp[k], act[k], "%s.%s" % (path, k))
            if d:
                return d
        return None
    if isinstance(exp, list):
        for i in range(min(len(exp), len(act))):
            d = _first_json_difference(exp[i], act[i], "%s[%d]" % (path, i))
            if d:
                return d
        if len(exp) != len(act):
            return "%s.length" % path, len(exp), len(act)
        return None
    return None if exp == act else (path, exp, act)


def _compare_value(otype, exp, act, path):
    if otype == "int":
        if not isinstance(exp, int) or isinstance(exp, bool):
            raise C.ValidationError("%s: expected value is not an integer" % path)
        if not isinstance(act, int) or isinstance(act, bool):
            return path, exp, act
        return None if exp == act else (path, exp, act)
    if otype == "bytes":
        try:
            eb, ab = bytes.fromhex(exp), bytes.fromhex(act)
        except (TypeError, ValueError):
            if not isinstance(exp, str):
                raise C.ValidationError("%s: expected value is not hex bytes" % path)
            return path, exp, act
        for i in range(min(len(eb), len(ab))):
            if eb[i] != ab[i]:
                return "%s[%d]" % (path, i), eb[i], ab[i]
        if len(eb) != len(ab):
            return "%s.length" % path, len(eb), len(ab)
        return None
    return _first_json_difference(exp, act, path)


def _apply_normalizations(contract, name, exp, act):
    """Return (exp, act) after the contract's explicit rules for output *name*."""
    for n in contract["normalizations"]:
        p = n["params"]
        if n["name"] == "index_offset" and p["output"] == name:
            if p.get("applies_to", "expected") == "expected":
                if isinstance(exp, int):
                    exp = exp + p["offset"]
            elif isinstance(act, int):
                act = act + p["offset"]
        elif n["name"] == "mask_pointer" and name in p["outputs"]:
            # Only NULL-ness survives: 0 for NULL, 1 for any address.
            exp = 0 if exp in (0, None) else 1
            act = 0 if act in (0, None) else 1
    return exp, act


# ---------------------------------------------------------------------------
# The case
# ---------------------------------------------------------------------------

def compare_case(contract: dict, expected: dict, actual: dict) -> dict:
    """Compare *actual* with captured *expected* under *contract*.

    Raises :class:`contracts.ValidationError` when any of the three objects is
    unacceptable. Never raises for a product mismatch -- that is a status.
    """
    C.validate_contract(contract)
    C.validate_capture_shape(expected)
    validate_actual(actual)
    if expected["contract_digest"] != contract["digest"]:
        raise C.ValidationError("capture contract_digest does not match the contract")
    if expected["contract_id"] != contract["id"]:
        raise C.ValidationError("capture is for a different contract id")

    success = contract.get("success_outcome", "pass")
    otypes = {o["name"]: o["type"] for o in contract["outputs"]}
    act_by_id = {v["id"]: v for v in actual["vectors"]}
    exp_ids = set()

    failures = []
    mismatched = set()
    distinct = set()
    compared = 0

    def fail(entry):
        failures.append(entry)
        mismatched.add(entry.get("vector"))

    for v in expected["vectors"]:
        vid = v["id"]
        exp_ids.add(vid)
        a = act_by_id.get(vid)
        if a is None:
            fail({"vector": vid, "kind": "missing_actual", "path": "vectors[%s]" % vid,
                  "expected": "present", "actual": None})
            continue
        compared += 1
        distinct.add(C.canonical_json(v["input"]))
        eout, aout = v["output"], a["output"]
        if success == "rejected_expected":
            if aout.get("rejected") is not True:
                fail({"vector": vid, "kind": "not_rejected",
                      "path": "vectors[%s].output.rejected" % vid,
                      "expected": True, "actual": aout.get("rejected")})
            continue
        for name in [o["name"] for o in contract["outputs"]]:
            path = "vectors[%s].output.%s" % (vid, name)
            if name not in aout:
                fail({"vector": vid, "kind": "missing_output", "path": path,
                      "expected": eout.get(name), "actual": None})
                break
            e, x = _apply_normalizations(contract, name, eout[name], aout[name])
            d = _compare_value(otypes[name], e, x, path)
            if d:
                fail({"vector": vid, "kind": "value", "path": d[0],
                      "expected": d[1], "actual": d[2]})
                break

    for v in actual["vectors"]:
        if v["id"] not in exp_ids:
            fail({"vector": v["id"], "kind": "unexpected_actual",
                  "path": "vectors[%s]" % v["id"], "expected": None, "actual": "present"})

    axes = contract["domain"]["axes"]
    uncovered = sorted(n for n, ax in axes.items()
                       if ax["required"] and ax["coverage"] in C.UNCOVERED_AXIS)

    if failures:
        status = "mismatch"
    elif not actual["vectors"] or not distinct:
        status = "not_run"
    elif uncovered:
        status = "partial"
    else:
        status = success

    coverage = {
        "domain": contract["domain"]["description"],
        "axes": {n: ax["coverage"] for n, ax in sorted(axes.items())},
        "uncovered_required_axes": uncovered,
        "vectors_expected": len(expected["vectors"]),
        "vectors_compared": compared,
        "distinct_inputs_compared": len(distinct),
        "vectors_mismatched": len(mismatched),
        "failures_total": len(failures),
    }

    actual_payload = [{"id": v["id"], "output": v["output"]} for v in actual["vectors"]]
    limitations = list(contract["limitations"]) + [
        x for x in expected["limitations"] if x not in contract["limitations"]]

    return {
        "schema": C.CASE_SCHEMA,
        "id": contract["id"],
        "lane": actual["lane"],
        "platform": actual["platform"],
        "required": contract["required"],
        "status": status,
        "success_outcome": success,
        "contract_digest": contract["digest"],
        "baseline_digest": expected["baseline_digest"],
        "input_sha256": expected["input_sha256"],
        "expected_sha256": expected["output_sha256"],
        "actual_sha256": C.payload_sha256(actual_payload),
        "coverage": coverage,
        "failures": failures[:MAX_LISTED_FAILURES],
        "provenance": {
            "candidate": actual["provenance"],
            "reference": {
                "capture_digest": expected["digest"],
                "captured_at": expected["captured_at"],
                "adapter": expected["adapter"],
                "adapter_digest": expected["adapter_digest"],
                "original": expected["original"],
                "evidence": expected["evidence"],
            },
        },
        "limitations": limitations,
    }


def unavailable_case(contract_id: str, reason: str, lane="portable", platform="",
                     required=True, status="unavailable") -> dict:
    """A case for a comparison that could not be made. Never a success."""
    if status not in ("unavailable", "not_run"):
        raise ValueError("unavailable_case only builds unavailable/not_run cases")
    return {
        "schema": C.CASE_SCHEMA, "id": contract_id, "lane": lane, "platform": platform,
        "required": required, "status": status, "success_outcome": None,
        "contract_digest": None, "baseline_digest": None, "input_sha256": None,
        "expected_sha256": None, "actual_sha256": None,
        "coverage": {"vectors_compared": 0, "distinct_inputs_compared": 0},
        "failures": [{"kind": status, "detail": reason}],
        "provenance": {}, "limitations": [reason],
    }


def qualifies(case: dict) -> bool:
    """True only for a required case that reached its declared success outcome."""
    return bool(case.get("required")) and case.get("status") in C.SUCCESS_OUTCOMES \
        and case.get("status") == case.get("success_outcome")


def exit_code(case: dict) -> int:
    status = case.get("status")
    if status in C.SUCCESS_OUTCOMES and status == case.get("success_outcome"):
        return 0
    if status == "mismatch":
        return 1
    return 2


def write_case(case: dict, out: Path) -> None:
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(case, fh, indent=2, sort_keys=True)
        fh.write("\n")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="python -m scripts.equivalence.compare",
                                 description=__doc__.split("\n\n")[0])
    ap.add_argument("--contract", required=True, type=Path)
    ap.add_argument("--baseline", required=True, type=Path)
    ap.add_argument("--capture", required=True, type=Path)
    ap.add_argument("--actual", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    a = ap.parse_args(argv)

    cid = a.contract.stem
    try:
        contract = C.load_contract(a.contract)
        cid = contract["id"]
        baseline = C.load_baseline(a.baseline)
        capture = C.load_capture(a.capture)
        C.validate_capture(capture, contract, baseline)
        actual = C.load_json_object(a.actual)
        case = compare_case(contract, capture, actual)
    except C.ValidationError as exc:
        case = unavailable_case(cid, "invalid input: %s" % exc)
    write_case(case, a.out)
    print("%s: %s (%s distinct inputs compared, %s failures)"
          % (case["id"], case["status"],
             case["coverage"].get("distinct_inputs_compared", 0),
             case["coverage"].get("failures_total", len(case["failures"]))))
    for f in case["failures"][:5]:
        print("  %s" % json.dumps(f, sort_keys=True))
    return exit_code(case)


if __name__ == "__main__":
    sys.exit(main())
