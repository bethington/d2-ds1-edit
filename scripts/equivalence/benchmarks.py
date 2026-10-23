"""Repeated-workload timing and resource records (robustness package E, task 3).

``measure_workload`` runs one command as a bounded subprocess, ``warmups``
times unrecorded-as-measurement and then ``repetitions`` times measured, and
returns a ``ds1edit.benchmark/1`` record: per-run wall time, a distribution
summary, resource observations with their scope, the identity of the build and
host before and after, and every output check. It never decides that a time is
"fast enough": comparisons are always ``unreviewed`` until a maintainer
reviews a numeric budget against baseline evidence (design sections 10-11).

Honesty rules enforced here, each covered by test_benchmarks.py:

* A missing, failing or partial memory/handle observer is recorded as
  *unavailable*; it can never turn into "0 bytes of growth".
* Output is checked after every cycle. A run that was fast but produced the
  wrong output is ``output_mismatch``, not a result.
* A change to the build or host identity between the first probe and any later
  run (an executable rebuilt under us, say) invalidates the record.
* A timeout, a cancellation or an exception kills the whole process tree, runs
  the caller's cleanup, and reports what was and was not measured.
* Instrumented (sanitizer/coverage) or non-Release builds are refused as
  timing subjects.
* Cold-start runs are recorded apart from warm samples and are never mixed
  into the warm distribution.
* A strictly rising resource series across runs is flagged for review.
* Records are only comparable like-for-like (same workload, inputs, build
  configuration and host class); otherwise the comparison is ``incomparable``.

Resource scope on Windows: per-process peak working set / peak pagefile usage
and peak open-handle count observed while the process ran (``WindowsObserver``).
Because every cycle is a fresh process, this catches per-process peaks, not
leaks that accumulate across map switches inside one editor session; that needs
an in-process repeat mode and is recorded as a limitation, not as a pass.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import signal
import statistics
import subprocess
try:
    from scripts.winproc import no_window as _no_window
except ImportError:  # run as a plain script: put the repo root on the path
    import os as _os
    import sys as _sys
    _sys.path.insert(0, _os.path.dirname(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))))
    _sys.modules.pop("scripts", None)
    from scripts.winproc import no_window as _no_window
import sys
import threading
import time
from pathlib import Path
from typing import Callable, Optional

SCHEMA = "ds1edit.benchmark/1"
PROFILE_SCHEMA = "ds1edit.performance-profile/1"
TOOL_VERSION = "benchmarks/1"
DEFAULT_REPETITIONS = 10
DEFAULT_WARMUPS = 3

STATUSES = ("pass", "partial", "fail", "timeout", "cancelled", "output_mismatch",
            "command_failed", "identity_changed", "refused")

UNREVIEWED = ("unreviewed: no maintainer-reviewed performance budget exists for "
              "this workload; the distributions are evidence for review, not a pass")


class BenchmarkError(Exception):
    """Invalid use of the harness (bad arguments, not a workload failure)."""


# ---------------------------------------------------------------------------
# Observers
# ---------------------------------------------------------------------------

class NullObserver:
    """Observer that is explicitly unavailable, with the reason on record."""

    def __init__(self, reason: str = "no resource observer configured"):
        self.reason = reason

    def available(self) -> tuple:
        return False, self.reason

    def start(self, proc):  # pragma: no cover - never started when unavailable
        raise BenchmarkError("NullObserver cannot observe")


class WindowsObserver:
    """Peak working set, peak pagefile and peak handle count of one process.

    Polls the process handle every ``interval`` seconds while it runs and once
    more after it exits (the kernel keeps the final peak counters for a handle
    that is still open). Scope: that single process, not its children.
    """

    scope = "per-process peak (psapi GetProcessMemoryInfo, GetProcessHandleCount)"

    def __init__(self, interval: float = 0.005):
        self.interval = interval

    def available(self) -> tuple:
        if os.name != "nt":
            return False, "WindowsObserver only works on Windows"
        try:
            import ctypes  # noqa: F401
        except Exception as exc:  # pragma: no cover
            return False, f"ctypes unavailable: {exc}"
        return True, ""

    def start(self, proc):
        return _WindowsSession(proc, self.interval)


class _WindowsSession:
    def __init__(self, proc, interval):
        import ctypes
        from ctypes import wintypes

        class PMC(ctypes.Structure):
            _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
                        ("PeakWorkingSetSize", ctypes.c_size_t),
                        ("WorkingSetSize", ctypes.c_size_t),
                        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                        ("PagefileUsage", ctypes.c_size_t),
                        ("PeakPagefileUsage", ctypes.c_size_t)]

        self._pmc_type = PMC
        self._k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._k32.K32GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(PMC),
                                                      wintypes.DWORD]
        self._k32.K32GetProcessMemoryInfo.restype = wintypes.BOOL
        self._k32.GetProcessHandleCount.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        self._k32.GetProcessHandleCount.restype = wintypes.BOOL
        self._wintypes = wintypes
        self._ctypes = ctypes
        self._handle = int(proc._handle)  # noqa: SLF001 - the Popen process handle
        self._interval = interval
        self.peak_ws = None
        self.peak_pf = None
        self.peak_handles = None
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def _query(self):
        pmc = self._pmc_type()
        pmc.cb = self._ctypes.sizeof(pmc)
        if self._k32.K32GetProcessMemoryInfo(self._handle, self._ctypes.byref(pmc), pmc.cb):
            self.peak_ws = max(self.peak_ws or 0, int(pmc.PeakWorkingSetSize))
            self.peak_pf = max(self.peak_pf or 0, int(pmc.PeakPagefileUsage))
        count = self._wintypes.DWORD(0)
        if self._k32.GetProcessHandleCount(self._handle, self._ctypes.byref(count)):
            self.peak_handles = max(self.peak_handles or 0, int(count.value))

    def _loop(self):
        while not self._stop.is_set():
            self._query()
            self._stop.wait(self._interval)

    def stop(self) -> Optional[dict]:
        self._stop.set()
        self._thread.join(timeout=2)
        self._query()   # final counters from the still-open handle
        if self.peak_ws is None:
            return None
        return {"peak_working_set_bytes": self.peak_ws,
                "peak_pagefile_bytes": self.peak_pf,
                "peak_handle_count": self.peak_handles}


# ---------------------------------------------------------------------------
# Statistics
# ---------------------------------------------------------------------------

def summarize(values: list) -> Optional[dict]:
    if not values:
        return None
    s = sorted(values)
    n = len(s)
    out = {"n": n, "median": statistics.median(s), "mean": statistics.fmean(s),
           "min": s[0], "max": s[-1],
           "stdev": statistics.stdev(s) if n > 1 else None}
    if n >= 4:
        q = statistics.quantiles(s, n=10, method="inclusive")
        out["p90"] = q[8]
    return out


def resource_trend(series: list) -> dict:
    """Describe a per-run resource series and flag a strictly rising one."""
    n = len(series)
    if n == 0:
        return {"samples": 0, "monotonic_increase": False}
    diffs = [b - a for a, b in zip(series, series[1:])]
    rising_steps = sum(1 for d in diffs if d > 0)
    mono = (n >= 5 and all(d >= 0 for d in diffs) and series[-1] > series[0]
            and rising_steps * 2 >= len(diffs))
    xs = list(range(n))
    slope = None
    if n > 1:
        mx, my = statistics.fmean(xs), statistics.fmean(series)
        den = sum((x - mx) ** 2 for x in xs)
        slope = sum((x - mx) * (y - my) for x, y in zip(xs, series)) / den
    return {"samples": n, "first": series[0], "last": series[-1],
            "min": min(series), "max": max(series),
            "growth": series[-1] - series[0], "slope_per_run": slope,
            "monotonic_increase": bool(mono)}


# ---------------------------------------------------------------------------
# Process control
# ---------------------------------------------------------------------------

DLL_NOT_FOUND = 0xC0000135


def suppress_error_dialogs() -> None:
    """Turn missing-DLL / fault / open-file dialogs into exit codes (Windows).

    Inherited by child processes. Must run before any launch so a broken
    environment cannot put modal dialogs on the user's desktop.
    """
    if os.name == "nt":
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)


def _popen(command, cwd, env):
    suppress_error_dialogs()
    kwargs = {"cwd": str(cwd), "stdin": subprocess.DEVNULL, "stdout": subprocess.PIPE,
              "stderr": subprocess.PIPE, "env": env, "shell": False}
    if os.name == "nt":
        kwargs["creationflags"] = (subprocess.CREATE_NEW_PROCESS_GROUP
                                   | _no_window().get("creationflags", 0))
    else:
        kwargs["start_new_session"] = True
    return subprocess.Popen(command, **kwargs)


def kill_tree(proc) -> None:
    """Terminate ``proc`` and its descendants; tolerate it already being gone."""
    if proc.poll() is not None:
        return
    try:
        if os.name == "nt":
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(proc.pid)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15, **_no_window())
        else:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception:
        pass
    try:
        proc.kill()
    except Exception:
        pass
    try:
        proc.wait(timeout=10)
    except Exception:
        pass


def _drain(pipe, sink: list):
    try:
        sink.append(pipe.read())
    except Exception:
        pass


class _RunOutcome:
    __slots__ = ("wall", "returncode", "stdout", "stderr", "state", "resource", "terminated")

    def __init__(self):
        self.wall = None
        self.returncode = None
        self.stdout = b""
        self.stderr = b""
        self.state = "ok"          # ok | timeout | cancelled | launch_failed
        self.resource = None
        self.terminated = None


def _run_once(command, cwd, env, timeout_s, observer, observer_ok, cancel, clock) -> _RunOutcome:
    out = _RunOutcome()
    t0 = clock()
    try:
        proc = _popen(command, cwd, env)
    except OSError as exc:
        out.state = "launch_failed"
        out.stderr = str(exc).encode()
        return out
    session = None
    if observer_ok:
        try:
            session = observer.start(proc)
        except Exception:
            session = None
    so, se = [], []
    threads = [threading.Thread(target=_drain, args=(proc.stdout, so), daemon=True),
               threading.Thread(target=_drain, args=(proc.stderr, se), daemon=True)]
    for t in threads:
        t.start()
    deadline = t0 + timeout_s
    try:
        while True:
            try:
                proc.wait(timeout=0.02)
                break
            except subprocess.TimeoutExpired:
                pass
            if cancel is not None and cancel.is_set():
                out.state = "cancelled"
                break
            if clock() >= deadline:
                out.state = "timeout"
                break
    finally:
        if out.state != "ok":
            kill_tree(proc)
        out.terminated = proc.poll() is not None
        out.wall = clock() - t0
        out.returncode = proc.returncode
        for t in threads:
            t.join(timeout=5)
        for pipe in (proc.stdout, proc.stderr):
            try:
                pipe.close()
            except Exception:
                pass
        if session is not None:
            try:
                out.resource = session.stop()
            except Exception:
                out.resource = None
    out.stdout = b"".join(so)
    out.stderr = b"".join(se)
    return out


# ---------------------------------------------------------------------------
# measure_workload
# ---------------------------------------------------------------------------

def measure_workload(command: list, cwd, repetitions: int, warmups: int, observer,
                     *, timeout_s: float = 120.0,
                     check: Optional[Callable] = None,
                     setup: Optional[Callable] = None,
                     cleanup: Optional[Callable] = None,
                     cancel: Optional[threading.Event] = None,
                     identity_probe: Optional[Callable] = None,
                     cold_first: bool = False,
                     env: Optional[dict] = None,
                     clock: Callable = time.perf_counter,
                     workload_id: Optional[str] = None,
                     inputs: Optional[dict] = None) -> dict:
    """Run ``command`` and return a ``ds1edit.benchmark/1`` record.

    ``observer`` has ``available() -> (bool, reason)`` and ``start(proc)``
    returning a session whose ``stop()`` yields a dict with any of
    ``peak_working_set_bytes``, ``peak_pagefile_bytes``, ``peak_handle_count``
    (or None). ``None`` as the observer is the same as an unavailable one.

    ``setup(index, phase)`` runs before every execution (phase is ``cold``,
    ``warmup`` or ``measured``); ``check(outcome, index, phase)`` runs after
    every execution and returns ``(ok, detail)``; ``cleanup()`` always runs
    last. ``identity_probe()`` returns a comparable dict, taken before the
    first run and after every run. ``cold_first`` makes the very first
    execution a separately recorded cold-start sample that is neither a warmup
    nor part of the warm distribution (it does not flush the OS cache; the
    record says so).
    """
    if not isinstance(command, (list, tuple)) or not command \
            or not all(isinstance(c, str) for c in command):
        raise BenchmarkError("command must be a non-empty list of strings (no shell)")
    if not isinstance(repetitions, int) or repetitions < 1:
        raise BenchmarkError("repetitions must be an integer >= 1")
    if not isinstance(warmups, int) or warmups < 0:
        raise BenchmarkError("warmups must be an integer >= 0")
    cwd = Path(cwd)
    if not cwd.is_dir():
        raise BenchmarkError(f"cwd is not a directory: {cwd}")

    ok_obs, obs_reason = (False, "no resource observer configured")
    if observer is not None:
        try:
            ok_obs, obs_reason = observer.available()
        except Exception as exc:
            ok_obs, obs_reason = False, f"observer.available() raised: {exc}"

    record = {
        "schema": SCHEMA, "tool": TOOL_VERSION,
        "workload_id": workload_id,
        "command": list(command), "cwd": str(cwd),
        "inputs": inputs or {},
        "repetitions": repetitions, "warmups": warmups, "timeout_s": timeout_s,
        "classification": {"measured": "warm", "cold": "first launch, OS cache not flushed"},
        "cold_samples": [], "warmup_samples": [], "samples": [],
        "failures": [], "flags": [],
        "identity": {"before": None, "after": None, "changed": False},
        "outputs": {"checked_runs": 0, "mismatches": 0, "details": []},
        "cleanup": {"ran": False, "child_terminated": None},
        "comparison": {"status": "unreviewed", "reason": UNREVIEWED},
    }
    status = "pass"

    def finish(st):
        record["status"] = st
        return record

    base_identity = None
    try:
        if identity_probe is not None:
            base_identity = identity_probe()
            record["identity"]["before"] = base_identity
            refusal = identity_refusal(base_identity)
            if refusal:
                record["failures"].append(refusal)
                return finish("refused")

        plan = ([("cold", 0)] if cold_first else []) \
            + [("warmup", i) for i in range(warmups)] \
            + [("measured", i) for i in range(repetitions)]
        series = {"peak_working_set_bytes": [], "peak_pagefile_bytes": [],
                  "peak_handle_count": []}
        observed_runs = 0

        for phase, idx in plan:
            if cancel is not None and cancel.is_set():
                record["failures"].append({"phase": phase, "index": idx, "reason": "cancelled"})
                status = "cancelled"
                break
            if setup is not None:
                setup(idx, phase)
            oc = _run_once(command, cwd, env, timeout_s, observer, ok_obs, cancel, clock)
            if oc.terminated is not None:
                record["cleanup"]["child_terminated"] = oc.terminated
            sample = {"index": idx, "wall_s": round(oc.wall, 6) if oc.wall is not None else None,
                      "returncode": oc.returncode, "resource": oc.resource}
            if oc.state == "launch_failed":
                record["failures"].append({"phase": phase, "index": idx,
                                           "reason": "launch failed: "
                                           + oc.stderr.decode("utf-8", "replace")})
                status = "command_failed"
                break
            if oc.state == "timeout":
                record["failures"].append({"phase": phase, "index": idx,
                                           "reason": f"timed out after {timeout_s}s; process tree killed"})
                status = "timeout"
                break
            if oc.state == "cancelled":
                record["failures"].append({"phase": phase, "index": idx,
                                           "reason": "cancelled; process tree killed"})
                status = "cancelled"
                break
            if oc.returncode in (DLL_NOT_FOUND, DLL_NOT_FOUND - (1 << 32)):
                record["failures"].append({"phase": phase, "index": idx,
                                           "reason": "0xC0000135 DLL not found; campaign aborted"})
                status = "command_failed"
                break
            if oc.returncode != 0:
                tail = oc.stderr.decode("utf-8", "replace")[-300:]
                record["failures"].append({"phase": phase, "index": idx,
                                           "reason": f"exit status {oc.returncode}: {tail}"})
                status = "command_failed"
                break
            if check is not None:
                try:
                    good, detail = check(oc, idx, phase)
                except Exception as exc:
                    good, detail = False, f"output check raised {type(exc).__name__}: {exc}"
                record["outputs"]["checked_runs"] += 1
                if not good:
                    record["outputs"]["mismatches"] += 1
                    record["outputs"]["details"].append({"phase": phase, "index": idx,
                                                         "detail": detail})
                    record["failures"].append({"phase": phase, "index": idx,
                                               "reason": f"output check failed: {detail}"})
                    status = "output_mismatch"
                    break
            if identity_probe is not None:
                now = identity_probe()
                record["identity"]["after"] = now
                if now != base_identity:
                    record["identity"]["changed"] = True
                    record["failures"].append({"phase": phase, "index": idx,
                                               "reason": "build/host identity changed during the run",
                                               "diff": _dict_diff(base_identity, now)})
                    status = "identity_changed"
                    break
            if phase == "cold":
                record["cold_samples"].append(sample)
            elif phase == "warmup":
                record["warmup_samples"].append(sample)
            else:
                record["samples"].append(sample)
                res = oc.resource
                if res:
                    observed_runs += 1
                    for k in series:
                        v = res.get(k)
                        if v is not None:
                            series[k].append(v)
    finally:
        if cleanup is not None:
            try:
                cleanup()
                record["cleanup"]["ran"] = True
            except Exception as exc:
                record["flags"].append(f"cleanup raised {type(exc).__name__}: {exc}")
        if identity_probe is not None and record["identity"]["after"] is None:
            try:
                record["identity"]["after"] = identity_probe()
            except Exception:
                pass

    walls = [s["wall_s"] for s in record["samples"]]
    record["stats"] = summarize(walls)
    record["cold_stats"] = summarize([s["wall_s"] for s in record["cold_samples"]])
    n_meas = len(record["samples"])

    if not ok_obs:
        record["resources"] = {"observed": False, "reason": obs_reason,
                               "series": None, "trend": None}
        record["flags"].append("resource_observer_unavailable")
    else:
        record["resources"] = {
            "observed": observed_runs > 0, "observed_runs": observed_runs,
            "measured_runs": n_meas,
            "scope": getattr(observer, "scope", "observer-defined"),
            "series": series,
            "trend": {k: resource_trend(v) for k, v in series.items() if v},
        }
        if observed_runs == 0:
            record["resources"]["reason"] = "observer started but returned no data"
            record["flags"].append("resource_observation_missing")
        elif observed_runs < n_meas:
            record["flags"].append("resource_observation_incomplete")
        for k, t in record["resources"]["trend"].items():
            if t["monotonic_increase"]:
                record["flags"].append(f"monotonic_increase:{k}")

    if status == "pass" and n_meas < repetitions:
        status = "fail"
    if status == "pass" and any(f.startswith("resource_observ") for f in record["flags"]):
        status = "partial"
    if status == "pass" and "resource_observer_unavailable" in record["flags"]:
        status = "partial"
    return finish(status)


# ---------------------------------------------------------------------------
# Identity and comparison
# ---------------------------------------------------------------------------

def _dict_diff(a, b) -> dict:
    a, b = a or {}, b or {}
    return {k: {"before": a.get(k), "after": b.get(k)}
            for k in sorted(set(a) | set(b)) if a.get(k) != b.get(k)}


def identity_refusal(identity: dict) -> Optional[dict]:
    """Reject instrumented or non-Release builds as timing subjects."""
    build = (identity or {}).get("build") or {}
    cfg = str(build.get("config", "")).lower()
    if build.get("sanitizers") or build.get("coverage"):
        return {"reason": "refused: instrumented (sanitizer/coverage) build is not a "
                          "valid timing subject"}
    if cfg and cfg != "release":
        return {"reason": f"refused: build configuration {build.get('config')!r} is not Release"}
    return None


def compare_records(baseline: dict, candidate: dict) -> dict:
    """Like-for-like comparison. Never pass/fail: ``unreviewed`` or ``incomparable``."""
    reasons = []

    def differ(path, a, b):
        if a != b:
            reasons.append(f"{path} differs: {a!r} vs {b!r}")

    for rec, name in ((baseline, "baseline"), (candidate, "candidate")):
        if rec.get("status") not in ("pass", "partial"):
            reasons.append(f"{name} record status is {rec.get('status')!r}")
        if not rec.get("stats"):
            reasons.append(f"{name} has no measured samples")
    differ("workload_id", baseline.get("workload_id"), candidate.get("workload_id"))
    differ("command", _norm_cmd(baseline.get("command")), _norm_cmd(candidate.get("command")))
    differ("inputs", baseline.get("inputs"), candidate.get("inputs"))
    differ("repetitions", baseline.get("repetitions"), candidate.get("repetitions"))
    differ("warmups", baseline.get("warmups"), candidate.get("warmups"))
    bi = (baseline.get("identity") or {}).get("before") or {}
    ci = (candidate.get("identity") or {}).get("before") or {}
    differ("host class", bi.get("host_class"), ci.get("host_class"))
    differ("build configuration", (bi.get("build") or {}).get("config"),
           (ci.get("build") or {}).get("config"))
    differ("compiler", (bi.get("build") or {}).get("compiler"),
           (ci.get("build") or {}).get("compiler"))
    if reasons:
        return {"status": "incomparable", "reasons": reasons}
    b, c = baseline["stats"], candidate["stats"]
    return {"status": "unreviewed", "reason": UNREVIEWED,
            "baseline_median_s": b["median"], "candidate_median_s": c["median"],
            "median_ratio": (c["median"] / b["median"]) if b["median"] else None,
            "distributions_overlap": c["min"] <= b["max"] and b["min"] <= c["max"],
            "baseline": b, "candidate": c}


def _norm_cmd(cmd):
    # executable and output paths legitimately differ between environments
    return [Path(cmd[0]).name] + list(cmd[1:]) if cmd else cmd


# ---------------------------------------------------------------------------
# Host / build identity (best effort, every field optional and labelled)
# ---------------------------------------------------------------------------

def sha256_file(path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _ps(command: str) -> Optional[str]:
    try:
        r = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", command],
                           capture_output=True, text=True, timeout=30, **_no_window())
        return r.stdout.strip() or None
    except Exception:
        return None


def host_identity() -> dict:
    info = {"os": platform.platform(), "machine": platform.machine(),
            "python": platform.python_version(), "node": platform.node()}
    if os.name == "nt":
        info["cpu"] = _ps("(Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name)")
        info["cpu_logical_cores"] = os.cpu_count()
        gpu = _ps("(Get-CimInstance Win32_VideoController | ForEach-Object "
                  "{ $_.Name + ' / driver ' + $_.DriverVersion }) -join '; '")
        info["gpu"] = gpu
        mem = _ps("[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB,1)")
        info["ram_gb"] = mem
        info["os_build"] = _ps("(Get-CimInstance Win32_OperatingSystem).BuildNumber")
    else:
        info["cpu"] = platform.processor() or None
        info["cpu_logical_cores"] = os.cpu_count()
        info["gpu"] = None
    info["host_class"] = f"{info['os'].split('-')[0]}|{info['machine']}|{info.get('cpu')}"
    return info


def build_identity(exe: Path, build_dir: Optional[Path], repo: Optional[Path],
                   config: str = "Release") -> dict:
    out = {"exe": str(exe), "exe_sha256": sha256_file(exe), "exe_bytes": Path(exe).stat().st_size,
           "config": config, "sanitizers": False, "coverage": False, "compiler": None}
    if build_dir:
        cache = Path(build_dir) / "CMakeCache.txt"
        if cache.is_file():
            text = cache.read_text(errors="replace")
            for key, field in (("DS1EDIT_ENABLE_SANITIZERS", "sanitizers"),
                               ("DS1EDIT_ENABLE_COVERAGE", "coverage")):
                m = re.search(rf"^{key}:BOOL=(\w+)", text, re.M)
                out[field] = bool(m and m.group(1).upper() in ("ON", "1", "TRUE", "YES"))
            m = re.search(r"^CMAKE_GENERATOR:INTERNAL=(.*)$", text, re.M)
            out["generator"] = m.group(1) if m else None
        cc = list((Path(build_dir) / "CMakeFiles").glob("*/CMakeCCompiler.cmake"))
        if cc:
            t = cc[0].read_text(errors="replace")
            idm = re.search(r'set\(CMAKE_C_COMPILER_ID "([^"]*)"', t)
            vm = re.search(r'set\(CMAKE_C_COMPILER_VERSION "([^"]*)"', t)
            out["compiler"] = f"{idm.group(1) if idm else '?'} {vm.group(1) if vm else '?'}"
    if repo:
        def git(*a):
            try:
                r = subprocess.run(["git", "-C", str(repo), *a], capture_output=True, timeout=60, **_no_window())
                return r.stdout if r.returncode == 0 else None
            except Exception:
                return None
        head = git("rev-parse", "HEAD")
        out["git_head"] = head.decode().strip() if head else None
        diff = git("diff", "HEAD")
        out["worktree_diff_sha256"] = hashlib.sha256(diff).hexdigest() if diff is not None else None
        out["worktree_dirty"] = bool(diff) if diff is not None else None
    return out


# ---------------------------------------------------------------------------
# Output checkers
# ---------------------------------------------------------------------------

PNG_SIG = b"\x89PNG\r\n\x1a\n"


def tree_digest(root: Path) -> tuple:
    """(sha256 over relative path + content of every file, file count, total bytes)."""
    h = hashlib.sha256()
    count = total = 0
    for p in sorted(Path(root).rglob("*")):
        if p.is_file():
            rel = p.relative_to(root).as_posix()
            h.update(rel.encode() + b"\0")
            data = p.read_bytes()
            h.update(hashlib.sha256(data).digest())
            count += 1
            total += len(data)
    return h.hexdigest(), count, total


class OutputChecker:
    """Checks the output tree after every cycle against the first cycle's.

    A cycle passes when the output directory has at least ``min_files`` files,
    every ``*.png`` has a valid PNG signature, and the content digest equals
    the reference taken from the first checked cycle (the reference is
    recorded in the benchmark record's ``outputs`` details by the caller).
    """

    def __init__(self, out_dir: Path, min_files: int = 1, stable: bool = True,
                 png_dims: Optional[tuple] = None):
        self.out_dir = Path(out_dir)
        self.min_files = min_files
        self.stable = stable
        self.png_dims = png_dims
        self.reference = None

    def __call__(self, outcome, index, phase):
        if not self.out_dir.is_dir():
            return False, f"output directory missing: {self.out_dir}"
        digest, count, _total = tree_digest(self.out_dir)
        if count < self.min_files:
            return False, f"{count} output file(s), need at least {self.min_files}"
        for p in self.out_dir.rglob("*.png"):
            head = p.read_bytes()[:33]
            if not head.startswith(PNG_SIG):
                return False, f"not a PNG: {p.name}"
            if self.png_dims and len(head) >= 24:
                w = int.from_bytes(head[16:20], "big")
                h = int.from_bytes(head[20:24], "big")
                if (w, h) != tuple(self.png_dims):
                    return False, f"{p.name} is {w}x{h}, expected {self.png_dims}"
        if self.stable:
            if self.reference is None:
                self.reference = digest
            elif digest != self.reference:
                return False, "output differs from the first cycle's output"
        return True, "ok"

    def prepare(self, index, phase):
        if self.out_dir.exists():
            shutil.rmtree(self.out_dir)
        self.out_dir.mkdir(parents=True)


# ---------------------------------------------------------------------------
# Profile runner
# ---------------------------------------------------------------------------

def _first_ini_entry(ini: Path) -> tuple:
    for line in ini.read_text(errors="replace").splitlines():
        line = line.strip()
        if line and not line.startswith(("#", ";")):
            a, b, path = line.split(None, 2)
            return a, b, path.strip()
    raise BenchmarkError(f"{ini} has no entries")


def expand_workloads(profile: dict, env_dir: Path, probe: Optional[Path]) -> list:
    """Turn a profile's workload entries into concrete runnable specs."""
    if profile.get("schema") != PROFILE_SCHEMA:
        raise BenchmarkError(f"profile schema must be {PROFILE_SCHEMA}")
    specs = []
    fixtures = Path(__file__).resolve().parents[2] / "test" / "equivalence" / "fixtures"
    for w in profile["workloads"]:
        if w.get("status") == "not_implemented":
            specs.append({"id": w["id"], "status": "not_run", "reason": w["reason"]})
            continue
        kind = w["kind"]
        if kind == "map-open-headless":
            for ini_name in w["maps"]:
                a, b, ds1 = _first_ini_entry(env_dir / "assets" / f"{ini_name}.ini")
                specs.append({"id": f"{w['id']}:{ini_name}", "kind": kind,
                              "command": ["{exe}", ds1, a, b, "--zoom=1:4", "--headless",
                                          "{out}/map.png"],
                              "needs": ["game-data"], "cold_first": w.get("cold_first", False),
                              "check": {"min_files": 1, "stable": w.get("stable", True)}})
        elif kind in ("probe-decode", "editor-cli"):
            cmd = [c.replace("{fixtures}", str(fixtures)) for c in w["command"]]
            specs.append({"id": w["id"], "kind": kind, "command": cmd,
                          "needs": w.get("needs", []), "cold_first": w.get("cold_first", False),
                          "check": w.get("check", {"min_files": 1, "stable": True})})
        else:
            raise BenchmarkError(f"unknown workload kind {kind!r}")
    return specs


def run_profile(profile: dict, env_dir: Path, exe: Path, probe: Optional[Path],
                out_dir: Path, observer, *, build_dir=None, repo=None,
                only: Optional[list] = None, timeout_s: Optional[float] = None) -> dict:
    # Commands run with cwd=env_dir, so every path handed to them must be absolute.
    env_dir, out_dir = Path(env_dir).resolve(), Path(out_dir).resolve()
    exe = Path(exe).resolve()
    probe = Path(probe).resolve() if probe else None
    out_dir.mkdir(parents=True, exist_ok=True)
    reps = int(profile.get("repetitions", DEFAULT_REPETITIONS))
    warm = int(profile.get("warmups", DEFAULT_WARMUPS))
    host = host_identity()
    # Source state is recorded once per run, not in the per-cycle identity probe:
    # the working tree can legitimately move (other people edit it) while the
    # measured executable, which is what identity pins, does not.
    source_state = build_identity(exe, build_dir, repo)
    results, skipped = [], []
    for spec in expand_workloads(profile, env_dir, probe):
        if only and not any(spec["id"].startswith(o) for o in only):
            continue
        if spec.get("status") == "not_run":
            skipped.append({"id": spec["id"], "status": "not_run", "reason": spec["reason"]})
            continue
        work = out_dir / "work" / re.sub(r"[^A-Za-z0-9_.-]", "_", spec["id"])
        checker = OutputChecker(work, **spec["check"])
        cmd = [c.replace("{exe}", str(exe)).replace("{probe}", str(probe or ""))
                .replace("{out}", str(work)) for c in spec["command"]]
        if "{probe}" in " ".join(spec["command"]) and not probe:
            skipped.append({"id": spec["id"], "status": "unavailable",
                            "reason": "probe executable not provided"})
            continue

        def probe_identity(exe=exe):
            return {"host_class": host["host_class"], "host": host,
                    "build": build_identity(exe, build_dir, None)}

        rec = measure_workload(cmd, env_dir, reps, warm, observer,
                               timeout_s=timeout_s or float(profile.get("timeout_s", 120)),
                               check=checker, setup=checker.prepare,
                               identity_probe=probe_identity,
                               cold_first=spec.get("cold_first", False),
                               workload_id=spec["id"],
                               inputs={"command_template": spec["command"],
                                       "output_reference_digest": None})
        rec["inputs"]["output_reference_digest"] = checker.reference
        rec["inputs"]["game_data"] = _game_data_identity(env_dir) if "game-data" in spec.get("needs", []) \
            or spec["kind"] == "editor-cli" else None
        (out_dir / f"{re.sub(r'[^A-Za-z0-9_.-]', '_', spec['id'])}.json").write_text(
            json.dumps(rec, indent=1, sort_keys=True) + "\n", encoding="utf-8")
        results.append(rec)
        shutil.rmtree(work, ignore_errors=True)
    summary = {"schema": "ds1edit.benchmark-run/1", "profile": profile["id"],
               "host": host, "build": source_state,
               "source_state_note": "git_head/worktree_diff_sha256 describe the tree at capture "
                                    "time; the measured binary is pinned by exe_sha256",
               "results": [
                   {"id": r["workload_id"], "status": r["status"],
                    "median_s": (r.get("stats") or {}).get("median"),
                    "flags": r["flags"]} for r in results],
               "not_run": skipped}
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=1, sort_keys=True) + "\n",
                                          encoding="utf-8")
    return summary


def _game_data_identity(env_dir: Path) -> Optional[dict]:
    """Size and mtime of the configured MPQs (not hashed: multi-hundred-MB)."""
    ini = env_dir / "ds1edit.ini"
    if not ini.is_file():
        return None
    out = {}
    for line in ini.read_text(errors="replace").splitlines():
        m = re.match(r"\s*(d2data|d2exp|d2char|patch_d2)\s*=\s*(.+?)\s*$", line, re.I)
        if m:
            p = Path(m.group(2))
            if p.is_file():
                st = p.stat()
                out[m.group(1).lower()] = {"path": str(p), "bytes": st.st_size,
                                           "mtime": int(st.st_mtime), "hashed": False}
    return out or None


def main(argv: Optional[list] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--profile", type=Path, required=True)
    r.add_argument("--env-dir", type=Path, required=True)
    r.add_argument("--exe", type=Path, required=True)
    r.add_argument("--probe", type=Path)
    r.add_argument("--build-dir", type=Path)
    r.add_argument("--repo", type=Path, default=Path("."))
    r.add_argument("--out-dir", type=Path, required=True)
    r.add_argument("--only", action="append")
    c = sub.add_parser("compare")
    c.add_argument("baseline", type=Path)
    c.add_argument("candidate", type=Path)
    args = ap.parse_args(argv)
    if args.cmd == "compare":
        res = compare_records(json.loads(args.baseline.read_text()),
                              json.loads(args.candidate.read_text()))
        print(json.dumps(res, indent=1))
        return 0 if res["status"] != "incomparable" else 1
    profile = json.loads(args.profile.read_text())
    obs = WindowsObserver()
    if not obs.available()[0]:
        obs = NullObserver(obs.available()[1])
    try:
        summary = run_profile(profile, args.env_dir, args.exe, args.probe, args.out_dir, obs,
                              build_dir=args.build_dir, repo=args.repo, only=args.only)
    except BenchmarkError as exc:
        print(f"benchmarks: {exc}", file=sys.stderr)
        return 2
    for r_ in summary["results"]:
        print(f"{r_['status']:16} {r_['id']}  median={r_['median_s']}  {r_['flags']}")
    for s in summary["not_run"]:
        print(f"{s['status']:16} {s['id']}  {s['reason']}")
    bad = [x for x in summary["results"] if x["status"] not in ("pass", "partial")]
    return 1 if bad else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
