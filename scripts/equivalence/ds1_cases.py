"""
DS1 map snapshots (schema ``ds1edit.map/1``) and their canonical comparison.

A snapshot is the JSON written by ``test_ds1_reference_probe dump`` -- what the
editor's real reader made of a ``.ds1`` -- or an expectation in the same
shape (test/fixtures/make_ds1_fixtures.py). Comparisons go through
``canonical_map(snapshot, policy)``, which applies only the normalizations a
policy names explicitly. Nothing is ignored by default: a field that differs
is a difference unless a named, documented rule says why it may.

Named normalizations (the editor's declared writer transformations):

``writer_v18_upgrade``
    The writer always emits version 18. Compare a re-saved map with its
    source without the version field.
``legacy_tag_layer_dropped``
    Below version 4 a tag layer is read although the file has no tagType;
    the writer only emits a tag layer for tagType 1/2, so a re-saved
    pre-v4 map has none.
``workspace_files_ignored``
    With workspace data enabled the writer appends ``DS1EDIT_WRKSPC_*``
    entries to the tileset name list; drop them.
``source_ignored``
    Drop the ``source`` path the probe records.

This module deliberately works on plain dicts and does not depend on
package B's result contracts (scripts/equivalence/contracts.py); B's
``ds1edit.case/1`` results can wrap the outcome of ``compare_maps``.
"""

from __future__ import annotations

import copy
import json
import os
import subprocess
from pathlib import Path

SCHEMA = "ds1edit.map/1"


def _writer_v18_upgrade(snap: dict) -> None:
    snap.pop("version", None)


def _legacy_tag_layer_dropped(snap: dict) -> None:
    for cell in snap.get("cells", []):
        cell["tags"] = []
    if "layers" in snap:
        snap["layers"]["tags"] = 0


def _workspace_files_ignored(snap: dict) -> None:
    snap["files"] = [f for f in snap.get("files", [])
                     if not f.startswith("DS1EDIT_WRKSPC_")]


def _source_ignored(snap: dict) -> None:
    snap.pop("source", None)


NORMALIZATIONS = {
    "writer_v18_upgrade": _writer_v18_upgrade,
    "legacy_tag_layer_dropped": _legacy_tag_layer_dropped,
    "workspace_files_ignored": _workspace_files_ignored,
    "source_ignored": _source_ignored,
}


def canonical_map(snapshot: dict, policy: dict | None = None) -> dict:
    """Return a normalized deep copy of ``snapshot``.

    ``policy`` is ``{"normalizations": [names...]}``; unknown names raise
    ``ValueError`` so a typo cannot silently widen a comparison. A snapshot
    with another schema raises ``ValueError``.
    """
    if not isinstance(snapshot, dict) or snapshot.get("schema") != SCHEMA:
        raise ValueError(f"not a {SCHEMA} snapshot: {snapshot.get('schema') if isinstance(snapshot, dict) else type(snapshot)}")
    names = list((policy or {}).get("normalizations", []))
    unknown = [n for n in names if n not in NORMALIZATIONS]
    if unknown:
        raise ValueError(f"unknown normalization(s): {', '.join(unknown)}")
    snap = copy.deepcopy(snapshot)
    for name in names:
        NORMALIZATIONS[name](snap)
    return snap


def diff_maps(expected, actual, path: str = "") -> list[str]:
    """Human-readable differences between two canonical snapshots."""
    out: list[str] = []
    if isinstance(expected, dict) and isinstance(actual, dict):
        for key in sorted(set(expected) | set(actual)):
            sub = f"{path}.{key}" if path else key
            if key not in actual:
                out.append(f"{sub}: missing")
            elif key not in expected:
                out.append(f"{sub}: unexpected")
            else:
                out.extend(diff_maps(expected[key], actual[key], sub))
    elif isinstance(expected, list) and isinstance(actual, list):
        if len(expected) != len(actual):
            out.append(f"{path}: length {len(expected)} != {len(actual)}")
        for i, (a, b) in enumerate(zip(expected, actual)):
            out.extend(diff_maps(a, b, f"{path}[{i}]"))
    elif expected != actual:
        out.append(f"{path}: {expected!r} != {actual!r}")
    return out


def compare_maps(expected: dict, actual: dict, policy: dict | None = None) -> dict:
    """Compare two snapshots under ``policy``.

    Returns ``{"status": "pass"|"fail", "differences": [...]}``. A rejected
    file (``status: error``) only matches an expectation of rejection.
    """
    if expected.get("status") == "error" or actual.get("status") == "error":
        same = expected.get("status") == actual.get("status")
        return {"status": "pass" if same else "fail",
                "differences": [] if same else
                [f"status: {expected.get('status')!r} != {actual.get('status')!r}"]}
    diffs = diff_maps(canonical_map(expected, policy), canonical_map(actual, policy))
    return {"status": "pass" if not diffs else "fail", "differences": diffs}


def load_snapshot(path: Path) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _probe_env(probe_exe: Path) -> dict:
    """Environment for the probe: on Windows its Allegro DLLs are copied next
    to the test binaries (the exe's folder or its parent, depending on the
    generator), so put both on PATH; elsewhere the loader finds them itself."""
    env = dict(os.environ)
    if os.name == "nt":
        exe_dir = Path(probe_exe).resolve().parent
        env["PATH"] = os.pathsep.join([str(exe_dir), str(exe_dir.parent), env.get("PATH", "")])
    return env


def run_probe(probe_exe: Path, ds1_path: Path, out_json: Path,
              timeout: float = 60.0) -> tuple[int, dict | None]:
    """Run ``probe dump`` on one file. Returns (exit code, snapshot or None)."""
    proc = subprocess.run([str(probe_exe), "dump", str(ds1_path), "--out", str(out_json)],
                          capture_output=True, timeout=timeout,
                          cwd=str(Path(out_json).parent), env=_probe_env(probe_exe))
    snap = load_snapshot(out_json) if Path(out_json).exists() else None
    return proc.returncode, snap


def run_resave(probe_exe: Path, ds1_path: Path, out_ds1: Path,
               timeout: float = 60.0) -> int:
    """Read ``ds1_path`` with the editor and save it with the checked writer."""
    proc = subprocess.run([str(probe_exe), "resave", str(ds1_path), "--out", str(out_ds1)],
                          capture_output=True, timeout=timeout,
                          cwd=str(Path(out_ds1).parent), env=_probe_env(probe_exe))
    return proc.returncode
