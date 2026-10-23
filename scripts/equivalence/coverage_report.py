"""Aggregate native coverage reports into one honest line/branch summary.

Input is the machine-readable output of the instrumented lane's own compiler,
never anything derived from suite or test counts:

* ``gcov-json``   GCC ``gcov --json-format`` (one or more JSON documents per
                  file, optionally gzip-compressed ``*.gcov.json.gz``);
* ``llvm-export`` ``llvm-cov export -format=text`` (``llvm.coverage.json.export``).

The tool and format version are pinned in the output (``tool``); a format
version this module has not been written against is refused rather than guessed
at. No third-party dependency.

What the summary will and will not say:

* Totals count only *instrumented production modules*: files under the
  production globs (default ``src/**/*.c``) that appear in at least one report.
* A production module with no report entry is ``uninstrumented``. It is listed,
  contributes no covered lines, and flips the overall status to ``partial``.
  It is never counted as covered and never silently dropped.
* Files matching an exclusion (Unity, tests, stubs, generated adapters, plus any
  ``--exclude``) are listed with their reason and are not production.
* A required report that is absent makes the result ``unavailable``: no
  numbers are emitted as if it had been read.
* A file with no branch points reports ``branches.percent = null`` (not
  applicable), not 0 and not 100. A file whose branches all went unexecuted
  reports 0.0.
* The same report content supplied twice (identical SHA-256) is merged once
  and the duplicate is disclosed. Different reports for the same file (for
  example one per test executable) are merged by summing execution counts.

CLI::

    python -m scripts.equivalence.coverage_report \
        --format gcov-json --report a.json --report b.json.gz \
        --source-root . --out coverage.json [--require-report a.json]

Exit status: 0 complete, 1 partial/unavailable, 2 invalid usage or unreadable
report content.
"""

from __future__ import annotations

import argparse
import fnmatch
import gzip
import hashlib
import json
import os
import sys
from pathlib import Path, PurePosixPath
from typing import Iterable, Optional

SCHEMA = "ds1edit.coverage/1"
TOOL_VERSION = "coverage_report/1"
GCOV_FORMAT_VERSIONS = ("1", "2")
LLVM_EXPORT_MAJOR = "2"

DEFAULT_PRODUCTION_GLOBS = ("src/**/*.c",)
DEFAULT_EXCLUSIONS = (
    ("test/**", "authored tests and probes"),
    ("**/unity/**", "Unity test framework"),
    ("**/*_stubs.c", "link stubs for tests"),
    ("**/generated/**", "generated adapters"),
    ("**/*_generated.c", "generated adapters"),
)


class CoverageError(Exception):
    """A report that cannot be trusted or parsed (distinct from absence)."""


# ---------------------------------------------------------------------------
# Path handling
# ---------------------------------------------------------------------------

def normalize_path(raw: str, source_root: Path) -> str:
    """Posix path relative to ``source_root`` when inside it, else absolute posix.

    Purely lexical: reports come from another host (a Linux runner's
    ``/home/runner/...`` is normalised against whatever ``source_root`` the
    caller names), so nothing here touches the filesystem except to resolve the
    root's own spelling.
    """
    text = raw.replace("\\", "/")
    parts: list[str] = []
    for part in text.split("/"):
        if part in ("", "."):
            continue
        if part == ".." and parts and parts[-1] != "..":
            parts.pop()
            continue
        parts.append(part)
    is_abs = text.startswith("/") or (len(text) > 1 and text[1] == ":")
    norm = ("/" if text.startswith("/") else "") + "/".join(parts)
    if not is_abs:
        return norm
    roots = {str(source_root).replace("\\", "/").rstrip("/")}
    try:
        roots.add(str(Path(source_root).resolve()).replace("\\", "/").rstrip("/"))
    except OSError:
        pass
    low = norm.lower()
    for root in roots:
        r = root.lower()
        if low.startswith(r + "/"):
            return norm[len(root) + 1:]
    return norm


def _glob_match(path: str, pattern: str) -> bool:
    """fnmatch with ``**`` crossing directories and ``*`` not crossing them."""
    parts = pattern.split("/")
    segs = path.split("/")

    def rec(i: int, j: int) -> bool:
        if i == len(parts):
            return j == len(segs)
        if parts[i] == "**":
            if i == len(parts) - 1:
                return True
            return any(rec(i + 1, k) for k in range(j, len(segs) + 1))
        if j == len(segs):
            return False
        return fnmatch.fnmatchcase(segs[j], parts[i]) and rec(i + 1, j + 1)

    return rec(0, 0)


# ---------------------------------------------------------------------------
# Parsers: each returns (producer_info, {file: FileCov})
# FileCov = {"lines": {lineno: count}, "branches": {(lineno, idx): count}}
# ---------------------------------------------------------------------------

def _empty() -> dict:
    return {"lines": {}, "branches": {}}


def _add(dst: dict, src: dict) -> None:
    for k, v in src["lines"].items():
        dst["lines"][k] = dst["lines"].get(k, 0) + v
    for k, v in src["branches"].items():
        dst["branches"][k] = dst["branches"].get(k, 0) + v


def _json_documents(data: bytes) -> list:
    """One or more concatenated JSON documents (gcov --stdout emits several)."""
    text = data.decode("utf-8")
    dec = json.JSONDecoder()
    docs, i, n = [], 0, len(text)
    while True:
        while i < n and text[i].isspace():
            i += 1
        if i >= n:
            break
        try:
            doc, i = dec.raw_decode(text, i)
        except json.JSONDecodeError as exc:
            raise CoverageError(f"not valid JSON: {exc}") from exc
        docs.append(doc)
    if not docs:
        raise CoverageError("empty report")
    return docs


def parse_gcov_json(docs: list, source_root: Path) -> tuple[dict, dict]:
    files: dict[str, dict] = {}
    versions, gcc = set(), set()
    for doc in docs:
        if not isinstance(doc, dict) or "files" not in doc:
            raise CoverageError("gcov JSON document has no 'files' member")
        fv = str(doc.get("format_version", ""))
        if fv not in GCOV_FORMAT_VERSIONS:
            raise CoverageError(
                f"unsupported gcov JSON format_version {fv!r}; this tool was "
                f"written for {list(GCOV_FORMAT_VERSIONS)}")
        versions.add(fv)
        if doc.get("gcc_version"):
            gcc.add(str(doc["gcc_version"]))
        cwd = doc.get("current_working_directory")
        for f in doc["files"]:
            name = f.get("file")
            if not isinstance(name, str):
                raise CoverageError("gcov file entry without a name")
            if cwd and not os.path.isabs(name) and not (len(name) > 1 and name[1] == ":"):
                name = str(PurePosixPath(str(cwd).replace("\\", "/")) / name)
            fc = files.setdefault(normalize_path(name, source_root), _empty())
            for ln in f.get("lines", []):
                num = int(ln["line_number"])
                fc["lines"][num] = fc["lines"].get(num, 0) + int(ln.get("count", 0))
                for idx, br in enumerate(ln.get("branches", [])):
                    key = (num, idx)
                    fc["branches"][key] = fc["branches"].get(key, 0) + int(br.get("count", 0))
    return {"format": "gcov-json", "format_versions": sorted(versions),
            "producer": "gcc " + ",".join(sorted(gcc)) if gcc else "gcc (version not reported)"}, files


def _llvm_lines(segments: list) -> dict:
    """Per-line execution count from llvm-cov segments.

    A segment is [line, col, count, hasCount, isRegionEntry, isGapRegion]. A
    line is executable when a counted, non-gap region covers any of it; its
    count is the maximum of the regions touching it (the same rule llvm-cov's
    own line view uses for the headline line figure).
    """
    lines: dict[int, int] = {}
    prev = None  # the active segment entering the current line
    i, n = 0, len(segments)
    while i < n:
        line = segments[i][0]
        j = i
        while j < n and segments[j][0] == line:
            j += 1
        here = segments[i:j]
        counts = []
        if (prev is not None and prev[3] and not prev[5]
                and here[0][1] > 1):  # a region ending at column 1 leaves the line out
            counts.append(prev[2])
        for seg in here:
            if seg[3] and not seg[5]:
                counts.append(seg[2])
        if counts:
            lines[line] = max(counts)
        # Fill lines spanned between this line and the next segment's line.
        last = here[-1]
        nxt = segments[j][0] if j < n else line
        if last[3] and not last[5]:
            for mid in range(line + 1, nxt):
                lines[mid] = max(lines.get(mid, 0), last[2])
        prev = last
        i = j
    return lines


def parse_llvm_export(docs: list, source_root: Path) -> tuple[dict, dict]:
    files: dict[str, dict] = {}
    versions, producers = set(), set()
    for doc in docs:
        if not isinstance(doc, dict) or doc.get("type") != "llvm.coverage.json.export":
            raise CoverageError("not an llvm-cov export document "
                                "(type != llvm.coverage.json.export)")
        ver = str(doc.get("version", ""))
        if ver.split(".")[0] != LLVM_EXPORT_MAJOR:
            raise CoverageError(
                f"unsupported llvm-cov export version {ver!r}; this tool was "
                f"written for major {LLVM_EXPORT_MAJOR}")
        versions.add(ver)
        if doc.get("tool"):
            producers.add(str(doc["tool"]))
        for item in doc.get("data", []):
            for f in item.get("files", []):
                name = f.get("filename")
                if not isinstance(name, str):
                    raise CoverageError("llvm export file entry without a name")
                fc = files.setdefault(normalize_path(name, source_root), _empty())
                for ln, cnt in _llvm_lines(f.get("segments", [])).items():
                    fc["lines"][ln] = fc["lines"].get(ln, 0) + cnt
                # branch rows: [l1,c1,l2,c2,execCount,falseExecCount,fileId,expFileId,kind]
                for br in f.get("branches", []):
                    base = (int(br[0]), int(br[1]))
                    for k, cnt in ((0, br[4]), (1, br[5])):
                        key = (base[0] * 10000 + base[1], k)
                        fc["branches"][key] = fc["branches"].get(key, 0) + int(cnt)
    return {"format": "llvm-export", "format_versions": sorted(versions),
            "producer": "llvm-cov " + ",".join(sorted(producers)) if producers
            else "llvm-cov export"}, files


PARSERS = {"gcov-json": parse_gcov_json, "llvm-export": parse_llvm_export}


# ---------------------------------------------------------------------------
# Reading report files
# ---------------------------------------------------------------------------

def read_report_file(path: Path) -> bytes:
    data = path.read_bytes()
    if path.suffix == ".gz" or data[:2] == b"\x1f\x8b":
        try:
            data = gzip.decompress(data)
        except OSError as exc:
            raise CoverageError(f"{path}: bad gzip: {exc}") from exc
    return data


# ---------------------------------------------------------------------------
# Aggregation
# ---------------------------------------------------------------------------

def _ratio(total: int, covered: int) -> dict:
    return {"total": total, "covered": covered,
            "percent": None if total == 0 else round(100.0 * covered / total, 2)}


def _file_summary(fc: dict) -> dict:
    lines_total = len(fc["lines"])
    lines_cov = sum(1 for c in fc["lines"].values() if c > 0)
    br_total = len(fc["branches"])
    br_cov = sum(1 for c in fc["branches"].values() if c > 0)
    return {"lines": _ratio(lines_total, lines_cov),
            "branches": _ratio(br_total, br_cov)}


def list_production_files(source_root: Path, globs: Iterable[str]) -> list[str]:
    found: set[str] = set()
    root = source_root.resolve()
    for g in globs:
        for p in root.glob(g):
            if p.is_file():
                found.add(p.relative_to(root).as_posix())
    return sorted(found)


def aggregate(reports: list, source_root: Path, *,
              fmt: str,
              production_files: Optional[list] = None,
              production_globs: Iterable[str] = DEFAULT_PRODUCTION_GLOBS,
              exclusions: Iterable = DEFAULT_EXCLUSIONS,
              required: Iterable = ()) -> dict:
    """Merge reports and classify every file. See the module docstring.

    ``reports`` items are ``Path`` objects or ``(name, bytes)`` pairs.
    ``required`` names reports that must exist; absent ones are recorded and
    the result is ``unavailable``. ``production_files`` overrides the glob
    walk (used by tests and by callers that already know the module list).
    """
    if fmt not in PARSERS:
        raise CoverageError(f"unknown format {fmt!r}; expected one of {sorted(PARSERS)}")
    source_root = Path(source_root)
    exclusions = list(exclusions)

    missing: list[dict] = []
    inputs: list[tuple[str, bytes]] = []
    for item in reports:
        if isinstance(item, tuple):
            inputs.append((str(item[0]), item[1]))
            continue
        p = Path(item)
        if not p.is_file():
            missing.append({"path": str(p), "required": False})
            continue
        inputs.append((str(p), read_report_file(p)))
    for req in required:
        rp = Path(req)
        if not rp.is_file() and not any(n == str(rp) for n, _ in inputs):
            if not any(m["path"] == str(rp) for m in missing):
                missing.append({"path": str(rp), "required": True})
            else:
                for m in missing:
                    if m["path"] == str(rp):
                        m["required"] = True

    required_missing = [m for m in missing if m["required"]]
    producer: dict = {"format": fmt, "format_versions": [], "producer": "unknown"}
    merged: dict[str, dict] = {}
    seen_hash: dict[str, str] = {}
    duplicates: list[dict] = []
    report_records: list[dict] = []

    if not required_missing:
        for name, data in inputs:
            digest = hashlib.sha256(data).hexdigest()
            if digest in seen_hash:
                duplicates.append({"path": name, "duplicate_of": seen_hash[digest],
                                   "sha256": digest})
                continue
            seen_hash[digest] = name
            try:
                info, files = PARSERS[fmt](_json_documents(
                    gzip.decompress(data) if data[:2] == b"\x1f\x8b" else data), source_root)
            except CoverageError as exc:
                raise CoverageError(f"{name}: {exc}") from exc
            producer.update({k: v for k, v in info.items() if k != "format_versions"})
            producer["format_versions"] = sorted(set(producer["format_versions"])
                                                 | set(info["format_versions"]))
            report_records.append({"path": name, "sha256": digest, "files": len(files)})
            for fname, fc in files.items():
                _add(merged.setdefault(fname, _empty()), fc)

    if production_files is None:
        production_files = list_production_files(source_root, production_globs)
    production_set = set(production_files)

    def excluded_by(path: str) -> Optional[tuple]:
        for pat, why in exclusions:
            if _glob_match(path, pat):
                return pat, why
        return None

    excl_hits: dict[tuple, list] = {}
    modules: list[dict] = []
    outside: list[str] = []
    uninstrumented: list[str] = []

    # Reported files: production, excluded, or outside the tree.
    for fname in sorted(merged):
        hit = excluded_by(fname)
        if hit:
            excl_hits.setdefault(hit, []).append(fname)
        elif fname in production_set:
            modules.append({"file": fname, "status": "instrumented",
                            **_file_summary(merged[fname])})
        else:
            outside.append(fname)

    # Declared production modules with no report entry.
    for fname in production_files:
        if fname in merged:
            continue
        hit = excluded_by(fname)
        if hit:
            excl_hits.setdefault(hit, []).append(fname)
            continue
        uninstrumented.append(fname)
        modules.append({"file": fname, "status": "uninstrumented",
                        "lines": None, "branches": None})
    modules.sort(key=lambda m: m["file"])

    lt = sum(m["lines"]["total"] for m in modules if m["status"] == "instrumented")
    lc = sum(m["lines"]["covered"] for m in modules if m["status"] == "instrumented")
    bt = sum(m["branches"]["total"] for m in modules if m["status"] == "instrumented")
    bc = sum(m["branches"]["covered"] for m in modules if m["status"] == "instrumented")

    if required_missing:
        status, reason = "unavailable", "required report(s) absent: " + ", ".join(
            m["path"] for m in required_missing)
    elif not report_records:
        status, reason = "unavailable", "no coverage report was read"
    elif uninstrumented:
        status, reason = "partial", (f"{len(uninstrumented)} production module(s) "
                                     "are uninstrumented and not counted as covered")
    else:
        status, reason = "complete", "every production module has a report entry"

    totals = None
    if status != "unavailable":
        totals = {"lines": _ratio(lt, lc), "branches": _ratio(bt, bc),
                  "scope": "instrumented production modules only; "
                           "uninstrumented modules are listed in 'modules' and "
                           "contribute no covered lines"}
    return {
        "schema": SCHEMA,
        "tool": {"name": TOOL_VERSION, **producer},
        "status": status,
        "status_reason": reason,
        "totals": totals,
        "production_modules": len(production_files) - sum(
            1 for f in production_files if excluded_by(f)),
        "instrumented_modules": sum(1 for m in modules if m["status"] == "instrumented"),
        "uninstrumented_modules": uninstrumented,
        "modules": modules,
        "exclusions": [{"pattern": p, "reason": w, "files": sorted(set(fs))}
                       for (p, w), fs in sorted(excl_hits.items())],
        "outside_production": sorted(outside),
        "reports": report_records,
        "duplicate_reports": duplicates,
        "missing_reports": missing,
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv: Optional[list] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--format", required=True, choices=sorted(PARSERS))
    ap.add_argument("--report", action="append", default=[], type=Path)
    ap.add_argument("--require-report", action="append", default=[], type=Path)
    ap.add_argument("--source-root", type=Path, default=Path("."))
    ap.add_argument("--production-glob", action="append", default=None)
    ap.add_argument("--exclude", action="append", default=[],
                    metavar="GLOB=REASON")
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args(argv)

    excl = list(DEFAULT_EXCLUSIONS)
    for e in args.exclude:
        pat, _, why = e.partition("=")
        if not pat or not why:
            print("--exclude needs GLOB=REASON", file=sys.stderr)
            return 2
        excl.append((pat, why))
    try:
        result = aggregate(args.report, args.source_root, fmt=args.format,
                           production_globs=args.production_glob or DEFAULT_PRODUCTION_GLOBS,
                           exclusions=excl, required=args.require_report)
    except CoverageError as exc:
        print(f"coverage_report: {exc}", file=sys.stderr)
        return 2
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    t = result["totals"]
    if t:
        print(f"{result['status']}: lines {t['lines']['covered']}/{t['lines']['total']}, "
              f"branches {t['branches']['covered']}/{t['branches']['total']}; "
              f"{len(result['uninstrumented_modules'])} uninstrumented production module(s)")
    else:
        print(f"{result['status']}: {result['status_reason']}")
    return 0 if result["status"] == "complete" else 1


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
