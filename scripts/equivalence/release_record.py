"""Cut a release record from a CI run, and run release.yml's checks locally.

Record mode (the maintainer's one command)::

    python -m scripts.equivalence.release_record --run-id 123456 --version 2.2.1 \\
        [--waivers waivers.json] [--evidence-dir LOCAL_LANES] --out release/qualification/

1. checks that the run is a successful ``build.yml`` run of this repository;
2. downloads its ``candidate-<platform>`` artifacts (packages, per-platform
   manifests, ``package-inspection.json``, ``evidence/*.json``) and the
   ``evidence-*`` artifacts of the instrumented Linux jobs;
3. combines the three manifests into one ``ds1edit.candidate/1`` and refuses a
   checkout that is not that candidate's source state;
4. qualifies the candidate against ``release-core.json`` with the CI evidence,
   any local-lane evidence (``--evidence-dir``: corpus, live, native,
   performance ``ds1edit.case/1`` files) and the waivers;
5. when ready, writes ``release/qualification/<source_state_digest>.json``
   (otherwise nothing is written and the blocking entries are listed).

``--dry-run-release`` instead runs what ``release.yml`` will run, against the
artifacts of the same run and the tag (``v<version>``): it finds the committed
record, checks the run, verifies manifests, packages, evidence files and every
required result, and writes the draft release notes. Nothing is published.

The same functions back release.yml (``find-record``, ``check-run``,
``verify-release`` subcommands), so a dry run and the real run cannot drift.

Exit status: 0 ok, 1 not ready / verification problems, 2 invalid input,
failed download or unusable evidence. Standard library only (plus ``gh``).
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

if __package__ in (None, ""):  # pragma: no cover - direct script execution
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
    from scripts.equivalence import qualify as Q
else:
    from . import contracts as C
    from . import qualify as Q

PLATFORMS = ("windows-x86", "linux-x86_64", "macos-arm64")
#: Artifacts of the instrumented Linux jobs that carry ds1edit.case-set/1 files.
EVIDENCE_ARTIFACTS = ("evidence-linux-sanitizers", "evidence-linux-coverage")
BUILD_WORKFLOW = ".github/workflows/build.yml"
PROFILE_PATH = "test/equivalence/profiles/release-core.json"
QUALIFICATION_DIR = "release/qualification"
_VERSION = re.compile(r"^\d+\.\d+\.\d+$")
_TAG = re.compile(r"^v\d+\.\d+\.\d+$")


# ---------------------------------------------------------------------------
# gh
# ---------------------------------------------------------------------------

class GhClient:
    """The few GitHub CLI operations the helper needs (fakeable in tests)."""

    def __init__(self, gh: str = "gh"):
        self.gh = gh

    def _run(self, *args):
        try:
            r = subprocess.run([self.gh, *args], capture_output=True, text=True,
                               timeout=900, **Q._no_window())
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise C.ValidationError("cannot run %s: %s" % (self.gh, exc))
        return r

    def repository(self) -> str:
        r = self._run("repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner")
        name = r.stdout.strip()
        if r.returncode or "/" not in name:
            raise C.ValidationError("cannot tell the repository (gh repo view failed); "
                                    "pass --repository owner/name: %s" % r.stderr.strip())
        return name

    def run_json(self, run_id, repo: str) -> dict:
        r = self._run("api", "repos/%s/actions/runs/%s" % (repo, run_id))
        if r.returncode:
            raise C.ValidationError("cannot read run %s of %s: %s"
                                    % (run_id, repo, r.stderr.strip()))
        try:
            return json.loads(r.stdout)
        except ValueError as exc:
            raise C.ValidationError("run %s: unreadable gh output: %s" % (run_id, exc))

    def download(self, run_id, name: str, dest, repo: str) -> bool:
        Path(dest).mkdir(parents=True, exist_ok=True)
        r = self._run("run", "download", str(run_id), "-R", repo, "-n", name, "-D", str(dest))
        return r.returncode == 0


# ---------------------------------------------------------------------------
# Reference identities
# ---------------------------------------------------------------------------

def build_reference(root) -> dict:
    """``candidate.reference`` from the committed contracts and captures.

    Contract digests come from ``ds1edit.contract/1`` files, adapter digests
    from ``ds1edit.capture/1`` files, and the baseline digest from the
    contracts (which must agree). Whatever the repository does not carry is
    left out, never invented; supply ``--reference`` to add local-lane ones.
    """
    root = Path(root)
    contracts, adapters, baselines = {}, {}, set()
    for f in sorted((root / "test" / "equivalence" / "contracts").glob("*.json")):
        obj = _try_json(f)
        if isinstance(obj, dict) and obj.get("schema") == "ds1edit.contract/1" \
                and isinstance(obj.get("digest"), str):
            contracts[obj["id"]] = obj["digest"]
            if isinstance(obj.get("baseline_digest"), str):
                baselines.add(obj["baseline_digest"])
    for f in sorted((root / "test" / "equivalence" / "fixtures").glob("*capture*.json")):
        obj = _try_json(f)
        if isinstance(obj, dict) and obj.get("schema") == "ds1edit.capture/1" \
                and isinstance(obj.get("adapter_digest"), str):
            adapters[obj["contract_id"]] = obj["adapter_digest"]
            if isinstance(obj.get("baseline_digest"), str):
                baselines.add(obj["baseline_digest"])
    if len(baselines) > 1:
        raise C.ValidationError("committed contracts name different baselines: %s; pass "
                                "--reference to choose" % ", ".join(sorted(baselines)))
    return {"baseline_digest": next(iter(baselines), None), "contracts": contracts,
            "adapters": adapters}


def _try_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


# ---------------------------------------------------------------------------
# Evidence collection
# ---------------------------------------------------------------------------

def _wrap_waivers(obj, where: str) -> dict:
    if isinstance(obj, list):
        return {"schema": Q.REVIEW_SCHEMA, "dispositions": obj}
    if isinstance(obj, dict) and obj.get("schema") == Q.REVIEW_SCHEMA:
        return obj
    raise C.ValidationError("%s must be a %s object or a list of dispositions"
                            % (where, Q.REVIEW_SCHEMA))


def collect_results(work: Path, evidence_dirs=(), waiver_files=()):
    """``(results, files)``: every evidence object and where it came from."""
    work = Path(work)
    paths = []
    for p in PLATFORMS:
        d = work / "candidate" / p
        if (d / "package-inspection.json").is_file():
            paths.append(d / "package-inspection.json")
        paths += sorted((d / "evidence").glob("*.json"))
    for name in EVIDENCE_ARTIFACTS:
        paths += sorted((work / "evidence" / name).rglob("*.json"))
    for d in evidence_dirs:
        d = Path(d)
        if not d.is_dir():
            raise C.ValidationError("evidence directory %s does not exist" % d)
        paths += sorted(d.glob("*.json"))
    results, files = [], []
    for f in paths:
        obj = C.load_json_object(f)
        results.append(obj)
        files.append(_file_entry(f, obj, work))
    for f in waiver_files:
        f = Path(f)
        try:
            raw = json.loads(f.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            raise C.ValidationError("cannot read waivers %s: %s" % (f, exc))
        obj = _wrap_waivers(raw, str(f))
        results.append(obj)
        files.append(_file_entry(f, obj, work))
    return results, files


def _file_entry(path: Path, obj: dict, base: Path) -> dict:
    try:
        rel = path.resolve().relative_to(base.resolve()).as_posix()
    except ValueError:
        rel = path.name
    return {"path": rel, "schema": obj.get("schema"), "sha256": Q.sha256_file(path),
            "payload_sha256": C.payload_sha256(obj)}


# ---------------------------------------------------------------------------
# Checks shared with release.yml
# ---------------------------------------------------------------------------

def find_record(root, qualification_dir):
    """``(source state digest, record path or None)`` for the checkout at *root*."""
    digest = Q.source_state_digest(Path(root))
    path = Path(qualification_dir) / ("%s.json" % digest)
    return digest, (path if path.is_file() else None)


def check_run(record: dict, run: dict, repository: str) -> list:
    """Problems with the candidate run a record names (empty when it is right)."""
    cand = record["candidate"]
    rid = str(cand.get("candidate_run_id"))
    problems = []
    if not rid.isdigit():
        problems.append("candidate_run_id %r is not a GitHub run ID" % rid)
    if cand.get("repository") != repository:
        problems.append("record names repository %s, not %s" % (cand.get("repository"),
                                                                 repository))
    want = {"path": BUILD_WORKFLOW, "conclusion": "success",
            "head_sha": cand["source"]["commit"]}
    for k, v in want.items():
        if run.get(k) != v:
            problems.append("candidate run %s: %s is %r, expected %r" % (rid, k, run.get(k), v))
    if (run.get("repository") or {}).get("full_name") != repository:
        problems.append("candidate run %s belongs to another repository" % rid)
    return problems


def verify_candidate_dir(record: dict, profile: dict, root, candidate_dir, tag: str,
                         platforms=PLATFORMS) -> list:
    """release.yml's verification of the downloaded candidate artifacts."""
    problems = []
    if not _TAG.match(tag or ""):
        return ["tag %r is not of the form vX.Y.Z" % tag]
    version = tag[1:]
    cand = record["candidate"]
    candidate_dir = Path(candidate_dir)
    if sorted(cand["platforms"]) != sorted(platforms):
        problems.append("record platforms %s are not %s" % (sorted(cand["platforms"]),
                                                           sorted(platforms)))
    reviewed = {a.get("payload_sha256") for a in record.get("input_artifacts", [])
                if isinstance(a, dict)}
    artifacts = {}
    for p, v in sorted(cand["platforms"].items()):
        d = candidate_dir / p
        pkg = v["package"]["name"]
        artifacts[p] = d / pkg
        names = sorted(f.name for f in d.iterdir()) if d.is_dir() else []
        allowed = {pkg, "candidate-%s.json" % p, "package-inspection.json", "evidence"}
        if set(names) - allowed:
            problems.append("%s: unexpected files in candidate artifact: %s"
                            % (p, sorted(set(names) - allowed)))
        if "-%s-" % version not in pkg:
            problems.append("%s: package %s is not version %s" % (p, pkg, tag))
        evidence = []
        if (d / "package-inspection.json").is_file():
            evidence.append(d / "package-inspection.json")
        ed = d / "evidence"
        if ed.exists():
            if not ed.is_dir():
                problems.append("%s: evidence is not a directory" % p)
            else:
                stray = [f.name for f in ed.iterdir() if not (f.is_file()
                                                              and f.suffix == ".json")]
                if stray:
                    problems.append("%s: unexpected files in evidence/: %s"
                                    % (p, sorted(stray)))
                evidence += sorted(f for f in ed.glob("*.json") if f.is_file())
        for f in evidence:
            try:
                payload = C.payload_sha256(C.load_json_object(f))
            except (OSError, ValueError, C.ValidationError) as exc:
                problems.append("%s: evidence file %s unusable: %s" % (p, f.name, exc))
                continue
            if payload not in reviewed:
                problems.append("%s: %s is not part of the reviewed record" % (p, f.name))
        mf = d / ("candidate-%s.json" % p)
        try:
            m = json.loads(mf.read_text(encoding="utf-8"))
            C._check_digest(m, mf.name)
        except (OSError, ValueError, C.ValidationError) as exc:
            problems.append("%s: manifest unusable: %s" % (p, exc))
            continue
        checks = {
            "schema": (m.get("schema"), Q.PLATFORM_MANIFEST_SCHEMA),
            "platform": (m.get("platform"), p),
            "candidate_run_id": (m.get("candidate_run_id"), cand["candidate_run_id"]),
            "commit": ((m.get("source") or {}).get("commit"), cand["source"]["commit"]),
            "source_state_digest": ((m.get("source") or {}).get("source_state_digest"),
                                    cand["source"]["source_state_digest"]),
            "package": (m.get("package"), v["package"]),
            "executables": (m.get("executables"), v["executables"]),
        }
        for k, (got, want) in checks.items():
            if got != want:
                problems.append("%s: manifest %s %r != record %r" % (p, k, got, want))
    problems += Q.verify_release_record(record, profile, Path(root), artifacts,
                                        version=version)
    return problems


def release_notes(record: dict, tag: str) -> str:
    """Draft release notes: the qualification identity and the Waived checks."""
    cand = record["candidate"]
    lines = ["# DS1Edit %s" % tag.lstrip("v"), "",
             "Qualified candidate: run %s, commit `%s`." % (cand["candidate_run_id"],
                                                            cand["source"]["commit"]),
             "Qualification record: `release/qualification/%s.json`."
             % record["source_state_digest"], "",
             "## Package SHA-256", ""]
    for p, sha in sorted(record["package_hashes"].items()):
        lines.append("- `%s` %s" % (p, sha))
    lines += ["", "## Waived checks", ""]
    waived = record.get("waived") or []
    if not waived:
        lines.append("None: every required check qualified.")
    for w in waived:
        wv = w["waiver"]
        lines.append("- `%s` (%s, %s): %s (reviewer %s, expires with %s)"
                     % (w["id"], w["platform"], w["status"], wv["reason"], wv["reviewer"],
                        wv["expires_version"]))
    return "\n".join(lines) + "\n"


def _load_record(path) -> dict:
    return C.load_json_object(Path(path))


def _emit(problems, stream=None):
    stream = stream or sys.stderr
    for p in problems:
        print("::error::%s" % p, file=stream)


def _verified_line(record: dict) -> str:
    return "verified candidate run %s, source state %s, %d required results" % (
        record["candidate"]["candidate_run_id"], record["source_state_digest"],
        len(record["required"]))


# ---------------------------------------------------------------------------
# Subcommands used by release.yml
# ---------------------------------------------------------------------------

def _cmd_find_record(a) -> int:
    root = Path(a.root)
    qdir = Path(a.qualification_dir) if a.qualification_dir else root / QUALIFICATION_DIR
    digest, path = find_record(root, qdir)
    print("source state: %s" % digest, file=sys.stderr)
    if path is None:
        print("::error::No reviewed qualification record %s for the tagged source state. "
              "Qualify a candidate built from this source and commit its record."
              % (qdir / ("%s.json" % digest)), file=sys.stderr)
        return 1
    rec = _load_record(path)
    c = rec["candidate"]
    print("digest=%s" % digest)
    print("record=%s" % path)
    print("run_id=%s" % c["candidate_run_id"])
    print("commit=%s" % c["source"]["commit"])
    print("repository=%s" % c["repository"])
    return 0


def _cmd_check_run(a) -> int:
    problems = check_run(_load_record(a.record), C.load_json_object(Path(a.run_json)),
                         a.repository)
    _emit(problems)
    return 1 if problems else 0


def _cmd_verify_release(a) -> int:
    record = _load_record(a.record)
    profile = Q.load_profile(Path(a.profile))
    problems = verify_candidate_dir(record, profile, Path(a.root), Path(a.candidate_dir),
                                    a.tag)
    if problems:
        _emit(problems)
        return 1
    print(_verified_line(record))
    for p, s in sorted(record["package_hashes"].items()):
        print("  %s %s" % (s, p))
    if a.notes_out:
        Path(a.notes_out).parent.mkdir(parents=True, exist_ok=True)
        Path(a.notes_out).write_text(release_notes(record, a.tag), encoding="utf-8",
                                     newline="\n")
        print("release notes -> %s" % a.notes_out)
    return 0


# ---------------------------------------------------------------------------
# Record mode / dry run
# ---------------------------------------------------------------------------

def _fresh_download(gh, run_id, repo, work: Path, with_evidence: bool):
    for sub in ("candidate", "evidence"):
        shutil.rmtree(work / sub, ignore_errors=True)
    for p in PLATFORMS:
        if not gh.download(run_id, "candidate-%s" % p, work / "candidate" / p, repo):
            raise C.ValidationError("candidate-%s from run %s is unavailable (expired, or "
                                    "the run did not produce it)" % (p, run_id))
    if with_evidence:
        for name in EVIDENCE_ARTIFACTS:
            if not gh.download(run_id, name, work / "evidence" / name, repo):
                print("warning: artifact %s of run %s is unavailable; its required "
                      "entries will be missing" % (name, run_id), file=sys.stderr)


def _print_not_ready(report: dict) -> None:
    for g in report["global_reasons"]:
        print("  candidate: %s: %s" % (g["code"], g["detail"]))
    for b in report["blocking"][:60]:
        why = ", ".join(sorted({r["code"] for r in b["reasons"]}))
        print("  blocking: %s (%s)" % (b["key"], why))
    if len(report["blocking"]) > 60:
        print("  ... and %d more" % (len(report["blocking"]) - 60))
    for w in report["waived"]:
        print("  waived: %s (%s): %s" % (w["key"], w["status"], w["waiver"]["reason"]))
    for r in report["rejected_waivers"]:
        print("  waiver not applied: %s: %s" % (r["key"], r["why"]))


def _make_record(a, gh, root: Path, profile_path: Path, repo: str) -> int:
    run_id, version, work = a.run_id, a.version, Path(a.work_dir)
    profile = Q.load_profile(profile_path)
    run = gh.run_json(run_id, repo)
    _fresh_download(gh, run_id, repo, work, with_evidence=True)
    manifests = []
    for p in PLATFORMS:
        manifests.append(C.load_json_object(work / "candidate" / p / ("candidate-%s.json" % p)))
    if sorted(m.get("platform") for m in manifests) != sorted(PLATFORMS):
        raise C.ValidationError("candidate manifests are for %s, not %s"
                                % (sorted(m.get("platform") for m in manifests),
                                   sorted(PLATFORMS)))
    reference = C.load_json_object(Path(a.reference)) if a.reference else build_reference(root)
    candidate = Q.combine_candidate(manifests, repo, profile["digest"], reference)
    problems = check_run({"candidate": candidate}, run, repo)
    if problems:
        raise C.ValidationError("; ".join(problems))
    here = Q.source_state_digest(root)
    want = candidate["source"]["source_state_digest"]
    if here != want:
        raise C.ValidationError(
            "checkout source state %s is not the candidate's source state %s; check out "
            "commit %s (clean) and run again" % (here, want, candidate["source"]["commit"]))
    results, files = collect_results(work, a.evidence_dir, a.waivers)
    report = dict(Q.qualify(profile, candidate, results, version=version))
    report["input_artifacts"] = files
    report["input_files"] = {"profile": Q.sha256_file(profile_path)}
    report = C.seal(report)
    n = len(report["required"])
    ok = sum(1 for r in report["required"] if r["qualifies"])
    waived = len(report["waived"])
    print("ready: %s  required %d, qualifying %d, waived %d, blocking %d"
          % (str(report["ready"]).lower(), n, ok, waived, n - ok - waived))
    _print_not_ready(report)
    if not report["ready"]:
        stash = work / "qualification-not-ready.json"
        Q._write(report, stash)
        print("not ready; nothing written to %s (report: %s)" % (a.out, stash))
        return 1
    out = Path(a.out) / ("%s.json" % report["source_state_digest"])
    Q._write(report, out)
    print("wrote %s" % out)
    return 0


def _dry_run(a, gh, root: Path, profile_path: Path, repo: str) -> int:
    work = Path(a.work_dir)
    qdir = Path(a.out)
    digest, path = find_record(root, qdir)
    if path is None:
        _emit(["No reviewed qualification record %s for the checkout's source state"
               % (qdir / ("%s.json" % digest))])
        return 1
    record = _load_record(path)
    profile = Q.load_profile(profile_path)
    run = gh.run_json(record["candidate"]["candidate_run_id"], repo)
    _fresh_download(gh, record["candidate"]["candidate_run_id"], repo, work,
                    with_evidence=False)
    problems = check_run(record, run, repo)
    problems += verify_candidate_dir(record, profile, root, work / "candidate", a.tag)
    if problems:
        _emit(problems)
        print("dry run: release.yml would refuse this release (%d problem(s))" % len(problems))
        return 1
    print(_verified_line(record))
    notes = work / "RELEASE_NOTES.md"
    notes.write_text(release_notes(record, a.tag), encoding="utf-8", newline="\n")
    print("dry run: release.yml would stage the packages of run %s; draft notes -> %s"
          % (record["candidate"]["candidate_run_id"], notes))
    return 0


def _record_parser():
    ap = argparse.ArgumentParser(
        prog="python -m scripts.equivalence.release_record",
        description=__doc__.split("\n\n")[0],
        epilog="subcommands used by release.yml: find-record, check-run, verify-release")
    ap.add_argument("--run-id", required=True, help="successful build.yml run to qualify")
    ap.add_argument("--version", required=True, help="version being released, X.Y.Z")
    ap.add_argument("--waivers", action="append", default=[], type=Path,
                    help="ds1edit.review/1 file (or a list) of waivers; repeatable")
    ap.add_argument("--evidence-dir", action="append", default=[], type=Path,
                    help="directory of local-lane ds1edit.case/1 JSON files; repeatable")
    ap.add_argument("--reference", type=Path,
                    help="JSON {baseline_digest, contracts, adapters} instead of the "
                         "committed contracts and captures")
    ap.add_argument("--out", type=Path, default=None,
                    help="record directory (default release/qualification)")
    ap.add_argument("--repository", default=None, help="owner/name (default: gh repo view)")
    ap.add_argument("--profile", type=Path, default=None)
    ap.add_argument("--work-dir", type=Path, default=None)
    ap.add_argument("--root", type=Path, default=Path("."))
    ap.add_argument("--tag", default=None, help="tag for --dry-run-release (default v<version>)")
    ap.add_argument("--dry-run-release", action="store_true",
                    help="run release.yml's verification against the committed record")
    return ap


def _sub_parser(name: str):
    ap = argparse.ArgumentParser(prog="release_record " + name)
    if name == "find-record":
        ap.add_argument("--root", default=".")
        ap.add_argument("--qualification-dir", default=None)
    elif name == "check-run":
        ap.add_argument("--record", required=True)
        ap.add_argument("--run-json", required=True)
        ap.add_argument("--repository", required=True)
    else:
        ap.add_argument("--record", required=True)
        ap.add_argument("--candidate-dir", required=True)
        ap.add_argument("--tag", required=True)
        ap.add_argument("--profile", default=PROFILE_PATH)
        ap.add_argument("--root", default=".")
        ap.add_argument("--notes-out", default=None)
    return ap


_SUBCOMMANDS = {"find-record": _cmd_find_record, "check-run": _cmd_check_run,
                "verify-release": _cmd_verify_release}


def main(argv=None, *, gh=None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    try:
        if argv and argv[0] in _SUBCOMMANDS:
            a = _sub_parser(argv[0]).parse_args(argv[1:])
            return _SUBCOMMANDS[argv[0]](a)
        a = _record_parser().parse_args(argv)
    except SystemExit as exc:
        return 2 if exc.code else 0
    except (C.ValidationError, OSError, KeyError) as exc:
        print("invalid input: %s" % exc, file=sys.stderr)
        return 2
    try:
        if not _VERSION.match(a.version):
            raise C.ValidationError("--version %r is not X.Y.Z (no leading v)" % a.version)
        if not a.run_id.isdigit():
            raise C.ValidationError("--run-id %r is not a GitHub run ID" % a.run_id)
        root = Path(a.root)
        a.out = a.out if a.out is not None else root / QUALIFICATION_DIR
        a.tag = a.tag or "v%s" % a.version
        profile_path = a.profile if a.profile is not None else root / PROFILE_PATH
        a.work_dir = a.work_dir if a.work_dir is not None else \
            root / "tmp" / "release-record" / a.run_id
        gh = gh or GhClient()
        repo = a.repository or gh.repository()
        if a.dry_run_release:
            return _dry_run(a, gh, root, profile_path, repo)
        return _make_record(a, gh, root, profile_path, repo)
    except (C.ValidationError, KeyError) as exc:
        print("invalid input: %s" % exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
