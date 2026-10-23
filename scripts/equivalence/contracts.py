"""Shared evidence interfaces for original-game equivalence testing.

Standard library only. Portable CI imports this module, so it must never import
MPQ readers, fleet transports, or anything that needs a game installation.

Schemas defined here (consumed by later packages -- do not change meanings):

* ``ds1edit.contract/1`` -- what is compared, against which original function.
* ``ds1edit.baseline/1`` -- the identified vanilla 1.13c binaries/archives/assets.
* ``ds1edit.capture/1``  -- independently captured original-game results.
* ``ds1edit.case/1``     -- one comparison verdict (built in ``compare.py``).

Every object that carries a ``digest`` field is sealed with
:func:`canonical_digest`, which hashes the object's canonical JSON with only its
own top-level ``digest`` omitted.
"""

from __future__ import annotations

import datetime
import hashlib
import json
import re
from pathlib import Path
from typing import Any, Iterable

CONTRACT_SCHEMA = "ds1edit.contract/1"
BASELINE_SCHEMA = "ds1edit.baseline/1"
CAPTURE_SCHEMA = "ds1edit.capture/1"
CASE_SCHEMA = "ds1edit.case/1"

BASELINE_BUILD = "vanilla-113c"

#: Case statuses. Nothing else is a valid verdict.
STATUSES = ("pass", "mismatch", "unavailable", "not_run", "partial",
            "rejected_expected")

#: The only outcomes a contract may declare as its success.
SUCCESS_OUTCOMES = ("pass", "rejected_expected")

#: Domain-axis coverage labels. ``partial``/``unknown`` on a required axis
#: demote an otherwise agreeing comparison to ``partial``.
AXIS_COVERAGE = ("exhaustive", "sampled", "pinned", "partial", "unknown")
UNCOVERED_AXIS = ("partial", "unknown")

#: Output value types a contract may declare.
OUTPUT_TYPES = ("int", "bytes", "json")

_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_RVA = re.compile(r"^0x[0-9a-f]+$")
_KIND = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
_UTC = re.compile(r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(\.\d+)?Z$")


class ValidationError(ValueError):
    """A contract, baseline, capture, or actual result is not acceptable.

    Raised for missing metadata, wrong identities, bad digests, and unknown
    normalizations. Never used to report a product mismatch.
    """


# ---------------------------------------------------------------------------
# Canonical JSON and digests
# ---------------------------------------------------------------------------

def canonical_json(value: Any) -> bytes:
    """UTF-8 JSON with sorted keys and compact separators."""
    try:
        text = json.dumps(value, sort_keys=True, separators=(",", ":"),
                          ensure_ascii=False, allow_nan=False)
    except (TypeError, ValueError) as exc:
        raise ValidationError("value is not canonical-JSON serialisable: %s" % exc)
    return text.encode("utf-8")


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def payload_sha256(value: Any) -> str:
    """SHA-256 of a payload's canonical JSON (no field is omitted)."""
    return sha256_hex(canonical_json(value))


def canonical_digest(value: dict) -> str:
    """SHA-256 of *value*'s canonical JSON, omitting only its own ``digest``."""
    if not isinstance(value, dict):
        raise ValidationError("canonical_digest needs a JSON object")
    body = {k: v for k, v in value.items() if k != "digest"}
    return sha256_hex(canonical_json(body))


def seal(value: dict) -> dict:
    """Return a copy of *value* with its ``digest`` field set."""
    out = dict(value)
    out["digest"] = canonical_digest(out)
    return out


def utc_now() -> str:
    """Current UTC time in the capture timestamp format."""
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


def vector_inputs_payload(vectors: Iterable[dict]) -> list:
    """The payload ``input_sha256`` hashes: ``[{id, input}]`` in vector order."""
    return [{"id": v["id"], "input": v["input"]} for v in vectors]


def vector_outputs_payload(vectors: Iterable[dict]) -> list:
    """The payload ``output_sha256`` hashes: ``[{id, output}]`` in vector order."""
    return [{"id": v["id"], "output": v["output"]} for v in vectors]


# ---------------------------------------------------------------------------
# Small field checkers
# ---------------------------------------------------------------------------

def _need(obj: dict, key: str, typ, where: str):
    if not isinstance(obj, dict):
        raise ValidationError("%s is not an object" % where)
    if key not in obj:
        raise ValidationError("%s is missing required field %r" % (where, key))
    val = obj[key]
    if typ is int and isinstance(val, bool):
        raise ValidationError("%s.%s must be an integer" % (where, key))
    if not isinstance(val, typ):
        raise ValidationError("%s.%s has the wrong type (%s)"
                              % (where, key, type(val).__name__))
    return val


def _hex64(obj: dict, key: str, where: str) -> str:
    val = _need(obj, key, str, where)
    if not _HEX64.match(val):
        raise ValidationError("%s.%s is not a lower-case SHA-256 hex digest" % (where, key))
    return val


def _nonempty_str(obj: dict, key: str, where: str) -> str:
    val = _need(obj, key, str, where)
    if not val.strip():
        raise ValidationError("%s.%s is empty" % (where, key))
    return val


def _check_digest(obj: dict, where: str) -> None:
    got = _hex64(obj, "digest", where)
    want = canonical_digest(obj)
    if got != want:
        raise ValidationError("%s digest mismatch: recorded %s, content hashes to %s"
                              % (where, got, want))


def _load_json(path: Path) -> dict:
    try:
        with open(path, "rb") as fh:
            value = json.loads(fh.read().decode("utf-8"))
    except OSError as exc:
        raise ValidationError("cannot read %s: %s" % (path, exc))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValidationError("%s is not valid UTF-8 JSON: %s" % (path, exc))
    if not isinstance(value, dict):
        raise ValidationError("%s does not hold a JSON object" % path)
    return value


def normalize_virtual_path(name: str) -> str:
    """Archive virtual path for lookup/hashing: lower-case, backslashes."""
    return name.replace("/", "\\").lower()


# ---------------------------------------------------------------------------
# Normalization registry
# ---------------------------------------------------------------------------
#
# Each entry validates its parameters. Applying it lives in compare.py; the
# registry is here so that load_contract can refuse unknown names before any
# comparison is attempted.

def _params_index_offset(p: dict, where: str) -> None:
    _nonempty_str(p, "output", where)
    _need(p, "offset", int, where)
    if p.get("applies_to", "expected") not in ("expected", "actual"):
        raise ValidationError("%s.applies_to must be 'expected' or 'actual'" % where)


def _params_mask_pointer(p: dict, where: str) -> None:
    outs = _need(p, "outputs", list, where)
    if not outs or not all(isinstance(o, str) and o for o in outs):
        raise ValidationError("%s.outputs must name at least one output" % where)
    _nonempty_str(p, "reason", where)


NORMALIZATIONS = {
    # The reference returns an index relative to a declared search start; add
    # ``offset`` to make it an absolute palette index. Never implicit.
    "index_offset": _params_index_offset,
    # Pointer-valued outputs are compared only for NULL vs non-NULL; their
    # addresses differ between processes. Requires a written reason.
    "mask_pointer": _params_mask_pointer,
}


def _validate_normalizations(norms, where: str) -> None:
    if not isinstance(norms, list):
        raise ValidationError("%s.normalizations must be a list" % where)
    for i, n in enumerate(norms):
        w = "%s.normalizations[%d]" % (where, i)
        name = _nonempty_str(n, "name", w)
        if name not in NORMALIZATIONS:
            raise ValidationError("%s names unknown normalization %r (known: %s)"
                                  % (w, name, ", ".join(sorted(NORMALIZATIONS))))
        NORMALIZATIONS[name](_need(n, "params", dict, w), w + ".params")


# ---------------------------------------------------------------------------
# Contract
# ---------------------------------------------------------------------------

def validate_contract(contract: dict) -> dict:
    """Validate a ``ds1edit.contract/1`` object in memory and return it."""
    w = "contract"
    if _need(contract, "schema", str, w) != CONTRACT_SCHEMA:
        raise ValidationError("contract schema is %r, expected %r"
                              % (contract.get("schema"), CONTRACT_SCHEMA))
    _nonempty_str(contract, "id", w)
    kind = _nonempty_str(contract, "kind", w)
    if not _KIND.match(kind):
        raise ValidationError("contract.kind %r is not a lower-case identifier" % kind)
    _hex64(contract, "baseline_digest", w)

    ref = _need(contract, "reference", dict, w)
    _nonempty_str(ref, "module", "contract.reference")
    rva = _nonempty_str(ref, "rva", "contract.reference")
    if not _RVA.match(rva):
        raise ValidationError("contract.reference.rva %r must be lower-case 0x hex" % rva)
    _hex64(ref, "function_bytes_sha256", "contract.reference")
    _hex64(ref, "abi_recipe_digest", "contract.reference")
    if "abi_recipe" in ref:
        recipe = _need(ref, "abi_recipe", dict, "contract.reference")
        if payload_sha256(recipe) != ref["abi_recipe_digest"]:
            raise ValidationError("contract.reference.abi_recipe does not hash to "
                                  "abi_recipe_digest")

    dom = _need(contract, "domain", dict, w)
    _nonempty_str(dom, "description", "contract.domain")
    axes = _need(dom, "axes", dict, "contract.domain")
    for name, ax in axes.items():
        aw = "contract.domain.axes.%s" % name
        cov = _nonempty_str(ax, "coverage", aw)
        if cov not in AXIS_COVERAGE:
            raise ValidationError("%s.coverage %r is not one of %s"
                                  % (aw, cov, ", ".join(AXIS_COVERAGE)))
        _need(ax, "required", bool, aw)

    outs = _need(contract, "outputs", list, w)
    if not outs:
        raise ValidationError("contract.outputs declares nothing to compare")
    seen = set()
    for i, o in enumerate(outs):
        ow = "contract.outputs[%d]" % i
        name = _nonempty_str(o, "name", ow)
        if name in seen:
            raise ValidationError("%s duplicates output %r" % (ow, name))
        seen.add(name)
        if _nonempty_str(o, "type", ow) not in OUTPUT_TYPES:
            raise ValidationError("%s.type must be one of %s" % (ow, ", ".join(OUTPUT_TYPES)))

    _validate_normalizations(_need(contract, "normalizations", list, w), w)
    for n in contract["normalizations"]:
        p = n["params"]
        named = [p["output"]] if "output" in p else list(p.get("outputs", []))
        for o in named:
            if o not in seen:
                raise ValidationError("normalization %r refers to undeclared output %r"
                                      % (n["name"], o))

    lim = _need(contract, "limitations", list, w)
    if not all(isinstance(x, str) for x in lim):
        raise ValidationError("contract.limitations must be strings")
    _need(contract, "required", bool, w)
    outcome = contract.get("success_outcome", "pass")
    if outcome not in SUCCESS_OUTCOMES:
        raise ValidationError("contract.success_outcome must be one of %s"
                              % ", ".join(SUCCESS_OUTCOMES))
    _check_digest(contract, w)
    return contract


def load_contract(path: Path) -> dict:
    return validate_contract(_load_json(Path(path)))


# ---------------------------------------------------------------------------
# Baseline
# ---------------------------------------------------------------------------

def validate_baseline(baseline: dict) -> dict:
    """Validate a ``ds1edit.baseline/1`` object in memory and return it."""
    w = "baseline"
    if _need(baseline, "schema", str, w) != BASELINE_SCHEMA:
        raise ValidationError("baseline schema is %r, expected %r"
                              % (baseline.get("schema"), BASELINE_SCHEMA))
    if _need(baseline, "build", str, w) != BASELINE_BUILD:
        raise ValidationError("baseline.build is %r; only %r is a reference"
                              % (baseline.get("build"), BASELINE_BUILD))

    bins = _need(baseline, "binaries", list, w)
    if not bins:
        raise ValidationError("baseline.binaries identifies no original binary")
    names = set()
    for i, b in enumerate(bins):
        bw = "baseline.binaries[%d]" % i
        n = _nonempty_str(b, "name", bw).lower()
        if n in names:
            raise ValidationError("%s duplicates binary %r" % (bw, n))
        names.add(n)
        _hex64(b, "sha256", bw)
        _need(b, "size", int, bw)

    arcs = _need(baseline, "archives", list, w)
    arc_names = set()
    for i, a in enumerate(arcs):
        aw = "baseline.archives[%d]" % i
        n = _nonempty_str(a, "name", aw).lower()
        if n in arc_names:
            raise ValidationError("%s duplicates archive %r" % (aw, n))
        arc_names.add(n)
        if _need(a, "priority", int, aw) != i:
            raise ValidationError("%s.priority must equal its position %d" % (aw, i))
        _hex64(a, "sha256", aw)
        _need(a, "size", int, aw)

    en = _need(baseline, "enumeration", dict, w)
    _need(en, "sources", list, "baseline.enumeration")
    _need(en, "limits", list, "baseline.enumeration")
    _need(en, "complete", bool, "baseline.enumeration")

    assets = _need(baseline, "assets", list, w)
    paths = set()
    for i, a in enumerate(assets):
        aw = "baseline.assets[%d]" % i
        p = _nonempty_str(a, "path", aw)
        if p != normalize_virtual_path(p):
            raise ValidationError("%s.path %r is not normalised (lower-case, backslashes)"
                                  % (aw, p))
        if p in paths:
            raise ValidationError("%s duplicates asset %r" % (aw, p))
        paths.add(p)
        if _nonempty_str(a, "archive", aw).lower() not in arc_names:
            raise ValidationError("%s.archive %r is not a baseline archive"
                                  % (aw, a["archive"]))
        _need(a, "size", int, aw)
        _hex64(a, "sha256", aw)

    _need(baseline, "render", dict, w)
    _check_digest(baseline, w)
    return baseline


def load_baseline(path: Path) -> dict:
    return validate_baseline(_load_json(Path(path)))


def baseline_binary(baseline: dict, name: str) -> dict | None:
    for b in baseline.get("binaries", []):
        if b.get("name", "").lower() == name.lower():
            return b
    return None


# ---------------------------------------------------------------------------
# Capture
# ---------------------------------------------------------------------------

def validate_capture_shape(capture: dict) -> dict:
    """Structural checks that need neither contract nor baseline."""
    w = "capture"
    if _need(capture, "schema", str, w) != CAPTURE_SCHEMA:
        raise ValidationError("capture schema is %r, expected %r"
                              % (capture.get("schema"), CAPTURE_SCHEMA))
    _nonempty_str(capture, "contract_id", w)
    _hex64(capture, "contract_digest", w)
    _hex64(capture, "baseline_digest", w)
    adapter = _need(capture, "adapter", dict, w)
    _nonempty_str(adapter, "name", "capture.adapter")
    _nonempty_str(adapter, "version", "capture.adapter")
    _hex64(adapter, "abi_recipe_digest", "capture.adapter")
    if _hex64(capture, "adapter_digest", w) != payload_sha256(adapter):
        raise ValidationError("capture.adapter_digest does not match the adapter record")

    orig = _need(capture, "original", dict, w)
    _nonempty_str(orig, "module", "capture.original")
    _hex64(orig, "module_sha256", "capture.original")
    _nonempty_str(orig, "rva", "capture.original")
    _hex64(orig, "function_bytes_sha256", "capture.original")

    ts = _nonempty_str(capture, "captured_at", w)
    if not _UTC.match(ts):
        raise ValidationError("capture.captured_at %r is not a UTC timestamp "
                              "(YYYY-MM-DDTHH:MM:SSZ)" % ts)
    ev = _need(capture, "evidence", list, w)
    for i, e in enumerate(ev):
        _nonempty_str(e, "kind", "capture.evidence[%d]" % i)
        _nonempty_str(e, "ref", "capture.evidence[%d]" % i)

    vecs = _need(capture, "vectors", list, w)
    if not vecs:
        raise ValidationError("capture.vectors is empty: an empty capture is not evidence")
    ids = set()
    for i, v in enumerate(vecs):
        vw = "capture.vectors[%d]" % i
        vid = _nonempty_str(v, "id", vw)
        if vid in ids:
            raise ValidationError("%s duplicates vector id %r" % (vw, vid))
        ids.add(vid)
        if "input" not in v:
            raise ValidationError("%s has no input" % vw)
        _need(v, "output", dict, vw)

    if _hex64(capture, "input_sha256", w) != payload_sha256(vector_inputs_payload(vecs)):
        raise ValidationError("capture.input_sha256 does not match its vector inputs")
    if _hex64(capture, "output_sha256", w) != payload_sha256(vector_outputs_payload(vecs)):
        raise ValidationError("capture.output_sha256 does not match its vector outputs")

    lim = _need(capture, "limitations", list, w)
    if not all(isinstance(x, str) for x in lim):
        raise ValidationError("capture.limitations must be strings")
    if capture.get("truncated", 0):
        raise ValidationError("capture.truncated is %r: a capped capture cannot be "
                              "used as complete reference evidence" % capture["truncated"])
    _check_digest(capture, w)
    return capture


def validate_capture(capture: dict, contract: dict, baseline: dict) -> None:
    """Refuse a capture that is not about this contract, baseline, and original.

    Raises :class:`ValidationError`. Contract and baseline are validated too, so
    a caller cannot pass an unchecked object through.
    """
    validate_contract(contract)
    validate_baseline(baseline)
    validate_capture_shape(capture)

    if contract["baseline_digest"] != baseline["digest"]:
        raise ValidationError("contract %s was written for baseline %s, not %s"
                              % (contract["id"], contract["baseline_digest"],
                                 baseline["digest"]))
    if capture["contract_id"] != contract["id"]:
        raise ValidationError("capture is for contract %r, not %r"
                              % (capture["contract_id"], contract["id"]))
    if capture["contract_digest"] != contract["digest"]:
        raise ValidationError("capture contract_digest %s does not match contract %s"
                              % (capture["contract_digest"], contract["digest"]))
    if capture["baseline_digest"] != baseline["digest"]:
        raise ValidationError("capture baseline_digest %s does not match baseline %s"
                              % (capture["baseline_digest"], baseline["digest"]))

    ref = contract["reference"]
    if capture["adapter"]["abi_recipe_digest"] != ref["abi_recipe_digest"]:
        raise ValidationError("capture adapter used ABI recipe %s; contract requires %s"
                              % (capture["adapter"]["abi_recipe_digest"],
                                 ref["abi_recipe_digest"]))
    accepted = contract.get("accepted_adapter_digests")
    if accepted is not None and capture["adapter_digest"] not in accepted:
        raise ValidationError("capture adapter_digest %s is not accepted by the contract"
                              % capture["adapter_digest"])

    orig = capture["original"]
    if orig["module"].lower() != ref["module"].lower():
        raise ValidationError("capture original module %r is not the contract's %r"
                              % (orig["module"], ref["module"]))
    if orig["rva"].lower() != ref["rva"]:
        raise ValidationError("capture original rva %s is not the contract's %s"
                              % (orig["rva"], ref["rva"]))
    if orig["function_bytes_sha256"] != ref["function_bytes_sha256"]:
        raise ValidationError("capture was taken against different function bytes "
                              "(%s) than the contract names (%s)"
                              % (orig["function_bytes_sha256"],
                                 ref["function_bytes_sha256"]))
    b = baseline_binary(baseline, ref["module"])
    if b is None:
        raise ValidationError("baseline does not identify module %r" % ref["module"])
    if b["sha256"] != orig["module_sha256"]:
        raise ValidationError("capture module %s sha256 %s is not the baseline's %s"
                              % (ref["module"], orig["module_sha256"], b["sha256"]))

    declared = {o["name"] for o in contract["outputs"]}
    for v in capture["vectors"]:
        out = v["output"]
        if contract.get("success_outcome", "pass") == "rejected_expected":
            if out.get("rejected") is not True:
                raise ValidationError("vector %r of an invalid-input contract does not "
                                      "record a rejection" % v["id"])
            continue
        missing = declared - set(out)
        if missing:
            raise ValidationError("capture vector %r lacks declared output(s) %s"
                                  % (v["id"], ", ".join(sorted(missing))))


def load_capture(path: Path) -> dict:
    return validate_capture_shape(_load_json(Path(path)))


def load_json_object(path: Path) -> dict:
    """Read a JSON object, raising :class:`ValidationError` on any problem."""
    return _load_json(Path(path))
