"""Portable replay of a captured reference against a candidate probe.

Uses only the captured original results, authored fixtures, and the candidate
probe executable. It never imports the MPQ reader, the fleet adapter, or
anything that needs a game installation.

    python -m scripts.equivalence.replay --contract PATH --baseline PATH \\
        --capture PATH --probe EXE --out-dir PATH

Writes ``actual.json`` and ``case.json`` into the output directory and exits
like ``compare``: 0 success, 1 mismatch, 2 invalid/unavailable/partial.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import subprocess
import sys
from pathlib import Path

if __package__ in (None, ""):  # pragma: no cover
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import compare as CMP
    from scripts.equivalence import contracts as C
else:
    from . import compare as CMP
    from . import contracts as C

REPO_ROOT = Path(__file__).resolve().parents[2]
PROBE_TIMEOUT_S = 60
INPUTS_SCHEMA = "ds1edit.palette-nearest-inputs/1"


class ReplayUnavailable(Exception):
    """The replay cannot produce a candidate result (probe/fixture problem)."""


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def platform_id() -> str:
    return "%s-%s" % (platform.system().lower() or "unknown",
                      platform.machine().lower() or "unknown")


def source_state(repo: Path = REPO_ROOT) -> dict:
    """Candidate source identity: commit plus a digest of uncommitted changes."""
    def git(*args):
        try:
            r = subprocess.run(["git", "-C", str(repo), *args], capture_output=True,
                               timeout=30)
        except (OSError, subprocess.TimeoutExpired):
            return None
        return r.stdout if r.returncode == 0 else None
    head = git("rev-parse", "HEAD")
    if head is None:
        return {"commit": None, "dirty_sha256": None, "note": "git unavailable"}
    status = git("status", "--porcelain=v1", "-uall") or b""
    diff = git("diff", "HEAD", "--binary") or b""
    return {"commit": head.decode().strip(),
            "dirty": bool(status.strip()),
            "dirty_sha256": hashlib.sha256(status + b"\0" + diff).hexdigest()
            if status.strip() else None}


def default_provenance(probe_exe: Path, build: dict | None = None) -> dict:
    return {
        "source_state": source_state(),
        "executable": {"name": Path(probe_exe).name, "sha256": _sha256_file(probe_exe)},
        "build": build or {"config": "unspecified", "compiler": "unspecified"},
        "packages": [],
    }


# ---------------------------------------------------------------------------
# palette-nearest adapter
# ---------------------------------------------------------------------------

def _load_inputs(contract: dict, inputs_root: Path) -> dict:
    ref = contract.get("inputs")
    if not isinstance(ref, dict) or "path" not in ref or "digest" not in ref:
        raise C.ValidationError("contract does not name its input fixture")
    rel = Path(ref["path"])
    if rel.is_absolute() or ".." in rel.parts:
        raise C.ValidationError("contract input path must be repository-relative")
    inp = C.load_json_object(Path(inputs_root) / rel)
    if inp.get("schema") != INPUTS_SCHEMA:
        raise C.ValidationError("input fixture has schema %r" % inp.get("schema"))
    if C.canonical_digest(inp) != inp.get("digest") or inp["digest"] != ref["digest"]:
        raise C.ValidationError("input fixture digest does not match the contract")
    return inp


def _run_palette_nearest(contract, capture, probe_cmd, out_dir: Path, inputs_root: Path):
    inputs = _load_inputs(contract, inputs_root)
    fixture = {v["id"]: v["input"] for v in inputs["vectors"]}
    cap_ids = [v["id"] for v in capture["vectors"]]
    if set(cap_ids) != set(fixture):
        raise C.ValidationError("capture covers %d of the fixture's %d vectors"
                                % (len(set(cap_ids) & set(fixture)), len(fixture)))
    pals = {}
    for k, h in inputs["palettes"].items():
        b = bytes.fromhex(h)
        if len(b) != 1024:
            raise C.ValidationError("palette %s is not 1024 bytes" % k)
        pals[k] = b

    groups = {}
    for v in capture["vectors"]:
        if v["input"] != fixture[v["id"]]:
            raise C.ValidationError("capture vector %s input differs from the fixture"
                                    % v["id"])
        inp = v["input"]
        if hashlib.sha256(pals[inp["palette"]]).hexdigest() != inp["palette_sha256"]:
            raise C.ValidationError("palette %s bytes do not match vector %s"
                                    % (inp["palette"], v["id"]))
        groups.setdefault(inp["palette"], []).append(v)

    outputs = {}
    for n, (pal_id, vecs) in enumerate(sorted(groups.items())):
        work = out_dir / ("run %02d %s" % (n, pal_id))
        work.mkdir(parents=True, exist_ok=True)
        pal_path, q_path, o_path = (work / "palette.rgba", work / "queries.rgb",
                                    work / "probe out.json")
        pal_path.write_bytes(pals[pal_id])
        q_path.write_bytes(b"".join(bytes(v["input"]["rgb"]) for v in vecs))
        if o_path.exists():
            o_path.unlink()
        cmd = [*probe_cmd, "--palette", str(pal_path), "--queries", str(q_path),
               "--out", str(o_path)]
        try:
            r = subprocess.run(cmd, capture_output=True, timeout=PROBE_TIMEOUT_S)
        except FileNotFoundError as exc:
            raise ReplayUnavailable("probe cannot be started: %s" % exc)
        except subprocess.TimeoutExpired:
            raise ReplayUnavailable("probe timed out on palette %s" % pal_id)
        if r.returncode != 0:
            raise ReplayUnavailable("probe exited %d on palette %s: %s"
                                    % (r.returncode, pal_id,
                                       r.stderr.decode("utf-8", "replace")[:200]))
        try:
            res = json.loads(o_path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            raise ReplayUnavailable("probe output unreadable: %s" % exc)
        idx = res.get("indices")
        if not isinstance(idx, list) or len(idx) != len(vecs):
            raise ReplayUnavailable("probe returned %s indices for %d queries"
                                    % (len(idx) if isinstance(idx, list) else "no",
                                       len(vecs)))
        if not isinstance(res.get("palette_unchanged"), bool):
            raise ReplayUnavailable("probe output lacks palette_unchanged")
        writes = [] if res["palette_unchanged"] else [{"offset": 0, "hex": "changed"}]
        for v, i in zip(vecs, idx):
            outputs[v["id"]] = {"index": i, "writes": writes}
    return [{"id": i, "output": outputs[i]} for i in cap_ids]


ADAPTERS = {"palette-nearest": _run_palette_nearest}


# ---------------------------------------------------------------------------

def replay(contract_path: Path, baseline_path: Path, capture_path: Path,
           probe_exe: Path, out_dir: Path, *, probe_prefix=(), build=None,
           inputs_root: Path = REPO_ROOT, lane="portable") -> dict:
    """Replay a capture against the candidate probe; write and return the case."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    cid = Path(contract_path).stem
    try:
        contract = C.load_contract(contract_path)
        cid = contract["id"]
        baseline = C.load_baseline(baseline_path)
        capture = C.load_capture(capture_path)
        C.validate_capture(capture, contract, baseline)
        adapter = ADAPTERS.get(contract["kind"])
        if adapter is None:
            raise C.ValidationError("no replay adapter for contract kind %r"
                                    % contract["kind"])
        probe_exe = Path(probe_exe)
        if not probe_exe.is_file():
            raise ReplayUnavailable("probe executable not found: %s" % probe_exe)
        vectors = adapter(contract, capture, [*probe_prefix, str(probe_exe)], out_dir,
                          Path(inputs_root))
        actual = {"lane": lane, "platform": platform_id(),
                  "provenance": default_provenance(probe_exe, build),
                  "vectors": vectors}
        with open(out_dir / "actual.json", "w", encoding="utf-8", newline="\n") as fh:
            json.dump(actual, fh, indent=1, sort_keys=True)
            fh.write("\n")
        case = CMP.compare_case(contract, capture, actual)
    except C.ValidationError as exc:
        case = CMP.unavailable_case(cid, "invalid input: %s" % exc, lane=lane,
                                    platform=platform_id())
    except ReplayUnavailable as exc:
        case = CMP.unavailable_case(cid, "candidate unavailable: %s" % exc, lane=lane,
                                    platform=platform_id())
    CMP.write_case(case, out_dir / "case.json")
    return case


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="python -m scripts.equivalence.replay")
    ap.add_argument("--contract", required=True, type=Path)
    ap.add_argument("--baseline", required=True, type=Path)
    ap.add_argument("--capture", required=True, type=Path)
    ap.add_argument("--probe", required=True, type=Path)
    ap.add_argument("--out-dir", required=True, type=Path)
    ap.add_argument("--build-config", default="unspecified")
    ap.add_argument("--compiler", default="unspecified")
    a = ap.parse_args(argv)
    case = replay(a.contract, a.baseline, a.capture, a.probe, a.out_dir,
                  build={"config": a.build_config, "compiler": a.compiler})
    cov = case["coverage"]
    print("%s: %s (%s/%s vectors, %s distinct inputs, %s failures) -> %s"
          % (case["id"], case["status"], cov.get("vectors_compared", 0),
             cov.get("vectors_expected", "?"), cov.get("distinct_inputs_compared", 0),
             cov.get("failures_total", len(case["failures"])), a.out_dir / "case.json"))
    for f in case["failures"][:5]:
        print("  %s" % json.dumps(f, sort_keys=True))
    return CMP.exit_code(case)


if __name__ == "__main__":
    sys.exit(main())
