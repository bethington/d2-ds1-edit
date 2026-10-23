"""Deterministic map renders against pinned inputs (package D, task 4).

A *visual profile* (``ds1edit.visual-profile/1``, see
``test/equivalence/profiles/visual-core.json``) names map render cases. Each
case pins the map (LvlTypes id, LvlPrest def, DS1 path), the exact asset bytes
it must read (by SHA-256, resolved against B's ``ds1edit.baseline/1`` corpus
manifest), the owned render configuration (zoom, gamma, seed, frame, time --
nothing comes from a local ``ds1edit.ini`` or user preferences), the reference
image (by SHA-256) and the features it exercises.

``render_cases(profile, baseline, candidate, out_dir)`` renders every case
with the candidate executable in a private runtime directory, checks that the
renderer read only pinned bytes from the identified archives, renders twice
to prove the output is deterministic, compares RGB pixels exactly with the
reference, and returns ``ds1edit.case/1`` results: one per map plus the
aggregate feature-coverage case (``profile["aggregate_case"]``).

Statuses follow B's semantics. Nothing here passes by default:

- ``unavailable``: the comparison could not be made meaningfully (no game
  directory, an archive or pinned input whose bytes differ from the baseline,
  a changed reference image, a missing editor asset).
- ``mismatch``: the comparison ran and the candidate disagreed, including
  renders that read from a mod directory or user preferences, renders that are
  not reproducible, and renderer failures.
- ``not_run``: selected but never executed.
- ``partial``: the aggregate when a required feature has no executed passing
  case in this run.

The first point of difference is reported as a stage: ``process`` (the
renderer failed), ``resource_selection`` (bytes read were not the pinned
ones, or came from somewhere else), ``determinism`` (two renders differ),
``final_rgb`` (pixels differ; with count, max channel difference and box).
The headless renderer does not expose its indexed buffers or palettes, so
those stages are not separable for whole-map renders; the decoder and palette
cases (decoder_cases.py, visual_values.py) cover them in isolation.

Command (local lane: needs a licensed 1.13c installation)::

    python -m scripts.equivalence.render_cases run \\
        --profile test/equivalence/profiles/visual-core.json \\
        --baseline tmp/equivalence/baseline-corpus-113c.json \\
        --candidate bin/ds1edit.exe --game-dir "<1.13c directory>" \\
        --out-dir tmp/visual-cases

Exit 0 only when every required case (and the aggregate) passed; 1 when any
mismatched; 2 for invalid input or anything unavailable/not run/partial.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
from pathlib import Path
from typing import Callable, Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import contracts as C  # noqa: E402

PROFILE_SCHEMA = "ds1edit.visual-profile/1"
LANE = "corpus"
DEFAULT_PLATFORM = "windows-x86"

#: Every render setting a case depends on. Each must be pinned explicitly.
RENDER_KEYS = ("zoom", "gamma", "mod_dir", "preferences", "seed", "frame",
               "time", "repeat")
GAMMAS = {"0.60", "0.80", "1.00", "1.10", "1.20", "1.30", "1.40", "1.50"}
ARCHIVE_SLOTS = (("patch_d2", "patch_d2.mpq"), ("d2exp", "d2exp.mpq"),
                 ("d2data", "d2data.mpq"), ("d2char", "d2char.mpq"))
_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_ZOOM = re.compile(r"^1:(1|2|4|8|16)$")

Runner = Callable[[Path, dict, Path], object]


class ProfileError(ValueError):
    """The profile cannot define a reproducible render."""


# ---------------------------------------------------------------------------
# profile
# ---------------------------------------------------------------------------

def _need(obj, key, typ, where):
    if not isinstance(obj, dict) or key not in obj:
        raise ProfileError(f"{where}: missing {key!r}")
    v = obj[key]
    if not isinstance(v, typ) or (isinstance(v, bool) and typ is not bool):
        raise ProfileError(f"{where}.{key}: expected {typ}")
    return v


def _sha(obj, key, where):
    v = _need(obj, key, str, where)
    if not _HEX64.match(v):
        raise ProfileError(f"{where}.{key}: not a lower-case SHA-256")
    return v


def validate_profile(profile: dict) -> dict:
    if _need(profile, "schema", str, "profile") != PROFILE_SCHEMA:
        raise ProfileError(f"profile.schema must be {PROFILE_SCHEMA}")
    _need(profile, "id", str, "profile")
    base = _need(profile, "baseline", dict, "profile")
    if _need(base, "build", str, "profile.baseline") != C.BASELINE_BUILD:
        raise ProfileError("profile.baseline.build must be vanilla-113c")
    _sha(base, "digest", "profile.baseline")

    render = _need(profile, "render", dict, "profile")
    for k in RENDER_KEYS:
        if k not in render or render[k] in (None, "", "unpinned", "any"):
            raise ProfileError(f"profile.render.{k} is not pinned")
    if not _ZOOM.match(str(render["zoom"])):
        raise ProfileError("profile.render.zoom must be 1:N")
    if render["gamma"] not in GAMMAS:
        raise ProfileError("profile.render.gamma must name one gamma step")
    if render["mod_dir"] != "none":
        raise ProfileError("profile.render.mod_dir must be 'none' (no overlays)")
    if render["preferences"] != "isolated":
        raise ProfileError("profile.render.preferences must be 'isolated'")
    if not isinstance(render["repeat"], int) or render["repeat"] < 2:
        raise ProfileError("profile.render.repeat must be >= 2 (determinism)")

    for i, a in enumerate(_need(profile, "editor_assets", list, "profile")):
        _need(a, "path", str, f"editor_assets[{i}]")
        _sha(a, "sha256", f"editor_assets[{i}]")

    features = _need(profile, "required_features", list, "profile")
    if not features or not all(isinstance(f, str) and f for f in features):
        raise ProfileError("profile.required_features must be non-empty strings")
    external = profile.get("external_features", {})
    if not isinstance(external, dict):
        raise ProfileError("profile.external_features must map feature -> case ids")
    _need(profile, "aggregate_case", str, "profile")

    cases = _need(profile, "cases", list, "profile")
    if not cases:
        raise ProfileError("profile.cases is empty")
    seen = set()
    for i, c in enumerate(cases):
        w = f"cases[{i}]"
        cid = _need(c, "id", str, w)
        if cid in seen:
            raise ProfileError(f"{w}: duplicate id {cid}")
        seen.add(cid)
        m = _need(c, "map", dict, w)
        for k in ("lvltype", "lvlprest"):
            if not isinstance(m.get(k), int) or m[k] < 0:
                raise ProfileError(f"{w}.map.{k} must be a non-negative int")
        _need(m, "ds1", str, f"{w}.map")
        act = _need(c, "act", int, w)
        if not 1 <= act <= 5:
            raise ProfileError(f"{w}.act must be 1..5")
        if not _need(c, "features", list, w):
            raise ProfileError(f"{w}.features is empty")
        inputs = _need(c, "inputs", list, w)
        if not inputs:
            raise ProfileError(f"{w}.inputs is empty: inputs must be pinned")
        for j, inp in enumerate(inputs):
            _need(inp, "path", str, f"{w}.inputs[{j}]")
            _sha(inp, "sha256", f"{w}.inputs[{j}]")
        ref = _need(c, "reference", dict, w)
        _need(ref, "path", str, f"{w}.reference")
        _sha(ref, "sha256", f"{w}.reference")
        if _need(c, "compare", str, w) != "exact-rgb":
            raise ProfileError(f"{w}.compare: only 'exact-rgb' is defined; a "
                               "tolerance must be a named contract rule")
    return profile


def profile_digest(profile: dict) -> str:
    return C.canonical_digest(profile)


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def baseline_asset_key(name: str) -> str:
    """The baseline manifest key for a path as the renderer names it."""
    p = name.strip().replace("/", "\\").lower()
    i = p.rfind("\\tiles\\")
    if p.startswith("assets\\tiles\\") or (i >= 0 and not p.startswith("data\\")):
        p = "data\\global\\tiles\\" + p[p.find("tiles\\") + 6:]
    if not p.startswith("data\\"):
        p = "data\\" + p
    return p


_WANT = re.compile(r"^(?:want to read .*?|reading ds1)\s*:\s*(\S.*?)\s*$")
_FOUND = re.compile(r"^ok \((\d+) bytes\), found in (.+?)\s*$")


def parse_resource_reads(stdout: str) -> list[dict]:
    """Pair each 'want to read ... : P' / 'reading ds1 : P' with its result."""
    reads, pending = [], None
    for line in stdout.splitlines():
        line = line.strip()
        m = _WANT.match(line)
        if m:
            pending = m.group(1)
            continue
        f = _FOUND.match(line)
        if f and pending is not None:
            reads.append({"name": pending, "size": int(f.group(1)),
                          "source": f.group(2)})
            pending = None
    return reads


def _rgb_image(path: Path):
    from PIL import Image
    with Image.open(path) as im:
        im.load()
        return im.convert("RGB")


def _pixel_sha(img) -> str:
    h = hashlib.sha256()
    h.update(("%dx%d:" % img.size).encode())
    h.update(img.tobytes())
    return h.hexdigest()


def compare_rgb(expected_png: Path, actual_png: Path) -> dict:
    """Exact RGB comparison: count, max channel difference, bounding box."""
    from PIL import ImageChops
    e, a = _rgb_image(expected_png), _rgb_image(actual_png)
    out = {"expected_size": list(e.size), "actual_size": list(a.size),
           "expected_sha256": _pixel_sha(e), "actual_sha256": _pixel_sha(a)}
    if e.size != a.size:
        out.update(equal=False, reason="size")
        return out
    d = ImageChops.difference(e, a)
    r, g, b = d.split()
    m = ImageChops.lighter(ImageChops.lighter(r, g), b)
    hist = m.histogram()
    total = e.size[0] * e.size[1]
    diff = total - hist[0]
    out.update(equal=diff == 0, diff_pixels=diff, total_pixels=total,
               max_channel_diff=max((i for i, n in enumerate(hist) if n), default=0),
               bbox=list(m.getbbox()) if diff else None)
    return out


# ---------------------------------------------------------------------------
# runtime
# ---------------------------------------------------------------------------

def find_archives(game_dir: Path) -> dict:
    """slot -> path for the four archives, matched case-insensitively."""
    names = {p.name.lower(): p for p in Path(game_dir).iterdir() if p.is_file()}
    return {slot: names.get(fname) for slot, fname in ARCHIVE_SLOTS}


def check_archives(game_dir: Path, baseline: dict, cache: dict) -> list[str]:
    """Problems with the game directory against the baseline's archives."""
    problems = []
    if game_dir is None or not Path(game_dir).is_dir():
        return [f"game directory not available: {game_dir}"]
    found = find_archives(Path(game_dir))
    want = {a["name"].lower(): a for a in baseline["archives"]}
    for slot, fname in ARCHIVE_SLOTS:
        p = found.get(slot)
        ident = want.get(fname)
        if ident is None:
            problems.append(f"baseline has no archive {fname}")
            continue
        if p is None:
            problems.append(f"{fname} not found in the game directory")
            continue
        key = str(p)
        if key not in cache:
            cache[key] = sha256_file(p)
        if cache[key] != ident["sha256"]:
            problems.append(f"{fname} bytes differ from the baseline")
    return problems


INI_TEMPLATE = """d2char   = {d2char}
d2data   = {d2data}
d2exp    = {d2exp}
patch_d2 = {patch_d2}
mod_dir  =
use_pd2  = no
upscale_enabled = NO
full_screen = NO
gamma_correction = {gamma}
default_zoom = {zoom}
only_normal_type2 = YES
always_max_layers = YES
"""


def build_runtime(candidate: Path, profile: dict, game_dir: Path,
                  runtime: Path) -> list[str]:
    """Private runtime: the candidate and its DLLs, pinned editor assets and an
    owned ds1edit.ini. Returns problems (empty when usable)."""
    problems = []
    candidate = Path(candidate)
    if runtime.exists():
        shutil.rmtree(runtime)
    (runtime / "assets" / "editor").mkdir(parents=True)
    (runtime / "appdata").mkdir()
    if candidate.is_file():
        shutil.copy2(candidate, runtime / candidate.name)
        for f in candidate.parent.iterdir():
            if f.is_file() and f.suffix.lower() in (".dll", ".so", ".dylib", ".cfg"):
                shutil.copy2(f, runtime / f.name)
    # Interface artwork the editor insists on at start-up; it never reaches a
    # map render, so it is copied rather than pinned.
    ui = REPO_ROOT / "assets" / "ui"
    if ui.is_dir():
        shutil.copytree(ui, runtime / "assets" / "ui")
    for a in profile["editor_assets"]:
        src = REPO_ROOT / a["path"]
        if not src.is_file():
            problems.append(f"editor asset missing: {a['path']}")
            continue
        if sha256_file(src) != a["sha256"]:
            problems.append(f"editor asset bytes differ: {a['path']}")
            continue
        shutil.copy2(src, runtime / "assets" / "editor" / src.name)
    arch = find_archives(Path(game_dir)) if game_dir and Path(game_dir).is_dir() else {}
    r = profile["render"]
    (runtime / "ds1edit.ini").write_text(INI_TEMPLATE.format(
        gamma=r["gamma"], zoom=r["zoom"],
        **{slot: str(arch.get(slot) or "") for slot, _ in ARCHIVE_SLOTS}),
        encoding="ascii")
    return problems


def default_runner(runtime: Path, case: dict, out_png: Path, *,
                   exe_name: str, zoom: str):
    """Render through A's strict headless runner, with preferences isolated."""
    from scripts import run_golden_tests as G
    env_keys = ("APPDATA", "XDG_CONFIG_HOME", "HOME")
    saved = {k: os.environ.get(k) for k in env_keys}
    old_zoom = G.GOLDEN_ZOOM
    try:
        for k in env_keys:
            os.environ[k] = str(runtime / "appdata")
        G.GOLDEN_ZOOM = zoom
        m = case["map"]
        return G.render_case(runtime / exe_name, runtime,
                             (m["lvltype"], m["lvlprest"]), m["ds1"], out_png)
    finally:
        G.GOLDEN_ZOOM = old_zoom
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


def _png_from_render(path: Path) -> Path:
    """The renderer writes BMP; keep a PNG of the same pixels."""
    img = _rgb_image(path)
    png = path.with_suffix(".png")
    img.save(png, "PNG")
    if png != path:
        path.unlink()
    return png


# ---------------------------------------------------------------------------
# cases
# ---------------------------------------------------------------------------

def _case(profile, pdigest, baseline, cdef, platform, *, status, failures,
          input_sha, expected_sha=None, actual_sha=None, coverage=None,
          candidate_prov=None, limitations=()):
    return {
        "schema": C.CASE_SCHEMA, "id": cdef["id"], "lane": LANE,
        "platform": platform, "required": bool(cdef.get("required", True)),
        "status": status, "success_outcome": "pass",
        "contract_digest": pdigest, "baseline_digest": baseline.get("digest"),
        "input_sha256": input_sha, "expected_sha256": expected_sha,
        "actual_sha256": actual_sha, "coverage": coverage or {},
        "failures": failures,
        "provenance": {"candidate": candidate_prov or {},
                       "reference": {"profile": profile["id"],
                                     "image": cdef["reference"]["path"],
                                     "image_sha256": cdef["reference"]["sha256"],
                                     "origin": cdef["reference"].get("origin")}},
        "limitations": list(profile.get("limitations", [])) + list(limitations),
    }


def _input_payload(profile, cdef):
    return C.payload_sha256({"map": cdef["map"], "inputs": cdef["inputs"],
                             "render": profile["render"],
                             "editor_assets": profile["editor_assets"]})


def check_inputs(cdef: dict, baseline_assets: dict) -> list[dict]:
    """Pinned inputs that the baseline does not back byte-for-byte."""
    bad = []
    for inp in cdef["inputs"]:
        key = baseline_asset_key(inp["path"])
        a = baseline_assets.get(key)
        if a is None:
            bad.append({"kind": "input_unknown", "path": key,
                        "expected": inp["sha256"], "actual": None})
        elif a["sha256"] != inp["sha256"]:
            bad.append({"kind": "input_hash", "path": key,
                        "expected": inp["sha256"], "actual": a["sha256"]})
    return bad


def check_reads(reads: list[dict], stdout: str, cdef: dict, game_dir,
                runtime: Path) -> list[dict]:
    """What the renderer actually read, against what the case pinned."""
    fails = []
    if "seeded from preferences" in stdout:
        fails.append({"kind": "preferences", "stage": "resource_selection",
                      "path": "d2install",
                      "expected": "isolated", "actual": "seeded from preferences"})
    if "Project Diablo 2 data from" in stdout:
        fails.append({"kind": "game_data", "stage": "resource_selection",
                      "path": "d2install", "expected": "vanilla",
                      "actual": "Project Diablo 2 data in use"})
    m = re.search(r"^mod_dir\s*=\s*(.*)$", stdout, re.M)
    if m and m.group(1).strip() not in ("(null)", ""):
        fails.append({"kind": "overlay", "stage": "resource_selection",
                      "path": "mod_dir", "expected": "none",
                      "actual": m.group(1).strip()})
    pinned = {baseline_asset_key(i["path"]) for i in cdef["inputs"]}
    gd = str(Path(game_dir)).lower().rstrip("\\/") if game_dir else None
    seen = set()
    for r in reads:
        src = r["source"].strip()
        local_editor = src in (".", str(runtime)) or src.lower().endswith("assets\\editor\\")
        if local_editor and ("assets\\editor" in r["name"].replace("/", "\\").lower()
                             or r["name"].lower().endswith("obj.txt")
                             or r["name"].lower().endswith("ds1edit.dt1")):
            continue
        key = baseline_asset_key(r["name"])
        if gd is None or not src.lower().startswith(gd):
            fails.append({"kind": "overlay", "stage": "resource_selection",
                          "path": key, "expected": "game archive",
                          "actual": src})
            continue
        seen.add(key)
        if key not in pinned:
            fails.append({"kind": "unpinned_input", "stage": "resource_selection",
                          "path": key, "expected": "pinned", "actual": "read"})
    for key in sorted(pinned - seen):
        fails.append({"kind": "pinned_input_not_read", "stage": "resource_selection",
                      "path": key, "expected": "read", "actual": None})
    return fails


def run_case(profile, pdigest, baseline, assets, cdef, candidate, game_dir,
             out_dir, platform, runner, archive_problems):
    case_dir = (Path(out_dir) / "cases" / cdef["id"]).resolve()
    if case_dir.exists():
        shutil.rmtree(case_dir)
    case_dir.mkdir(parents=True)
    input_sha = _input_payload(profile, cdef)
    cand = {"executable": {"sha256": sha256_file(candidate)
                           if Path(candidate).is_file() else None,
                           "path": str(candidate)}}

    def done(status, failures, **kw):
        c = _case(profile, pdigest, baseline, cdef, platform, status=status,
                  failures=failures, input_sha=input_sha, candidate_prov=cand, **kw)
        (case_dir / "case.json").write_text(json.dumps(c, indent=2, sort_keys=True),
                                            encoding="utf-8")
        return c

    # 1. inputs: the reference must be the pinned image, every pinned input
    #    must be the baseline's bytes, and the archives must be the baseline's.
    unavailable = [{"kind": "archive", "detail": p} for p in archive_problems]
    unavailable += check_inputs(cdef, assets)
    ref = REPO_ROOT / cdef["reference"]["path"]
    if not ref.is_file():
        unavailable.append({"kind": "reference_missing", "path": str(ref)})
    elif sha256_file(ref) != cdef["reference"]["sha256"]:
        unavailable.append({"kind": "reference_hash", "path": str(ref),
                            "expected": cdef["reference"]["sha256"],
                            "actual": sha256_file(ref)})
    if not Path(candidate).is_file():
        unavailable.append({"kind": "candidate_missing", "path": str(candidate)})
    if unavailable:
        return done("unavailable", unavailable)

    # 2. private runtime with an owned configuration
    runtime = case_dir / "runtime"
    problems = build_runtime(Path(candidate), profile, game_dir, runtime)
    if problems:
        return done("unavailable", [{"kind": "runtime", "detail": p} for p in problems])

    # 3. render `repeat` times; every run must succeed and agree byte-for-byte
    pngs, reads_all = [], None
    for n in range(profile["render"]["repeat"]):
        out = case_dir / f"render{n}.bmp"
        res = runner(runtime, cdef, out)
        for stream in ("stdout", "stderr"):
            (case_dir / f"render{n}.{stream}.txt").write_text(
                getattr(res, stream, "") or "", encoding="utf-8")
        if not getattr(res, "ok", False):
            return done("mismatch", [{"kind": "process", "stage": "process",
                                      "detail": getattr(res, "reason", "failed"),
                                      "returncode": getattr(res, "returncode", None)}])
        reads = parse_resource_reads(res.stdout)
        if reads_all is None:
            reads_all = reads
            (case_dir / "resources.json").write_text(json.dumps(reads, indent=1),
                                                     encoding="utf-8")
            read_fails = check_reads(reads, res.stdout, cdef, game_dir, runtime)
            if read_fails:
                return done("mismatch", read_fails,
                            coverage={"resources_read": len(reads)})
        pngs.append(_png_from_render(out))
    first = _rgb_image(pngs[0])
    for p in pngs[1:]:
        if _pixel_sha(_rgb_image(p)) != _pixel_sha(first):
            return done("mismatch", [{"kind": "nondeterministic",
                                      "stage": "determinism",
                                      "detail": f"{pngs[0].name} != {p.name}"}])

    # 4. compare with the reference
    shutil.copyfile(ref, case_dir / ("expected" + ref.suffix))
    cmp = compare_rgb(ref, pngs[0])
    (case_dir / "compare.json").write_text(json.dumps(cmp, indent=1), encoding="utf-8")
    cov = {"resources_read": len(reads_all), "pinned_inputs": len(cdef["inputs"]),
           "renders": len(pngs), "features": list(cdef["features"])}
    if cmp["equal"]:
        return done("pass", [], expected_sha=cmp["expected_sha256"],
                    actual_sha=cmp["actual_sha256"], coverage=cov)
    return done("mismatch", [{"kind": "value", "stage": "final_rgb",
                              "path": "image",
                              "expected": cmp["expected_sha256"],
                              "actual": cmp["actual_sha256"],
                              "detail": {k: cmp.get(k) for k in (
                                  "reason", "diff_pixels", "total_pixels",
                                  "max_channel_diff", "bbox", "expected_size",
                                  "actual_size")}}],
                expected_sha=cmp["expected_sha256"],
                actual_sha=cmp["actual_sha256"], coverage=cov)


def aggregate_case(profile, pdigest, baseline, results, platform,
                   external_results: Optional[list] = None) -> dict:
    """Pass only when every required feature has an executed passing case."""
    passing = {r["id"] for r in results if r["status"] == "pass"}
    executed = {r["id"] for r in results if r["status"] not in ("not_run", "unavailable")}
    ext_pass = {r["id"] for r in (external_results or [])
                if r.get("status") == r.get("success_outcome") and r.get("status")}
    by_feature = {f: [] for f in profile["required_features"]}
    for c in profile["cases"]:
        for f in c["features"]:
            by_feature.setdefault(f, []).append(c["id"])
    for f, ids in profile.get("external_features", {}).items():
        by_feature.setdefault(f, []).extend(ids)
    failures, covered = [], {}
    for f in profile["required_features"]:
        ids = by_feature.get(f, [])
        ok = [i for i in ids if i in passing or i in ext_pass]
        covered[f] = ok
        if not ids:
            failures.append({"kind": "missing_feature_coverage", "path": f,
                             "expected": "a case", "actual": None})
        elif not ok:
            failures.append({"kind": "feature_not_passed", "path": f,
                             "expected": ids, "actual": None})
    if not executed:
        status = "not_run"
    elif any(r["status"] == "mismatch" for r in results):
        status = "mismatch"
    elif failures or any(r["status"] != "pass" for r in results):
        status = "partial"
    else:
        status = "pass"
    return {
        "schema": C.CASE_SCHEMA, "id": profile["aggregate_case"], "lane": LANE,
        "platform": platform, "required": True, "status": status,
        "success_outcome": "pass", "contract_digest": pdigest,
        "baseline_digest": baseline.get("digest"),
        "input_sha256": C.payload_sha256(sorted(c["id"] for c in profile["cases"])),
        "expected_sha256": None,
        "actual_sha256": C.payload_sha256(sorted(
            [r["id"], r["status"]] for r in results)),
        "coverage": {"features": covered,
                     "cases_selected": len(profile["cases"]),
                     "cases_executed": len(executed),
                     "cases_passed": len(passing)},
        "failures": failures + [{"kind": "case_" + r["status"], "path": r["id"]}
                                for r in results if r["status"] != "pass"],
        "provenance": {"candidate": {}, "reference": {"profile": profile["id"]}},
        "limitations": list(profile.get("limitations", [])),
    }


def render_cases(profile: dict, baseline: dict, candidate: Path, out_dir: Path,
                 *, game_dir: Optional[Path] = None,
                 runner: Optional[Runner] = None,
                 platform: str = DEFAULT_PLATFORM,
                 external_results: Optional[list] = None,
                 only: Optional[list] = None) -> list[dict]:
    """Render and compare every profile case; returns ds1edit.case/1 results
    (one per map, then the aggregate). Raises ProfileError / ValidationError
    for an invalid profile or baseline."""
    validate_profile(profile)
    C.validate_baseline(baseline)
    if baseline["digest"] != profile["baseline"]["digest"]:
        raise ProfileError("baseline digest is not the one the profile pins")
    render = baseline.get("render", {})
    if render.get("mod_directories") not in (None, "none"):
        raise ProfileError("baseline was recorded with mod directories")
    if game_dir is None:
        env = os.environ.get("DS1EDIT_GAME_DIR_113C")
        game_dir = Path(env) if env else None
    candidate = Path(candidate)
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    pdigest = profile_digest(profile)
    assets = {a["path"]: a for a in baseline["assets"]}
    if runner is None:
        exe_name, zoom = candidate.name, profile["render"]["zoom"]

        def runner(rt, c, out):  # noqa: E306
            return default_runner(rt, c, out, exe_name=exe_name, zoom=zoom)
    archive_problems = check_archives(game_dir, baseline, {})
    results = []
    for cdef in profile["cases"]:
        if only and cdef["id"] not in only:
            results.append(_case(profile, pdigest, baseline, cdef, platform,
                                 status="not_run",
                                 failures=[{"kind": "not_run", "detail": "not selected"}],
                                 input_sha=_input_payload(profile, cdef)))
            continue
        try:
            results.append(run_case(profile, pdigest, baseline, assets, cdef,
                                    candidate, game_dir, out_dir, platform,
                                    runner, archive_problems))
        except Exception as e:  # a crash in one case never passes it
            results.append(_case(profile, pdigest, baseline, cdef, platform,
                                 status="mismatch",
                                 failures=[{"kind": "internal", "stage": "process",
                                            "detail": f"{type(e).__name__}: {e}"}],
                                 input_sha=_input_payload(profile, cdef)))
    results.append(aggregate_case(profile, pdigest, baseline, results, platform,
                                  external_results))
    (out_dir / "results.json").write_text(json.dumps(results, indent=2, sort_keys=True),
                                          encoding="utf-8")
    return results


def exit_code(results: list[dict]) -> int:
    req = [r for r in results if r.get("required")]
    if req and all(r["status"] == "pass" for r in req):
        return 0
    if any(r["status"] == "mismatch" for r in req):
        return 1
    return 2


# ---------------------------------------------------------------------------
# pinning helper: list what a render read, with baseline hashes
# ---------------------------------------------------------------------------

def pin_inputs(stdout_text: str, baseline: dict) -> list[dict]:
    """Inputs for a profile case from one trusted render's log. Every read
    must exist in the baseline; editor-local files are excluded."""
    assets = {a["path"]: a for a in baseline["assets"]}
    out, seen = [], set()
    for r in parse_resource_reads(stdout_text):
        if r["source"].strip() == ".":
            continue
        key = baseline_asset_key(r["name"])
        if key in seen:
            continue
        seen.add(key)
        if key not in assets:
            raise ProfileError(f"{key} is not in the baseline")
        out.append({"path": key, "sha256": assets[key]["sha256"]})
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="render_cases")
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--profile", required=True, type=Path)
    r.add_argument("--baseline", required=True, type=Path)
    r.add_argument("--candidate", required=True, type=Path)
    r.add_argument("--game-dir", type=Path)
    r.add_argument("--out-dir", required=True, type=Path)
    r.add_argument("--platform", default=DEFAULT_PLATFORM)
    r.add_argument("--only", action="append")
    v = sub.add_parser("validate")
    v.add_argument("--profile", required=True, type=Path)
    a = ap.parse_args(argv)
    try:
        profile = json.loads(Path(a.profile).read_text(encoding="utf-8"))
        validate_profile(profile)
        if a.cmd == "validate":
            print(f"{profile['id']}: {len(profile['cases'])} cases, valid")
            return 0
        baseline = C.load_baseline(a.baseline)
        results = render_cases(profile, baseline, a.candidate, a.out_dir,
                               game_dir=a.game_dir, platform=a.platform,
                               only=a.only)
    except (ProfileError, C.ValidationError, OSError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    for res in results:
        extra = ""
        if res["failures"]:
            f = res["failures"][0]
            extra = f"  [{f.get('kind')}] {f.get('path', '')} {f.get('detail', '')}"
        print(f"{res['status']:12s} {res['id']}{extra}")
    print(f"results: {a.out_dir / 'results.json'}")
    return exit_code(results)


if __name__ == "__main__":
    sys.exit(main())
