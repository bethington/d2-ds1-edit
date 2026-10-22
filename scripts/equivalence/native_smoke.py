"""Native smoke workflows against an extracted release package (package F, task 2).

``run_native_smoke(extracted_root, profile, out_dir)`` runs the *extracted*
executable -- never ``bin/`` -- in a directory and environment this run owns,
and returns ``ds1edit.case/1`` results for the ``native.{platform}.*`` IDs in
``test/equivalence/profiles/release-core.json`` (requirement R8).

What is automated here, and what is not:

* ``cli-help``          help / --help / -h / unknown verb / missing arguments,
                        with exact exit codes and output.
* ``no-game-startup``   the GUI path with an owned INI pointing at an *empty*
                        directory: must fail cleanly, and must not discover a
                        game install through the registry, common paths or
                        remembered preferences on the host.
* ``selftest``          ``--selftest`` on declared maps against a game install
                        named in the owned INI; records display/renderer init
                        lines and a clean exit. A selftest proves the loop
                        draws and pumps input, nothing about user workflows.
* ``platform-startup``  Windows/Linux: the declared runtime files are present in
                        the extraction and the executable starts with a
                        minimal, declared environment. macOS: unavailable (the
                        app bundle / Input Monitoring path is manual).
* ``clean-environment`` always ``unavailable`` here. A developer host carries
                        undeclared prerequisites (this one has the VC++
                        redistributable installed), so it cannot establish it.
* ``gui-*``             ``not_run``. Those are manual (docs/testing/
                        native-release-checklist.md); ``manual_case`` turns a
                        tester's recorded result into a case.

Fails closed: every refusal, stale output, timeout, crash or missing identity
becomes a non-``pass`` status with a named failure; nothing is inferred.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform as _platform
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable

try:  # package form (python -m scripts.equivalence.native_smoke) or flat import
    from . import contracts as C
except ImportError:  # pragma: no cover
    import contracts as C  # type: ignore

TOOL_VERSION = "native_smoke/1"
PROFILE_SCHEMA = "ds1edit.native-profile/1"
LANE = "native-workflow"
CHUNK = 1024 * 1024
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
PLATFORMS = ("windows-x86", "linux-x86_64", "macos-arm64")

CASE_NAMES = ("cli-help", "no-game-startup", "selftest", "clean-environment",
              "gui-edit-undo-save-reopen", "gui-modal-input", "platform-startup")
MANUAL_CASES = ("gui-edit-undo-save-reopen", "gui-modal-input",
                "clean-environment", "platform-startup")

INI_NAME = "Ds1edit.ini"
DISPLAY_RE = re.compile(r"^display: (\d+)x(\d+), (.+?) driver(.*)$", re.M)
FRAMES_RE = re.compile(r"selftest: rendered (\d+) frame\(s\), exiting")


class SmokeError(ValueError):
    """The profile or arguments are unusable (not a product result)."""


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest()


def _read(path: Path) -> str:
    try:
        return Path(path).read_bytes().decode("utf-8", errors="replace")
    except OSError:
        return ""


def host_platform() -> str:
    machine = _platform.machine().lower()
    if sys.platform.startswith("win"):
        return "windows-x86"
    if sys.platform == "darwin":
        return "macos-arm64"
    if sys.platform.startswith("linux") and machine in ("x86_64", "amd64"):
        return "linux-x86_64"
    raise SmokeError("cannot map host %s/%s to a package platform"
                     % (sys.platform, machine))


def validate_profile(profile: dict) -> dict:
    if not isinstance(profile, dict) or profile.get("schema") != PROFILE_SCHEMA:
        raise SmokeError("profile schema must be %r" % PROFILE_SCHEMA)
    for key in ("id", "platforms", "timeouts", "cli_cases", "no_game", "selftest"):
        if key not in profile:
            raise SmokeError("profile is missing %r" % key)
    for name, p in profile["platforms"].items():
        if name not in PLATFORMS:
            raise SmokeError("unknown platform %r" % name)
        if not isinstance(p.get("executable"), str) or not p["executable"]:
            raise SmokeError("platform %s has no executable" % name)
    return profile


def load_profile(path: Path) -> dict:
    return validate_profile(json.loads(Path(path).read_text(encoding="utf-8")))


# ---------------------------------------------------------------------------
# Case construction
# ---------------------------------------------------------------------------

def _case(case_name: str, plat: str, status: str, *, prov: dict, inputs: int,
          failures=(), expected=None, actual=None, limitations=(),
          coverage=None) -> dict:
    assert status in C.STATUSES
    cov = {"distinct_inputs_compared": inputs, "uncovered_required_axes": []}
    cov.update(coverage or {})
    fails = []
    for f in failures:
        f = dict(f)
        f.setdefault("kind", case_name)
        f.setdefault("detail", f.get("message", ""))
        fails.append(f)
    pkg = prov.get("archive_sha256")
    return {
        "schema": C.CASE_SCHEMA,
        "id": "native.%s.%s" % (plat, case_name),
        "lane": LANE,
        "platform": plat,
        "required": True,
        "status": status,
        "success_outcome": "pass",
        "contract_digest": None,
        "baseline_digest": None,
        "input_sha256": pkg,
        "expected_sha256": C.canonical_digest({"v": expected}),
        "actual_sha256": C.canonical_digest({"v": actual}),
        "expected": expected,
        "actual": actual,
        "coverage": cov,
        "failures": fails,
        "provenance": prov,
        "limitations": list(limitations),
    }


def manual_case(case_name: str, plat: str, *, result: str, archive_sha256: str,
                executable_sha256: str, tester: str, environment: dict,
                notes: str, checklist_items: list, checklist_total: int) -> dict:
    """A ``ds1edit.case/1`` for a manual checklist result.

    ``result`` is ``pass`` or ``fail``. ``pass`` requires every checklist item
    done (``len(checklist_items) == checklist_total``); anything less is
    ``partial``. The case is bound to the package it was run against.
    """
    if case_name not in MANUAL_CASES and not case_name.startswith("gui-"):
        raise SmokeError("%r is not a manual native case" % case_name)
    if plat not in PLATFORMS:
        raise SmokeError("unknown platform %r" % plat)
    if result not in ("pass", "fail"):
        raise SmokeError("result must be pass or fail")
    for what, val in (("archive_sha256", archive_sha256),
                      ("executable_sha256", executable_sha256)):
        if not re.fullmatch(r"[0-9a-f]{64}", val or ""):
            raise SmokeError("%s must be a sha256 hex digest" % what)
    if not tester or not notes or not environment:
        raise SmokeError("tester, environment and notes are required")
    if checklist_total <= 0:
        raise SmokeError("checklist_total must be positive")
    done = len(set(checklist_items))
    if result == "fail":
        status = "mismatch"
    elif done >= checklist_total:
        status = "pass"
    else:
        status = "partial"
    prov = {"tool": TOOL_VERSION, "manual": True, "tester": tester,
            "environment": environment, "notes": notes,
            "archive_sha256": archive_sha256,
            "executable": {"sha256": executable_sha256},
            "candidate": {"packages": [archive_sha256],
                          "executable": {"sha256": executable_sha256}}}
    fails = [] if status == "pass" else [
        {"kind": "manual", "detail": "result=%s, %d/%d checklist items"
         % (result, done, checklist_total)}]
    return _case(case_name, plat, status, prov=prov, inputs=max(done, 0),
                 failures=fails, expected={"checklist_items": checklist_total},
                 actual={"checklist_items": done, "result": result},
                 limitations=["human-recorded; see notes"])


# ---------------------------------------------------------------------------
# Environment, identity and execution
# ---------------------------------------------------------------------------

def _owned_env(plat: str, home: Path, extra: dict, base: dict) -> dict:
    """A from-scratch environment: nothing is inherited except what is needed
    to load system DLLs / run at all. Every per-user location points at *home*."""
    home = str(home)
    tmp = os.path.join(home, "tmp")
    env: dict = {}
    if plat == "windows-x86":
        root = base.get("SystemRoot") or base.get("SYSTEMROOT") or r"C:\Windows"
        env.update({"SystemRoot": root, "windir": root,
                    "PATH": os.path.join(root, "System32") + os.pathsep + root,
                    "USERPROFILE": home, "APPDATA": os.path.join(home, "AppData", "Roaming"),
                    "LOCALAPPDATA": os.path.join(home, "AppData", "Local"),
                    "HOMEDRIVE": os.path.splitdrive(home)[0] or "C:",
                    "HOMEPATH": os.path.splitdrive(home)[1] or home,
                    "TEMP": tmp, "TMP": tmp})
        dirs = [env["APPDATA"], env["LOCALAPPDATA"], tmp]
    else:
        env.update({"PATH": "/usr/bin:/bin", "HOME": home,
                    "XDG_CONFIG_HOME": os.path.join(home, ".config"),
                    "XDG_DATA_HOME": os.path.join(home, ".local", "share"),
                    "XDG_CACHE_HOME": os.path.join(home, ".cache"),
                    "TMPDIR": tmp, "LANG": "C"})
        # Display variables are declared pass-throughs, not inherited defaults.
        for k in ("DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR"):
            if base.get(k):
                env[k] = base[k]
        dirs = [env["XDG_CONFIG_HOME"], env["XDG_DATA_HOME"], env["XDG_CACHE_HOME"], tmp]
    for d in dirs:
        os.makedirs(d, exist_ok=True)
    for k, v in (extra or {}).items():
        env[str(k)] = str(v)
    return env


def _is_crash(rc: int) -> bool:
    # POSIX: killed by a signal (negative); Windows: NTSTATUS 0xC.......
    return rc < 0 or rc >= 0x80000000


def _inside(child: Path, parent: Path) -> bool:
    child, parent = child.resolve(), parent.resolve()
    return child == parent or parent in child.parents


def _find_exe_root(extracted: Path, exe_rel: str) -> Path | None:
    if (extracted / exe_rel).is_file():
        return extracted
    subs = [d for d in extracted.iterdir() if d.is_dir()] if extracted.is_dir() else []
    hits = [d for d in subs if (d / exe_rel).is_file()]
    return hits[0] if len(hits) == 1 else None


def _discover_inspection(root: Path) -> dict | None:
    """The package-inspection.json written beside ``extracted/`` by package_check."""
    for up in (root, root.parent, root.parent.parent):
        f = up / "package-inspection.json"
        if f.is_file():
            try:
                rep = json.loads(f.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                return None
            er = rep.get("extracted_root")
            if er and _inside(root, Path(er)):
                return rep
            return None
    return None


class _Ctx:
    """Everything one run shares."""

    def __init__(self, **kw):
        self.__dict__.update(kw)
        self.ordinal = 0

    def evidence_dir(self, label: str) -> Path:
        self.ordinal += 1
        safe = re.sub(r"[^A-Za-z0-9._-]+", "_", label)
        d = self.evidence / ("%02d-%s" % (self.ordinal, safe))
        d.mkdir(parents=True, exist_ok=False)
        return d


def _run_process(ctx: _Ctx, label: str, args: list, timeout: float, *,
                 env: dict | None = None, require_files=()) -> dict:
    """Run the extracted executable once. Always returns an evidence dict;
    ``failures`` is non-empty for anything that is not a clean launch/exit."""
    d = ctx.evidence_dir(label)
    out_p, err_p = d / "stdout.txt", d / "stderr.txt"
    cmd = [str(ctx.exe)] + [str(a) for a in args]
    rec = {"name": label, "args": [str(a) for a in args], "returncode": None,
           "timed_out": False, "elapsed_s": None, "evidence": str(d),
           "failures": [], "_stdout": "", "_stderr": ""}
    stale = [str(p) for p in require_files if os.path.lexists(p)]
    if stale:
        rec["failures"].append({"kind": "stale_output", "path": stale[0],
                                "message": "output exists before launch; refusing to "
                                           "trust it: %s" % stale[0]})
        return rec
    t0 = time.monotonic()
    try:
        with open(out_p, "xb") as fo, open(err_p, "xb") as fe:
            cp = ctx.runner(cmd, cwd=str(ctx.root), env=env or ctx.env,
                            stdin=subprocess.DEVNULL, stdout=fo, stderr=fe,
                            timeout=timeout, shell=False)
        rec["returncode"] = cp.returncode
    except subprocess.TimeoutExpired:
        rec["timed_out"] = True
        rec["failures"].append({"kind": "timeout",
                                "message": "no exit within %ss" % timeout})
    except OSError as exc:
        rec["failures"].append({"kind": "launch", "message": "could not launch: %s" % exc})
    rec["elapsed_s"] = round(time.monotonic() - t0, 3)
    for key, p in (("stdout", out_p), ("stderr", err_p)):
        rec[key + "_sha256"] = sha256_file(p) if p.exists() else None
    rec["_stdout"], rec["_stderr"] = _read(out_p), _read(err_p)
    rc = rec["returncode"]
    if rc is not None and _is_crash(rc):
        rec["failures"].append({"kind": "crash", "message": "abnormal exit status %#x" % (rc & 0xFFFFFFFF)})
    for p in require_files:
        try:
            ok = Path(p).stat().st_size > 0
        except OSError:
            ok = False
        if not ok:
            rec["failures"].append({"kind": "missing_output", "path": str(p),
                                    "message": "expected output not written: %s" % p})
    return rec


def _check_text(rec: dict, text: str, must: list, absent: list):
    for s in must:
        if s not in text:
            rec["failures"].append({"kind": "output", "message": "missing expected text %r" % s})
    for s in absent:
        if s in text:
            rec["failures"].append({"kind": "output", "message": "unexpected text %r" % s})


def _finish(rec: dict) -> dict:
    rec.pop("_stdout", None)
    rec.pop("_stderr", None)
    rec["ok"] = not rec["failures"]
    return rec


def _status(subs: list, total: int) -> str:
    if not subs or not total:
        return "not_run"
    good = sum(1 for s in subs if s["ok"])
    if good == total:
        return "pass"
    return "mismatch"


class _OwnedIni:
    """Write the owned Ds1edit.ini next to the executable for one run, keep a
    copy as evidence, and always remove it afterwards."""

    def __init__(self, ctx: _Ctx, d2_install: str, label: str):
        self.path = ctx.root / INI_NAME
        # use_pd2 = no: auto-detection must never put Project Diablo 2 data
        # into vanilla evidence.
        self.text = ("d2_install = %s\nuse_pd2 = no\nfull_screen = NO\n"
                     "screen_width = 800\nscreen_height = 600\n" % d2_install)
        self.ctx, self.label = ctx, label

    def __enter__(self):
        if os.path.lexists(self.path):
            raise SmokeError("%s already exists in the extraction" % self.path)
        self.path.write_text(self.text, encoding="utf-8", newline="\n")
        keep = self.ctx.evidence / ("owned-ini-%s.txt" % self.label)
        keep.write_text(self.text, encoding="utf-8", newline="\n")
        return self

    def __exit__(self, *exc):
        try:
            os.remove(self.path)
        except OSError:
            pass
        return False


# ---------------------------------------------------------------------------
# The cases
# ---------------------------------------------------------------------------

def _run_cli(ctx: _Ctx, profile: dict) -> tuple:
    subs = []
    t = profile["timeouts"]["cli"]
    for c in profile["cli_cases"]:
        rec = _run_process(ctx, "cli-" + c["name"], c["args"], t)
        if rec["returncode"] is not None and rec["returncode"] != c["returncode"]:
            rec["failures"].append({"kind": "exit_code", "message": "exit %s, expected %s"
                                    % (rec["returncode"], c["returncode"]),
                                    "expected": c["returncode"], "actual": rec["returncode"]})
        _check_text(rec, rec["_stdout"] + "\n" + rec["_stderr"],
                    c.get("output_contains", []), c.get("output_absent", []))
        subs.append(_finish(rec))
    return subs, {"cases": [c["name"] for c in profile["cli_cases"]]}


def _run_no_game(ctx: _Ctx, profile: dict) -> tuple:
    ng = profile["no_game"]
    empty = ctx.home / "no-game-install"
    empty.mkdir(parents=True, exist_ok=True)
    if any(empty.iterdir()):
        raise SmokeError("no-game directory is not empty")
    with _OwnedIni(ctx, str(empty), "no-game"):
        rec = _run_process(ctx, "no-game-startup", ng["args"], profile["timeouts"]["startup"])
    text = rec["_stdout"] + "\n" + rec["_stderr"]
    rc = rec["returncode"]
    if rc == 0:
        rec["failures"].append({"kind": "exit_code", "message":
                                "exit 0 with no game data: a game install was found somewhere "
                                "other than the owned INI"})
    _check_text(rec, text, ng.get("output_contains", []) + ["using configured path <%s>" % empty],
                ng.get("output_absent", []))
    return [_finish(rec)], {"expect_nonzero_exit": True}


def _mpq_identity(game: Path) -> dict:
    out = {}
    for name in ("patch_d2.mpq", "d2exp.mpq", "d2data.mpq", "d2char.mpq"):
        found = next((p for p in game.iterdir() if p.name.lower() == name), None) \
            if game.is_dir() else None
        out[name] = ({"size": found.stat().st_size, "sha256": sha256_file(found)}
                     if found else None)
    return out


def _run_selftest(ctx: _Ctx, profile: dict, game: Path) -> tuple:
    st = profile["selftest"]
    subs = []
    shots = ctx.evidence / "shots"
    shots.mkdir(exist_ok=True)
    for m in st["maps"]:
        shot = shots / ("%s.png" % m)
        args = ["assets/%s.ini" % m, "--selftest=%d" % st["frames"],
                "--selftest-shot=%s" % shot]
        with _OwnedIni(ctx, str(game), "selftest-" + m):
            rec = _run_process(ctx, "selftest-" + m, args, profile["timeouts"]["selftest"],
                               require_files=[shot])
        text = rec["_stdout"] + "\n" + rec["_stderr"]
        if rec["returncode"] not in (None, 0):
            rec["failures"].append({"kind": "exit_code", "message": "exit %s, expected 0"
                                    % rec["returncode"]})
        _check_text(rec, text, st.get("output_contains", []) + ["using configured path <%s>" % game],
                    st.get("output_absent", []))
        mm = FRAMES_RE.search(text)
        if rec["returncode"] == 0 and not rec["failures"] and not mm:
            rec["failures"].append({"kind": "output", "message": "no frame-count line"})
        elif mm and int(mm.group(1)) != st["frames"]:
            rec["failures"].append({"kind": "output", "message": "rendered %s frames, wanted %d"
                                    % (mm.group(1), st["frames"])})
        disp = DISPLAY_RE.search(text)
        if disp is None:
            rec["failures"].append({"kind": "display_init", "message":
                                    "no 'display: WxH, <driver> driver' initialisation line"})
        else:
            rec["display"] = {"width": int(disp.group(1)), "height": int(disp.group(2)),
                              "driver": disp.group(3), "line": disp.group(0)}
        rec["display_lines"] = [l for l in text.splitlines() if l.startswith("display:")]
        if shot.is_file():
            head = shot.read_bytes()[:8]
            rec["shot"] = {"path": str(shot), "sha256": sha256_file(shot),
                           "size": shot.stat().st_size}
            if head != PNG_MAGIC:
                rec["failures"].append({"kind": "missing_output", "path": str(shot),
                                        "message": "selftest shot is not a PNG"})
        subs.append(_finish(rec))
    return subs, {"maps": st["maps"], "frames": st["frames"], "windowed": True}


def _run_platform_startup(ctx: _Ctx, profile: dict, plat: str, pdef: dict) -> tuple:
    missing = [r for r in pdef.get("runtime_files", []) if not (ctx.root / r).is_file()]
    if missing:
        return None, missing
    return [_finish(_run_process(ctx, "platform-startup-help", ["help"],
                                 profile["timeouts"]["cli"]))], []


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def run_native_smoke(extracted_root: Path, profile: dict, out_dir: Path, *,
                     platform: str | None = None, runner: Callable | None = None,
                     game_install: str | Path | None = None,
                     inspection: dict | None = None, environ: dict | None = None,
                     write: bool = True) -> list[dict]:
    """Run the native smoke cases; returns ``ds1edit.case/1`` dicts.

    ``out_dir`` must not exist (it is owned by the run): evidence goes under
    ``evidence/``, case JSON under ``results/`` (a directory ``qualify`` can load).
    """
    validate_profile(profile)
    plat = platform or profile.get("platform") or host_platform()
    if plat not in profile["platforms"]:
        raise SmokeError("profile has no platform %r" % plat)
    pdef = profile["platforms"][plat]
    runner = runner or subprocess.run
    base_env = dict(os.environ if environ is None else environ)
    extracted_root = Path(extracted_root)
    out_dir = Path(out_dir)
    if out_dir.exists():
        raise SmokeError("out_dir %s already exists; it must be fresh" % out_dir)
    out_dir.mkdir(parents=True)
    out_dir = out_dir.resolve()
    evidence = out_dir / "evidence"
    evidence.mkdir()
    (out_dir / "results").mkdir()
    home = out_dir / "home"
    home.mkdir()

    version = profile.get("version")
    exe_rel = pdef["executable"]
    cases: list[dict] = []

    # -- identities ---------------------------------------------------------
    root = _find_exe_root(extracted_root, exe_rel) if extracted_root.is_dir() else None
    problems: list[dict] = []          # preconditions that forbid launching
    exe = None
    rel_parts = Path(exe_rel.replace("\\", "/")).parts
    if os.path.isabs(exe_rel) or ".." in rel_parts or re.match(r"^[A-Za-z]:", exe_rel):
        problems.append({"kind": "executable_outside_extraction",
                         "message": "declared executable %r is not a path inside the "
                                    "extraction" % exe_rel})
        root = extracted_root if extracted_root.is_dir() else None
    elif root is None:
        problems.append({"kind": "executable_missing",
                         "message": "%s not found under %s" % (exe_rel, extracted_root)})
    else:
        exe = root / exe_rel
        # no symlink hops: the executable and every parent up to the root are real
        hop = exe
        while hop != root:
            if hop.is_symlink():
                problems.append({"kind": "executable_outside_extraction",
                                 "message": "%s is a symlink" % hop})
                break
            hop = hop.parent
        if not problems and not _inside(exe, root):
            problems.append({"kind": "executable_outside_extraction",
                             "message": "%s resolves outside %s" % (exe, root)})
    if root is not None and (root / INI_NAME).exists() and not problems:
        problems.append({"kind": "user_ini_in_extraction",
                         "message": "%s exists in the extraction; a user config could be "
                                    "picked up" % (root / INI_NAME)})

    exe_sha = sha256_file(exe) if (exe and not problems) else None
    insp = inspection if inspection is not None else \
        (_discover_inspection(root) if root is not None else None)
    pkg_sha = None
    if insp and insp.get("status") == "pass" and insp.get("platform") == plat:
        pkg_sha = (insp.get("archive") or {}).get("sha256")
    if version is None and insp:
        version = insp.get("expected_version")

    game = Path(game_install) if game_install else (
        Path(profile["game_install"]) if profile.get("game_install") else None)

    prov = {"tool": TOOL_VERSION, "profile": profile["id"], "archive_sha256": pkg_sha,
            "package_version": version,
            "executable": {"path": exe_rel, "sha256": exe_sha},
            "extracted_root": str(root) if root else None,
            "candidate": {"packages": [pkg_sha] if pkg_sha else [],
                          "executable": {"sha256": exe_sha}},
            "environment": {
                "host_os": _platform.platform(), "python": sys.version.split()[0],
                "clean_environment": False,
                "disclosure": profile.get("environment_disclosure", []),
                "owned_working_dir": str(root) if root else None,
                "owned_home": str(home),
                "declared_env_keys": None},
            "game": None}

    def blocked(case_name, status, failures, limitations=()):
        cases.append(_case(case_name, plat, status, prov=dict(prov), inputs=0,
                           failures=failures, limitations=limitations))

    def finish():
        order = {n: i for i, n in enumerate(CASE_NAMES)}
        cases.sort(key=lambda c: order[c["id"].split(".", 2)[2]])
        if write:
            for c in cases:
                (out_dir / "results" / ("%s.json" % c["id"])).write_text(
                    json.dumps(c, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            (out_dir / "native-smoke-report.json").write_text(json.dumps({
                "schema": "ds1edit.native-smoke/1", "tool": TOOL_VERSION, "platform": plat,
                "profile": profile["id"], "provenance": prov,
                "cases": [{"id": c["id"], "status": c["status"]} for c in cases]},
                indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return cases

    manual_limit = ["manual: see docs/testing/native-release-checklist.md"]

    def add_static():
        blocked("clean-environment", "unavailable",
                [{"kind": "clean_environment",
                  "message": "not run: this host has development tools and undeclared "
                             "prerequisites (e.g. an installed VC++ redistributable); a "
                             "clean VM/base image run is required and has not been done"}],
                ["developer host only"])
        for n in ("gui-edit-undo-save-reopen", "gui-modal-input"):
            blocked(n, "not_run",
                    [{"kind": "manual", "message": "GUI workflow is manual; no recorded result"}],
                    manual_limit)

    if problems or exe is None or root is None:
        for n in ("cli-help", "no-game-startup", "selftest", "platform-startup"):
            blocked(n, "unavailable", problems or [{"kind": "executable_missing",
                                                    "message": "no executable"}])
        add_static()
        return finish()

    if pkg_sha is None:
        problems.append({"kind": "package_identity",
                         "message": "no passing package-inspection.json for this extraction "
                                    "and platform; the package hash is unknown"})

    # -- declared runtime closure ------------------------------------------
    missing_rt = [r for r in pdef.get("runtime_files", []) if not (root / r).is_file()]
    if missing_rt:
        miss = [{"kind": "missing_runtime", "path": r,
                 "message": "declared runtime file missing from the extraction: %s" % r}
                for r in missing_rt]
        blocked("platform-startup", "mismatch", miss)
        for n in ("cli-help", "no-game-startup", "selftest"):
            blocked(n, "not_run", [{"kind": "missing_runtime", "message":
                                    "not launched: declared runtime missing (a loader error "
                                    "dialog could block the run): %s" % ", ".join(missing_rt)}])
        add_static()
        return finish()

    if problems:
        for n in ("cli-help", "no-game-startup", "selftest", "platform-startup"):
            blocked(n, "unavailable", problems)
        add_static()
        return finish()

    env = _owned_env(plat, home, profile.get("env", {}), base_env)
    prov["environment"]["declared_env_keys"] = sorted(env)
    ctx = _Ctx(root=root, exe=exe, env=env, runner=runner, evidence=evidence, home=home)

    def result(case_name, subs, expected, limitations=(), cov=None, full=None):
        total = len(subs)
        st = _status(subs, full if full is not None else total)
        fails = [dict(f, sub=s["name"]) for s in subs for f in s["failures"]]
        passed = sum(1 for s in subs if s["ok"])
        cases.append(_case(case_name, plat, st, prov=dict(prov), inputs=passed,
                           failures=fails, expected=expected,
                           actual={"subcases": [{k: v for k, v in s.items() if k != "failures"}
                                                for s in subs]},
                           limitations=limitations, coverage=cov))

    try:
        subs, exp = _run_cli(ctx, profile)
        result("cli-help", subs, exp)
        subs, exp = _run_no_game(ctx, profile)
        result("no-game-startup", subs, exp,
               limitations=["proves the owned empty-install path fails cleanly; registry/common-"
                            "path auto-detection is bypassed by an explicit d2_install"])
        if pdef.get("startup") == "app-bundle" or plat == "macos-arm64":
            blocked("platform-startup", "unavailable",
                    [{"kind": "manual", "message": "macOS app bundle / Input Monitoring "
                                                   "startup is manual"}], manual_limit)
        else:
            subs, _ = _run_platform_startup(ctx, profile, plat, pdef)
            result("platform-startup", subs, {"runtime_files": pdef.get("runtime_files", []),
                                              "minimal_env": True},
                   limitations=["startup of `help` under a declared minimal environment; "
                                "not a clean-machine test"])
        if game is None or not game.is_dir():
            blocked("selftest", "unavailable",
                    [{"kind": "game_install", "message": "no game install directory given"}])
        else:
            prov["game"] = {"install": str(game), "mpq": _mpq_identity(game)}
            subs, exp = _run_selftest(ctx, profile, game)
            result("selftest", subs, exp,
                   limitations=["renders frames and pumps input; proves no user workflow"],
                   cov={"maps": [s["name"] for s in subs if s["ok"]]})
    finally:
        # The package must be what it was: the executable may not change under test.
        if exe_sha and sha256_file(exe) != exe_sha:
            for c in cases:
                if c["status"] == "pass":
                    c["status"] = "mismatch"
                    c["failures"].append({"kind": "executable_changed", "detail":
                                          "executable bytes changed during the run",
                                          "message": "executable bytes changed during the run"})
    add_static()
    return finish()


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="native_smoke", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run", help="run the automated native cases")
    r.add_argument("--extracted-root", required=True, type=Path)
    r.add_argument("--profile", required=True, type=Path)
    r.add_argument("--out-dir", required=True, type=Path)
    r.add_argument("--platform", choices=PLATFORMS)
    r.add_argument("--game-install", type=Path)
    m = sub.add_parser("manual", help="record a manual checklist result as a case")
    m.add_argument("--case", required=True, choices=list(CASE_NAMES))
    m.add_argument("--platform", required=True, choices=PLATFORMS)
    m.add_argument("--result", required=True, choices=("pass", "fail"))
    m.add_argument("--smoke-report", required=True, type=Path,
                   help="native-smoke-report.json of the run on the same package")
    m.add_argument("--tester", required=True)
    m.add_argument("--environment", required=True,
                   help="free text: OS build, hardware/VM, installed prerequisites")
    m.add_argument("--notes", required=True)
    m.add_argument("--items", required=True, type=int,
                   help="checklist items completed")
    m.add_argument("--total", required=True, type=int, help="checklist items in the section")
    m.add_argument("--out", required=True, type=Path)
    try:
        args = ap.parse_args(argv)
    except SystemExit as exc:
        return 2 if exc.code else 0
    try:
        if args.cmd == "run":
            profile = load_profile(args.profile)
            cases = run_native_smoke(args.extracted_root, profile, args.out_dir,
                                     platform=args.platform, game_install=args.game_install)
            for c in cases:
                print("%-12s %s" % (c["status"], c["id"]))
                for f in c["failures"][:6]:
                    print("    - %s" % f.get("message", f.get("detail", "")))
            bad = [c for c in cases if c["status"] != "pass"]
            return 1 if bad else 0
        rep = json.loads(args.smoke_report.read_text(encoding="utf-8"))
        prov = rep["provenance"]
        case = manual_case(args.case, args.platform, result=args.result,
                           archive_sha256=prov.get("archive_sha256") or "",
                           executable_sha256=(prov.get("executable") or {}).get("sha256") or "",
                           tester=args.tester, environment={"description": args.environment},
                           notes=args.notes, checklist_items=list(range(args.items)),
                           checklist_total=args.total)
        if args.case == "clean-environment":
            case["provenance"]["environment"]["clean_environment"] = True
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(case, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print("%s %s -> %s" % (case["status"], case["id"], args.out))
        return 0 if case["status"] == "pass" else 1
    except (SmokeError, C.ValidationError, OSError, ValueError, KeyError) as exc:
        print("native_smoke: %s" % exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
