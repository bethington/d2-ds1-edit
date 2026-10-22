"""
Run golden screenshot comparison tests.

Renders maps using the current build's --headless mode and compares
against golden reference PNGs.

Usage:
    python run_golden_tests.py [--core|--full|MAP ...] [--tolerance N] [--update]
                               [--exe PATH] [--assets-dir PATH]
                               [--golden-dir PATH] [--artifacts-dir PATH]

Exit status: 0 only when at least one map was selected and every selected map
passed; 1 when any selected map failed or was skipped (a missing reference or
INI counts); 2 for an invalid command line or configuration, detected before
any renderer is launched.

--update rewrites the reference images from the current build. Only do that
once you have confirmed the new output is right -- it blesses whatever the
build currently produces.

Default tolerance is 0 (exact match). Use --tolerance to allow for
animation non-determinism (e.g., --tolerance 255 ignores color diffs).
"""

import argparse
import contextlib
import json
import os
import re
import shutil
import sys
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow")
    sys.exit(2)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
BIN_DIR = os.path.join(PROJECT_ROOT, "bin")
ASSETS_DIR = os.path.join(BIN_DIR, "assets")
GOLDEN_DIR = os.path.join(PROJECT_ROOT, "test", "golden")
ARTIFACTS_DIR = os.path.join(PROJECT_ROOT, "tmp", "golden-runs")
EXE = os.path.join(BIN_DIR, "ds1edit.exe" if os.name == "nt" else "ds1edit")

# Import comparison function
sys.path.insert(0, SCRIPT_DIR)
from compare_golden import compare_images

CORE_MAPS = [
    "act1_town",
    "act1_cave",
    "act2_desert",
    "act3_jungle",
    "act4_fortress",
    "act5_town",
]


class IniError(ValueError):
    """A map INI that cannot name a render."""


_ID_RE = re.compile(r"[0-9]+")


def parse_ini_entry(ini_path):
    """Return (lvltype_id, lvlprest_def, ds1_path) from a map INI's first entry.

    The first non-blank, non-comment line must be two non-negative integer IDs
    followed by a DS1 path. The path is the rest of the line, so it may contain
    spaces; its separators are passed through unchanged. Raises IniError.
    """
    with open(ini_path, "r", encoding="utf-8", errors="replace") as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#") or line.startswith(";"):
                continue
            parts = line.split(None, 2)
            if len(parts) < 3:
                raise IniError(f"invalid INI line {lineno}: expected "
                               f"'<lvltype id> <lvlprest def> <ds1 path>'")
            lvltype_id, lvlprest_def, ds1_path = parts
            if not (_ID_RE.fullmatch(lvltype_id) and
                    _ID_RE.fullmatch(lvlprest_def)):
                raise IniError(f"invalid INI line {lineno}: IDs must be "
                               f"non-negative integers, got {lvltype_id!r} "
                               f"{lvlprest_def!r}")
            return lvltype_id, lvlprest_def, ds1_path.strip()
    raise IniError("INI has no entries")


def parse_ini_first_entry(ini_path):
    """Compatibility wrapper: the first entry, or None if there is none."""
    try:
        return parse_ini_entry(ini_path)
    except (IniError, OSError):
        return None


def get_all_ini_names(assets_dir=None):
    assets_dir = ASSETS_DIR if assets_dir is None else assets_dir
    names = []
    for f in sorted(os.listdir(assets_dir)):
        if f.endswith(".ini"):
            names.append(f[:-4])
    return names


# Pinned so a reference means the same thing on every machine. Without it the
# render follows default_zoom from the local ds1edit.ini and the comparison
# fails on a size mismatch. 1:4 keeps the reference images small while still
# making a palette or tile regression obvious.
GOLDEN_ZOOM = "1:4"
RENDER_TIMEOUT_S = 120
SCHEMA = "ds1edit.golden/1"


@dataclass
class RenderResult:
    """Outcome of one headless render. ok is True only for exit status 0 plus
    a fresh, decodable image at the requested output path."""
    ok: bool
    reason: str
    returncode: Optional[int]
    stdout: str
    stderr: str


def _text(data):
    """Child output as text; TimeoutExpired carries bytes even with text=True."""
    if data is None:
        return ""
    if isinstance(data, bytes):
        return data.decode("utf-8", errors="replace")
    return data


def render_case(exe, cwd, ids, ds1_path, output, timeout_s=RENDER_TIMEOUT_S):
    """Render one map headlessly and verify the result.

    exe and output may contain spaces: the command is an argument list run
    without a shell. output must not exist yet -- an existing file could be
    left over from an earlier run, so it is refused rather than deleted, and
    the renderer is not launched. Callers should give each case its own fresh
    directory.
    """
    exe = Path(exe)
    output = Path(output)
    lvltype_id, lvlprest_def = ids

    if os.path.lexists(output):
        return RenderResult(False, f"output path already exists before launch: "
                                   f"{output}", None, "", "")
    if not exe.is_file():
        return RenderResult(False, f"executable not found: {exe}", None, "", "")

    cmd = [str(exe), ds1_path, str(lvltype_id), str(lvlprest_def),
           "--zoom=" + GOLDEN_ZOOM, "--headless", str(output)]
    try:
        proc = subprocess.run(cmd, cwd=str(cwd), capture_output=True,
                              text=True, encoding="utf-8", errors="replace",
                              timeout=timeout_s, shell=False)
    except subprocess.TimeoutExpired as e:
        return RenderResult(False, f"renderer timed out after {timeout_s}s",
                            None, _text(e.stdout), _text(e.stderr))
    except OSError as e:
        return RenderResult(False, f"could not launch renderer {exe}: {e}",
                            None, "", "")

    rc = proc.returncode
    out, err = _text(proc.stdout), _text(proc.stderr)
    if rc != 0:
        return RenderResult(False, f"renderer exited with status {rc}",
                            rc, out, err)
    if not output.exists():
        return RenderResult(False, "renderer exited 0 but wrote no output",
                            rc, out, err)
    if output.stat().st_size == 0:
        return RenderResult(False, "renderer wrote an empty output file",
                            rc, out, err)
    try:
        with Image.open(output) as img:
            img.verify()
        with Image.open(output) as img:   # verify() leaves the image unusable
            img.load()
    except Exception as e:
        return RenderResult(False, f"invalid image output: "
                                   f"{type(e).__name__}: {e}", rc, out, err)
    return RenderResult(True, "rendered", rc, out, err)


def render_headless(lvltype_id, lvlprest_def, ds1_path, output_path):
    """Compatibility wrapper: render with the default executable from bin/.
    Returns True only when render_case() succeeds."""
    return render_case(Path(EXE), Path(BIN_DIR), (lvltype_id, lvlprest_def),
                       ds1_path, Path(output_path)).ok


def bmp_to_png_path(bmp_path):
    """Convert a BMP to a PNG next to it for comparison."""
    img = Image.open(bmp_path)
    if img.mode == "P":
        img = img.convert("RGB")
    png_path = os.path.splitext(bmp_path)[0] + ".png"
    img.save(png_path, "PNG")
    return png_path


def _tolerance(text):
    try:
        value = int(text, 10)
    except ValueError:
        value = None
    if value is None or not 0 <= value <= 255:
        raise argparse.ArgumentTypeError(
            f"must be an integer from 0 to 255, got {text!r}")
    return value


def parse_args(argv=None):
    """Parse the command line. Invalid arguments raise SystemExit(2)."""
    parser = argparse.ArgumentParser(
        prog="run_golden_tests.py",
        description="Render maps headlessly and compare them with golden "
                    "reference PNGs. Exit 0 only when every selected map "
                    "passed; 1 when any failed or was skipped; 2 for an "
                    "invalid command line or configuration.")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--core", dest="mode", action="store_const", const="core",
                      help="the six core maps (default)")
    mode.add_argument("--full", dest="mode", action="store_const", const="full",
                      help="every <name>.ini in the assets directory")
    parser.add_argument("maps", nargs="*", metavar="MAP",
                        help="explicit map names (instead of --core/--full)")
    parser.add_argument("--tolerance", type=_tolerance, default=0, metavar="N",
                        help="max per-channel difference, 0-255 (default 0)")
    parser.add_argument("--update", action="store_true",
                        help="replace references with the current renders")
    parser.add_argument("--exe", default=EXE,
                        help="renderer executable; it runs in its own "
                             "directory (default: bin/ds1edit[.exe])")
    parser.add_argument("--assets-dir", default=ASSETS_DIR,
                        help="directory of <map>.ini files "
                             "(default: bin/assets)")
    parser.add_argument("--golden-dir", default=GOLDEN_DIR,
                        help="reference images (default: test/golden)")
    parser.add_argument("--artifacts-dir", default=ARTIFACTS_DIR,
                        help="each run gets a new subdirectory here "
                             "(default: tmp/golden-runs)")
    parser.add_argument("--allow-mod", action="store_true",
                        help="do not fail a case whose renderer log shows "
                             "Project Diablo 2 archives in use")
    parser.set_defaults(timeout=RENDER_TIMEOUT_S)
    options = parser.parse_args(argv)
    if options.maps and options.mode:
        parser.error("map names cannot be combined with --core or --full")
    return options


def configuration_errors(options):
    """Problems that make the suite unrunnable; checked before any launch."""
    errors = []
    if not os.path.isfile(options.exe):
        errors.append(f"executable not found: {options.exe}")
    if not os.path.isdir(options.assets_dir):
        errors.append(f"assets directory not found: {options.assets_dir}")
    if not os.path.isdir(options.golden_dir):
        errors.append(f"golden directory not found: {options.golden_dir}")
    return errors


def select_maps(options):
    """The requested map names, de-duplicated in first-seen order."""
    if options.maps:
        names = options.maps
    elif options.mode == "full":
        names = get_all_ini_names(options.assets_dir)
    else:
        names = CORE_MAPS
    return list(dict.fromkeys(names))


def _valid_map_name(name):
    return (name not in ("", ".", "..") and "/" not in name
            and "\\" not in name and os.path.basename(name) == name)


def _new_run_dir(artifacts_dir):
    """A fresh directory owned by this run; never reuses an earlier one."""
    artifacts_dir = Path(artifacts_dir)
    artifacts_dir.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    return Path(tempfile.mkdtemp(prefix=f"run-{stamp}-", dir=artifacts_dir))


def mod_data_evidence(*streams):
    """The first renderer log line showing Project Diablo 2 archives in use,
    or None. Matches the d2install summary line and any slot line naming a
    ProjectD2 folder; the "found at ... not used" line does not match."""
    for text in streams:
        for line in (text or "").splitlines():
            low = line.lower()
            if "project diablo 2 data from" in low:
                return line.strip()
            if "slot " in low and "projectd2" in low:
                return line.strip()
    return None


def _run_one(name, case_dir, exe, assets_dir, golden_dir, tolerance, update,
             timeout_s, allow_mod=False):
    """Run a single case. Returns (status, reason, artifacts)."""
    artifacts = {}
    golden_png = golden_dir / f"{name}.png"
    golden_bmp = golden_dir / f"{name}.bmp"
    if golden_png.exists():
        reference = golden_png
    elif golden_bmp.exists():
        reference = golden_bmp
    else:
        reference = None
    if reference is None and not update:
        return "skipped", f"no golden reference in {golden_dir}", artifacts

    ini_path = assets_dir / f"{name}.ini"
    if not ini_path.is_file():
        return "skipped", f"no INI file {ini_path}", artifacts
    try:
        lvltype_id, lvlprest_def, ds1_path = parse_ini_entry(ini_path)
    except IniError as e:
        return "failed", f"{e} ({ini_path})", artifacts

    if not update:
        try:
            with Image.open(reference) as img:
                img.load()
        except Exception as e:
            return ("failed", f"unreadable golden reference {reference}: "
                              f"{type(e).__name__}: {e}", artifacts)

    case_dir.mkdir(parents=True)
    actual_bmp = case_dir / "actual.bmp"
    result = render_case(exe, exe.parent, (lvltype_id, lvlprest_def),
                         ds1_path, actual_bmp, timeout_s)
    if result.returncode is not None or result.stdout or result.stderr:
        for stream in ("stdout", "stderr"):
            log = case_dir / f"{stream}.txt"
            log.write_text(getattr(result, stream), encoding="utf-8")
            artifacts[stream] = str(log)
    if not result.ok:
        if actual_bmp.exists():
            artifacts["actual"] = str(actual_bmp)   # whatever it wrote
        return "failed", result.reason, artifacts
    if not allow_mod:
        evidence = mod_data_evidence(result.stdout, result.stderr)
        if evidence:
            return ("failed", f"renderer used Project Diablo 2 data, not "
                              f"vanilla ({evidence}); set use_pd2 = no in the "
                              f"renderer's ini or pass --allow-mod", artifacts)

    actual_png = Path(bmp_to_png_path(str(actual_bmp)))
    actual_bmp.unlink()
    artifacts["actual"] = str(actual_png)

    if update:
        if reference is not None:
            previous = case_dir / f"previous{reference.suffix}"
            shutil.copyfile(reference, previous)
            artifacts["previous"] = str(previous)
        try:
            size = _replace_reference(actual_png, golden_png)
        except Exception as e:
            return ("failed", f"update failed, reference unchanged: "
                              f"{type(e).__name__}: {e}", artifacts)
        return "updated", f"wrote {golden_png} ({size[0]}x{size[1]})", artifacts

    match, stats = compare_images(str(reference), str(actual_png), tolerance)
    if match:
        return ("passed", f"{stats['total_pixels']:,} px, "
                          f"max diff={stats['max_channel_diff']}", artifacts)
    expected = case_dir / f"expected{reference.suffix}"
    shutil.copyfile(reference, expected)
    artifacts["expected"] = str(expected)
    if "error" in stats:
        return "failed", stats["error"], artifacts
    return ("failed", f"{stats['diff_pixels']:,}/{stats['total_pixels']:,} "
                      f"pixels differ ({stats['diff_pct']:.2f}%), "
                      f"max diff={stats['max_channel_diff']}", artifacts)


def _replace_reference(actual_png, golden_png):
    """Write the new reference beside the old one, check it decodes, then
    swap it in with one os.replace(). Any failure leaves the old reference
    untouched and removes the temporary file. Returns the image size."""
    with Image.open(actual_png) as img:
        size = img.size
        rgb = img.convert("RGB")
    # Store as an indexed PNG when the render fits in 256 colours, which is
    # lossless for these and keeps the repo small.
    if rgb.getcolors(maxcolors=256):
        rgb = rgb.convert("P", palette=Image.ADAPTIVE, colors=256)
    fd, tmp = tempfile.mkstemp(prefix=f".{golden_png.stem}.", suffix=".png.tmp",
                               dir=golden_png.parent)
    os.close(fd)
    try:
        rgb.save(tmp, "PNG", optimize=True)
        with Image.open(tmp) as check:
            check.load()
            if check.size != size:
                raise ValueError(f"rewritten image is {check.size}, "
                                 f"expected {size}")
        os.replace(tmp, golden_png)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)
    return size


def _write_results(path, summary):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
        f.write("\n")


_LABELS = {"passed": "PASS", "failed": "FAIL", "skipped": "SKIP",
           "updated": "UPDATED"}


def run_suite(names, options):
    """Run every named case and return the result summary.

    Never stops early: an exception in one case is recorded as that case's
    failure and the next case still runs. options needs exe, assets_dir and
    golden_dir; tolerance, update, timeout and artifacts_dir are optional.
    Without artifacts_dir, work happens in a temporary directory that is
    removed afterwards and no artifact paths are reported.
    """
    names = list(dict.fromkeys(names))
    if not names:
        raise ValueError("no maps selected")
    exe = Path(os.path.abspath(options.exe))
    assets_dir = Path(options.assets_dir)
    golden_dir = Path(options.golden_dir)
    tolerance = getattr(options, "tolerance", 0)
    update = getattr(options, "update", False)
    timeout_s = getattr(options, "timeout", RENDER_TIMEOUT_S)
    artifacts_dir = getattr(options, "artifacts_dir", None)
    allow_mod = getattr(options, "allow_mod", False)

    with contextlib.ExitStack() as stack:
        if artifacts_dir:
            run_dir = _new_run_dir(artifacts_dir)
        else:
            run_dir = Path(stack.enter_context(
                tempfile.TemporaryDirectory(prefix="golden-run-")))
        cases = []
        for name in names:
            case_dir = run_dir / "cases" / name
            try:
                status, reason, artifacts = _run_one(
                    name, case_dir, exe, assets_dir, golden_dir, tolerance,
                    update, timeout_s, allow_mod)
            except Exception as e:
                status, reason, artifacts = (
                    "failed", f"internal error: {type(e).__name__}: {e}", {})
            if not artifacts_dir:
                artifacts = {}
            print(f"  {_LABELS[status]} {name}: {reason}")
            cases.append({"id": name, "status": status, "reason": reason,
                          "artifacts": artifacts})

        counts = {k: sum(1 for c in cases if c["status"] == k)
                  for k in ("passed", "failed", "skipped", "updated")}
        good = counts["updated"] if update else counts["passed"]
        summary = {
            "schema": SCHEMA,
            "mode": "update" if update else "compare",
            "tolerance": tolerance,
            "zoom": GOLDEN_ZOOM,
            "selected": len(cases),
            **counts,
            "ok": len(cases) > 0 and good == len(cases),
            "run_dir": str(run_dir) if artifacts_dir else None,
            "cases": cases,
        }
        if artifacts_dir:
            try:
                _write_results(run_dir / "results.json", summary)
            except OSError as e:
                # Unrecorded results are not a pass, whatever the cases did.
                summary["ok"] = False
                summary["results_error"] = str(e)
                print(f"ERROR: could not write {run_dir / 'results.json'}: {e}",
                      file=sys.stderr)
    return summary


def main(argv=None):
    try:
        options = parse_args(argv)
    except SystemExit as e:
        return e.code if isinstance(e.code, int) else 2

    errors = configuration_errors(options)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 2
    names = select_maps(options)
    if not names:
        print(f"ERROR: no maps selected (no .ini files in "
              f"{options.assets_dir})", file=sys.stderr)
        return 2
    bad = [n for n in names if not _valid_map_name(n)]
    if bad:
        print(f"ERROR: invalid map names: {', '.join(bad)}", file=sys.stderr)
        return 2

    if options.maps:
        label = "selected"
    else:
        label = options.mode or "core"
    print(f"Golden screenshot tests ({label}): {len(names)} maps, "
          f"tolerance={options.tolerance}, zoom={GOLDEN_ZOOM}"
          f"{', UPDATING REFERENCES' if options.update else ''}")
    print()
    try:
        summary = run_suite(names, options)
    except OSError as e:
        print(f"ERROR: could not create a run directory under "
              f"{options.artifacts_dir}: {e}", file=sys.stderr)
        return 2

    print(f"\nResults: {summary['passed']} passed, {summary['failed']} failed, "
          f"{summary['skipped']} skipped"
          + (f", {summary['updated']} updated" if options.update else ""))
    if summary["run_dir"]:
        print(f"Artifacts: {summary['run_dir']}")
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
