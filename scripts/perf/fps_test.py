#!/usr/bin/env python3
"""Frame-rate test for the DS1Edit GUI.

Runs ``ds1edit --selftest=N --selftest-report=...`` on a matrix of maps x
graphics drivers x window sizes, each run in its own owned runtime directory
(exe + DLLs + assets, a generated Ds1edit.ini with full_screen = NO and an
allegro5.cfg naming the driver), and judges the JSON reports (schema
"ds1edit.fps/1").

Verdicts (never a pass when data is missing):

  FAIL  a run exited nonzero or timed out
  FAIL  a run produced no readable report
  FAIL  any memory-bitmap draw, or a memory font
  FAIL  median render avg > 16.7 ms at 1080p (cannot sustain 60 fps)
  FAIL  render avg regressed > 25 % against a reviewed baseline taken on the
        same host and build identity
  note  'incomparable' when the baseline host/build identity differs,
        'unreviewed' when the baseline has "reviewed": false

Windowed only. Error-mode dialogs are suppressed before any launch.

Examples
  python scripts/perf/fps_test.py --exe bin/ds1edit.exe
  python scripts/perf/fps_test.py --exe bin/ds1edit.exe --drivers opengl \\
      --maps act1_town --size 1920x1080 --runs 5 --profile-on-fail
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable, Optional

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

try:  # reuse the benchmark harness pieces when present
    from scripts.equivalence import benchmarks as _bm
except Exception:  # pragma: no cover - standalone copy of the script
    _bm = None

SCHEMA = "ds1edit.fps/1"
SUMMARY_SCHEMA = "ds1edit.fps-summary/1"
BASELINE_SCHEMA = "ds1edit.fps-baseline/1"

DEFAULT_GAME_DIR = r"F:\D2VersionChanger\VersionChanger\LoD\1.13c"
CORE_MAPS = ["act1_town", "act1_cave", "act2_desert", "act3_jungle",
             "act4_fortress", "act5_town"]
DEFAULT_SIZES = ["1280x720", "1920x1080"]
BUDGET_MS_1080P = 16.7
REGRESSION_LIMIT = 0.25
DEFAULT_BASELINE = ROOT / "test" / "equivalence" / "profiles" / "fps-baseline.json"

WPT_DIR = r"C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit"


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

def suppress_error_dialogs() -> None:
    if _bm is not None:
        _bm.suppress_error_dialogs()
    elif os.name == "nt":
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x8003)


def parse_size(text: str) -> tuple:
    m = re.fullmatch(r"(\d+)x(\d+)", text.strip().lower())
    if not m:
        raise ValueError(f"bad size {text!r}; expected WxH")
    return int(m.group(1)), int(m.group(2))


def median(values: list) -> Optional[float]:
    return statistics.median(values) if values else None


def case_key(map_name: str, driver: str, size: tuple) -> str:
    return f"{map_name}|{driver}|{size[0]}x{size[1]}"


def default_drivers() -> list:
    return ["direct3d", "opengl"] if os.name == "nt" else ["opengl"]


# ---------------------------------------------------------------------------
# Owned runtime
# ---------------------------------------------------------------------------

def _link_or_copy(src: Path, dst: Path) -> None:
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy2(src, dst)


def build_runtime(run_dir: Path, exe: Path, game_dir: str, driver: str,
                  size: tuple, assets_dir: Path) -> Path:
    """Create run_dir/rt with the exe, its shared libraries and assets."""
    rt = run_dir / "rt"
    if rt.exists():
        shutil.rmtree(rt)
    rt.mkdir(parents=True)
    exe_dir = exe.parent
    _link_or_copy(exe, rt / exe.name)
    for pattern in ("*.dll", "*.so*", "*.dylib"):
        for lib in exe_dir.glob(pattern):
            _link_or_copy(lib, rt / lib.name)
    if assets_dir.is_dir():
        shutil.copytree(assets_dir, rt / "assets")
    (rt / "Ds1edit.ini").write_text(
        f"d2_install = {game_dir}\nfull_screen = NO\n"
        f"screen_width = {size[0]}\nscreen_height = {size[1]}\n")
    (rt / "allegro5.cfg").write_text(f"[graphics]\ndriver={driver}\n")
    return rt


# ---------------------------------------------------------------------------
# Running one case
# ---------------------------------------------------------------------------

Runner = Callable[[list, Path, dict, float], tuple]


def default_runner(command: list, cwd: Path, env: dict, timeout_s: float) -> tuple:
    """-> (returncode | None on timeout, stdout, stderr)."""
    suppress_error_dialogs()
    kwargs = {"cwd": str(cwd), "stdin": subprocess.DEVNULL,
              "stdout": subprocess.PIPE, "stderr": subprocess.PIPE, "env": env}
    proc = subprocess.Popen(command, **kwargs)
    try:
        out, err = proc.communicate(timeout=timeout_s)
        return proc.returncode, out.decode(errors="replace"), err.decode(errors="replace")
    except subprocess.TimeoutExpired:
        if _bm is not None:
            _bm.kill_tree(proc)
        else:
            proc.kill()
        out, err = proc.communicate()
        return None, out.decode(errors="replace"), err.decode(errors="replace")


def selftest_command(exe_path: Path, map_name: str, frames: int, report: Path) -> list:
    return [str(Path(exe_path).resolve()), f"assets/{map_name}.ini",
            f"--selftest={frames}", f"--selftest-report={report}"]


def load_report(path: Path) -> tuple:
    """-> (report | None, problem | None)."""
    if not path.is_file():
        return None, "no report written"
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError) as exc:
        return None, f"unreadable report: {exc}"
    if data.get("schema") != SCHEMA:
        return None, f"unexpected report schema {data.get('schema')!r}"
    for key in ("render_ms", "present_ms", "frames", "window"):
        if key not in data:
            return None, f"report is missing {key!r}"
    return data, None


def run_once(run_dir: Path, rt: Path, exe_name: str, map_name: str, frames: int,
             timeout_s: float, runner: Runner) -> dict:
    report = run_dir.resolve() / "report.json"
    if report.exists():
        report.unlink()
    command = selftest_command(rt / exe_name, map_name, frames, report)
    t0 = time.time()
    rc, out, err = runner(command, rt, dict(os.environ), timeout_s)
    wall = time.time() - t0
    (run_dir / "stdout.txt").write_text(out or "")
    (run_dir / "stderr.txt").write_text(err or "")
    data, problem = load_report(report)
    res = {"run_dir": str(run_dir), "exit_code": rc, "timed_out": rc is None,
           "process_wall_s": round(wall, 3), "report": data, "problems": []}
    m = re.search(r"selftest: shutdown (\d+) ms", out or "")
    res["shutdown_ms"] = int(m.group(1)) if m else None
    if rc is None:
        res["problems"].append(f"timed out after {timeout_s:.0f} s")
    elif rc != 0:
        res["problems"].append(f"exit code {rc}")
    if problem:
        res["problems"].append(problem)
    if data:
        if data.get("memory_bitmap_draws", 0) > 0:
            res["problems"].append(
                f"{data['memory_bitmap_draws']} memory-bitmap draw(s)")
        if data.get("font_memory_bitmap"):
            res["problems"].append("font glyphs are memory bitmaps")
    return res


# ---------------------------------------------------------------------------
# Judging
# ---------------------------------------------------------------------------

def summarize_case(runs: list) -> dict:
    reports = [r["report"] for r in runs if r["report"]]
    def med(fn):
        vals = [fn(r) for r in reports]
        return median([v for v in vals if v is not None])
    out = {
        "runs": len(runs),
        "reports": len(reports),
        "render_avg_ms": med(lambda r: r["render_ms"]["avg"]),
        "render_p95_ms": med(lambda r: r["render_ms"]["p95"]),
        "render_p99_ms": med(lambda r: r["render_ms"]["p99"]),
        "render_max_ms": max([r["render_ms"]["max"] for r in reports], default=None),
        "present_avg_ms": med(lambda r: r["present_ms"]["avg"]),
        "wall_fps": med(lambda r: r.get("wall_fps")),
        "shutdown_ms": median([r["shutdown_ms"] for r in runs
                               if r.get("shutdown_ms") is not None]),
        "window": reports[0]["window"] if reports else None,
        "driver_string": reports[0].get("driver") if reports else None,
        "build": reports[0].get("build") if reports else None,
    }
    slow = [r.get("slowest_section") for r in reports if r.get("slowest_section")]
    out["slowest_section"] = slow[0]["name"] if slow else None
    return out


def judge_case(map_name: str, driver: str, size: tuple, runs: list) -> dict:
    s = summarize_case(runs)
    problems = []
    for i, r in enumerate(runs):
        for p in r["problems"]:
            problems.append(f"run {i + 1}: {p}")
    if s["reports"] == 0 and not problems:
        problems.append("no report from any run")
    height = (s["window"] or {}).get("height", size[1])
    if (size[1] >= 1080 and height >= 1080 and s["render_avg_ms"] is not None
            and s["render_avg_ms"] > BUDGET_MS_1080P):
        problems.append(
            f"median render avg {s['render_avg_ms']:.2f} ms > "
            f"{BUDGET_MS_1080P} ms at 1080p (cannot sustain 60 fps)")
    return {"key": case_key(map_name, driver, size), "map": map_name,
            "driver": driver, "size": f"{size[0]}x{size[1]}",
            "summary": s, "problems": problems,
            "status": "FAIL" if problems else "PASS",
            "baseline": "none"}


def host_summary(host: dict) -> dict:
    return {k: host.get(k) for k in ("host_class", "gpu", "cpu", "os_build", "node")}


def compare_baseline(cases: list, baseline: Optional[dict], host: dict) -> dict:
    """Annotate each case with baseline status; returns the comparison header."""
    if baseline is None:
        for c in cases:
            c["baseline"] = "none"
        return {"present": False}
    head = {"present": True, "reviewed": bool(baseline.get("reviewed")),
            "host": baseline.get("host")}
    bh = baseline.get("host") or {}
    same_host = (bh.get("host_class") == host.get("host_class")
                 and bh.get("gpu") == host.get("gpu"))
    for c in cases:
        ref = (baseline.get("cases") or {}).get(c["key"])
        s = c["summary"]
        if ref is None:
            c["baseline"] = "no-entry"
            continue
        if not same_host or ref.get("build") != s.get("build"):
            c["baseline"] = "incomparable"
            continue
        if not baseline.get("reviewed"):
            c["baseline"] = "unreviewed"
            continue
        cur, old = s.get("render_avg_ms"), ref.get("render_avg_ms")
        if cur is None or not old:
            c["baseline"] = "incomparable"
            continue
        change = (cur - old) / old
        c["baseline_change"] = round(change, 4)
        if change > REGRESSION_LIMIT:
            c["baseline"] = "regressed"
            c["problems"].append(
                f"render avg {cur:.2f} ms is {change * 100:.0f}% over the reviewed "
                f"baseline {old:.2f} ms")
            c["status"] = "FAIL"
        else:
            c["baseline"] = "ok"
    return head


def format_table(cases: list) -> str:
    cols = ("case", "render avg", "p95", "p99", "present", "wall fps",
            "exit ms", "slowest", "baseline", "status")
    rows = []
    for c in cases:
        s = c["summary"]
        def f(v, spec="{:.2f}"):
            return "-" if v is None else spec.format(v)
        rows.append((c["key"], f(s["render_avg_ms"]), f(s["render_p95_ms"]),
                     f(s["render_p99_ms"]), f(s["present_avg_ms"]),
                     f(s["wall_fps"], "{:.1f}"), f(s["shutdown_ms"], "{:.0f}"),
                     s["slowest_section"] or "-", c["baseline"], c["status"]))
    widths = [max(len(str(x)) for x in col) for col in zip(cols, *rows)]
    line = lambda r: "  ".join(str(x).ljust(w) for x, w in zip(r, widths))
    out = [line(cols), line(["-" * w for w in widths])]
    out += [line(r) for r in rows]
    for c in cases:
        for p in c["problems"]:
            out.append(f"  ! {c['key']}: {p}")
    return "\n".join(out)


# ---------------------------------------------------------------------------
# Profiling (Windows Performance Recorder / perf)
# ---------------------------------------------------------------------------

def _find_tool(name: str, which: Callable = shutil.which) -> Optional[str]:
    found = which(name)
    if found:
        return found
    if os.name == "nt":
        cand = Path(WPT_DIR) / f"{name}.exe"
        if cand.is_file():
            return str(cand)
    return None


def _tail(text: str, n: int = 400) -> str:
    text = (text or "").strip()
    return text if len(text) <= n else text[-n:]


def top_hotspots_from_text(text: str, limit: int = 20) -> list:
    """Rank 'module!function' (xperf) or 'symbol' lines (perf report --stdio)."""
    counts: dict = {}
    for m in re.finditer(r"\b([A-Za-z0-9_.\-]+\.(?:exe|dll))!([^\s,;\"']+)", text):
        key = f"{m.group(1)}!{m.group(2)}"
        counts[key] = counts.get(key, 0) + 1
    if counts:
        return sorted(counts.items(), key=lambda kv: -kv[1])[:limit]
    rows = []
    for line in text.splitlines():  # perf report: "  12.34%  prog  lib  [.] sym"
        m = re.match(r"\s*(\d+\.\d+)%\s+.*?\[[.k]\]\s+(\S.*)$", line)
        if m:
            rows.append((m.group(2).strip(), float(m.group(1))))
    return rows[:limit]


def write_hotspots(path: Path, header: str, rows: list, unit: str) -> None:
    lines = [header, ""]
    for i, (name, value) in enumerate(rows, 1):
        lines.append(f"{i:2d}. {value:>10}  {unit}  {name}")
    if not rows:
        lines.append("(no symbolised samples could be extracted; open trace.etl in wpa.exe)")
    path.write_text("\n".join(lines) + "\n")


def profile_case(command: list, cwd: Path, run_dir: Path, env: dict,
                 timeout_s: float, symbols_dir: Optional[str] = None,
                 which: Callable = shutil.which,
                 run: Callable = subprocess.run) -> dict:
    """Rerun one case under a profiler. Never raises, never prompts.

    -> {"status": "ok"|"unavailable"|"failed", "message", "trace", "hotspots"}
    """
    result = {"status": "unavailable", "message": "", "trace": None, "hotspots": None}
    env = dict(env)
    if symbols_dir:
        env["_NT_SYMBOL_PATH"] = (f"srv*;{symbols_dir}" if os.name == "nt" else symbols_dir)
    hotspots = run_dir / "hotspots.txt"

    def go(cmd, timeout=timeout_s + 120):
        return run(cmd, cwd=str(cwd), env=env, capture_output=True, text=True,
                   timeout=timeout, stdin=subprocess.DEVNULL)

    try:
        if os.name == "nt":
            wpr = _find_tool("wpr", which)
            if not wpr:
                result["message"] = "wpr.exe not found (install the Windows Performance Toolkit)"
                return result
            start = go([wpr, "-start", "CPU", "-filemode"], 60)
            if start.returncode != 0:
                result["message"] = ("wpr refused to start: "
                                     + _tail(start.stdout + start.stderr)
                                     + " (it usually needs an elevated shell)")
                return result
            trace = run_dir / "trace.etl"
            try:
                go(command, timeout_s)
            finally:
                stop = go([wpr, "-stop", str(trace)], 300)
            if stop.returncode != 0:
                result.update(status="failed", message="wpr -stop failed: "
                              + _tail(stop.stdout + stop.stderr))
                try:
                    go([wpr, "-cancel"], 60)
                except Exception:
                    pass
                return result
            result["trace"] = str(trace)
            xperf = _find_tool("xperf", which)
            if not xperf:
                result.update(status="ok", message="trace recorded; xperf not found, "
                              "no hotspot summary (open trace.etl in wpa.exe)")
                write_hotspots(hotspots, "no xperf: no summary", [], "samples")
                result["hotspots"] = str(hotspots)
                return result
            prof = go([xperf, "-i", str(trace), "-symbols", "-a", "profile", "-detail"], 900)
            rows = top_hotspots_from_text(prof.stdout or "")
            write_hotspots(hotspots, "Top hot functions (sample count, xperf -a profile -detail)",
                           rows, "samples")
            result.update(status="ok", hotspots=str(hotspots),
                          message="" if rows else "trace recorded; no symbolised rows found")
            return result
        perf = _find_tool("perf", which)
        if not perf:
            result["message"] = "perf not found"
            return result
        data = run_dir / "perf.data"
        rec = go([perf, "record", "-g", "-o", str(data), "--"] + command, timeout_s)
        if rec.returncode != 0 or not data.exists():
            result["message"] = "perf record failed: " + _tail(rec.stdout + rec.stderr)
            return result
        result["trace"] = str(data)
        rep = go([perf, "report", "--stdio", "--no-children", "-i", str(data)], 600)
        rows = top_hotspots_from_text(rep.stdout or "")
        write_hotspots(hotspots, "Top hot functions (percent of samples, perf report)",
                       rows, "%")
        result.update(status="ok", hotspots=str(hotspots))
        return result
    except (OSError, subprocess.SubprocessError) as exc:
        result.update(status="unavailable", message=f"profiler could not run: {exc}")
        return result


# ---------------------------------------------------------------------------
# Orchestration
# ---------------------------------------------------------------------------

def run_matrix(args, runner: Runner = default_runner, profiler=profile_case,
               host: Optional[dict] = None) -> tuple:
    exe = Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit(f"fps_test: --exe {exe} does not exist")
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = Path(args.out_dir) if args.out_dir else ROOT / "tmp" / "perf" / f"fps-{stamp}"
    out_dir.mkdir(parents=True, exist_ok=True)
    assets = Path(args.assets_dir) if args.assets_dir else (
        exe.parent / "assets" if (exe.parent / "assets").is_dir() else ROOT / "assets")
    sizes = [parse_size(s) for s in args.size.split(",")]
    maps = [m.strip() for m in args.maps.split(",") if m.strip()]
    drivers = [d.strip() for d in args.drivers.split(",") if d.strip()]
    if host is None:
        host = _bm.host_identity() if _bm else {"host_class": sys.platform}

    cases = []
    for map_name in maps:
        for driver in drivers:
            for size in sizes:
                runs = []
                for i in range(args.runs):
                    run_dir = out_dir / "runs" / f"{map_name}-{driver}-{size[0]}x{size[1]}-r{i + 1}"
                    run_dir.mkdir(parents=True, exist_ok=True)
                    rt = build_runtime(run_dir, exe, args.game_dir, driver, size, assets)
                    runs.append(run_once(run_dir, rt, exe.name, map_name, args.frames,
                                         args.timeout, runner))
                case = judge_case(map_name, driver, size, runs)
                case["runs_detail"] = [
                    {k: v for k, v in r.items() if k != "report"} for r in runs]
                case["_runs"] = runs
                cases.append(case)

    baseline = None
    bpath = Path(args.baseline) if args.baseline else DEFAULT_BASELINE
    if bpath.is_file():
        try:
            baseline = json.loads(bpath.read_text())
        except (OSError, ValueError):
            baseline = None
    base_head = compare_baseline(cases, baseline, host)

    for c in cases:
        want = args.profile or (args.profile_on_fail and c["status"] == "FAIL")
        c["profile"] = None
        if want:
            map_name, driver = c["map"], c["driver"]
            size = parse_size(c["size"])
            run_dir = out_dir / "profile" / f"{map_name}-{driver}-{c['size']}"
            run_dir.mkdir(parents=True, exist_ok=True)
            rt = build_runtime(run_dir, exe, args.game_dir, driver, size, assets)
            cmd = selftest_command(rt / exe.name, map_name, args.frames, run_dir / "report.json")
            c["profile"] = profiler(cmd, rt, run_dir, dict(os.environ), args.timeout,
                                    args.symbols_dir)
        c.pop("_runs", None)

    verdict = "PASS"
    if any(c["status"] == "FAIL" for c in cases) or not cases:
        verdict = "FAIL"
    summary = {"schema": SUMMARY_SCHEMA, "verdict": verdict,
               "created": datetime.datetime.now().isoformat(timespec="seconds"),
               "exe": str(exe), "frames": args.frames, "runs": args.runs,
               "budget_ms_1080p": BUDGET_MS_1080P, "host": host_summary(host),
               "baseline": base_head, "cases": cases}
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    table = format_table(cases)
    (out_dir / "summary.txt").write_text(table + f"\n\nverdict: {verdict}\n")
    return summary, table, out_dir


def write_baseline(path: Path, summary: dict) -> None:
    data = {"schema": BASELINE_SCHEMA, "reviewed": False,
            "note": "Recorded by fps_test.py --write-baseline. Set reviewed to true "
                    "only after a maintainer has checked these numbers; until then "
                    "comparisons report 'unreviewed' and never fail.",
            "created": summary["created"], "host": summary["host"],
            "frames": summary["frames"], "runs": summary["runs"], "cases": {}}
    for c in summary["cases"]:
        s = c["summary"]
        data["cases"][c["key"]] = {
            "build": s.get("build"), "render_avg_ms": s["render_avg_ms"],
            "render_p95_ms": s["render_p95_ms"], "wall_fps": s["wall_fps"],
            "present_avg_ms": s["present_avg_ms"]}
    path.write_text(json.dumps(data, indent=2) + "\n")


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--exe", required=True, help="ds1edit executable under test")
    p.add_argument("--game-dir", default=DEFAULT_GAME_DIR,
                   help="vanilla 1.13c install (default %(default)s)")
    p.add_argument("--maps", default=",".join(CORE_MAPS))
    p.add_argument("--drivers", default=",".join(default_drivers()))
    p.add_argument("--frames", type=int, default=120)
    p.add_argument("--runs", type=int, default=3)
    p.add_argument("--size", default=",".join(DEFAULT_SIZES),
                   help="WxH, comma separated (default %(default)s)")
    p.add_argument("--timeout", type=float, default=300.0, help="seconds per run")
    p.add_argument("--out-dir", help="default tmp/perf/fps-<timestamp>")
    p.add_argument("--assets-dir", help="default <exe dir>/assets, else <repo>/assets")
    p.add_argument("--baseline", help=f"default {DEFAULT_BASELINE}")
    p.add_argument("--write-baseline", metavar="PATH",
                   help="write this run as a baseline file (reviewed: false)")
    p.add_argument("--profile", action="store_true", help="profile every case")
    p.add_argument("--profile-on-fail", action="store_true",
                   help="rerun failing cases under WPR (Windows) or perf (Linux)")
    p.add_argument("--symbols-dir", help="PDB/debug-symbol directory (RelWithDebInfo build)")
    return p


def main(argv: Optional[list] = None) -> int:
    args = build_parser().parse_args(argv)
    suppress_error_dialogs()
    summary, table, out_dir = run_matrix(args)
    print(table)
    print(f"\nverdict: {summary['verdict']}   ({out_dir / 'summary.json'})")
    if args.write_baseline:
        write_baseline(Path(args.write_baseline), summary)
        print(f"baseline written to {args.write_baseline} (reviewed: false)")
    return 0 if summary["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
