#!/usr/bin/env python3
"""Watch whether a test run steals focus or shows up on the taskbar (Windows).

    python scripts/perf/focus_watch.py [--interval-ms 50] [--json OUT] -- COMMAND ...

Runs COMMAND (with no console window of its own) and, every --interval-ms
until it exits, records

* the foreground window (GetForegroundWindow) and whether its owning process
  is COMMAND or one of its descendants (a "test process"), and
* every visible top-level window owned by a test process, and whether it would
  appear on the taskbar / in Alt-Tab (visible, no WS_EX_TOOLWINDOW, no owner,
  or WS_EX_APPWINDOW).

The goal for DS1EDIT_TEST_OFFSCREEN runs is 0 foreground samples and 0
taskbar windows. Exit status: the command's, or 3 when --strict is given and
the watch saw a focus steal or a taskbar window.

The pure parts (process-tree walk, taskbar rule, summary) are unit tested in
test/python/test_focus_watch.py; the Win32 sampling is Windows-only.
"""

import argparse
import json
import os
import subprocess
import sys
import time

try:
    from scripts.winproc import no_window
except ImportError:  # run as a plain script: put the repo root on the path
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    sys.modules.pop("scripts", None)
    from scripts.winproc import no_window

TREE_REFRESH_S = 0.1
WS_EX_TOOLWINDOW = 0x00000080
WS_EX_APPWINDOW = 0x00040000
WS_EX_NOACTIVATE = 0x08000000


# ---------------------------------------------------------------------------
# Pure logic
# ---------------------------------------------------------------------------

def descendants(pairs, root):
    """PIDs of root and everything below it. pairs: iterable of (pid, ppid)."""
    children = {}
    for pid, ppid in pairs:
        children.setdefault(ppid, []).append(pid)
    seen = {root}
    stack = [root]
    while stack:
        for c in children.get(stack.pop(), ()):
            if c not in seen:
                seen.add(c)
                stack.append(c)
    return seen


def update_tree(parents, current_pairs):
    """Fold a process snapshot into ``parents`` (pid -> (ppid, trusted)).

    The first time a pid is seen its edge is *trusted* only if the parent is
    itself in that snapshot. A ppid can name a dead process whose number now
    belongs to someone else, and trusting it would claim an unrelated process
    (and its windows) as a test's. An untrusted edge stays untrusted for as
    long as that pid lives. A pid that reappears with a different parent has
    been reused: its record is replaced. Dead processes keep their record so
    a surviving child still traces back through them. Returns the live pids.
    """
    pairs = list(current_pairs)
    alive = {pid for pid, _ in pairs}
    for pid, ppid in pairs:
        old = parents.get(pid)
        if old is None or old[0] != ppid:
            parents[pid] = (ppid, ppid in alive)
    return alive


def test_pids(parents, alive, root):
    """Live pids that are root or descend from it via trusted edges."""
    out = set()
    for pid in alive:
        cur, hops = pid, 0
        while cur is not None and hops < 64:
            if cur == root:
                out.add(pid)
                break
            rec = parents.get(cur)
            cur = rec[0] if rec and rec[1] else None
            hops += 1
    return out


def is_taskbar_window(visible, ex_style, has_owner):
    """Would the shell list this top-level window on the taskbar / Alt-Tab?"""
    if not visible:
        return False
    if ex_style & WS_EX_APPWINDOW:
        return True
    return not (ex_style & WS_EX_TOOLWINDOW) and not has_owner


def summarize(samples, interval_ms=50):
    """samples: [{"t": s, "fg_pid": int|None, "fg_is_test": bool,
    "windows": [{"hwnd","pid","title","class","taskbar","rect"}]}]"""
    fg = [s for s in samples if s.get("fg_is_test")]
    taskbar = {}
    seen = {}
    first = {}
    tb_samples = 0
    for s in samples:
        for w in s.get("windows", ()):
            first.setdefault(w["hwnd"], dict(w, t=s.get("t")))
            seen[w["hwnd"]] = w
            if w.get("taskbar"):
                taskbar[w["hwnd"]] = w
                tb_samples += 1
    return {
        "samples": len(samples),
        "interval_ms": interval_ms,
        "foreground_samples": len(fg),
        "foreground_pids": sorted({s["fg_pid"] for s in fg}),
        "foreground_times": [s["t"] for s in fg],
        "foreground_detail": sorted({(s.get("fg_class", ""), s.get("fg_ex", 0)) for s in fg}),
        "test_windows_seen": len(seen),
        "taskbar_windows": len(taskbar),
        "taskbar_samples": tb_samples,
        "taskbar_window_titles": sorted({w.get("title", "") for w in taskbar.values()}),
        "windows": sorted(seen.values(), key=lambda w: w["hwnd"]),
        "first_sightings": sorted(first.values(), key=lambda w: w["hwnd"]),
    }


# ---------------------------------------------------------------------------
# Win32 sampling
# ---------------------------------------------------------------------------

def _win32():
    import ctypes
    from ctypes import wintypes
    u = ctypes.WinDLL("user32", use_last_error=True)
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    u.GetForegroundWindow.restype = wintypes.HWND
    u.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    u.IsWindowVisible.argtypes = [wintypes.HWND]
    u.GetWindow.argtypes = [wintypes.HWND, wintypes.UINT]
    u.GetWindow.restype = wintypes.HWND
    u.GetWindowLongW.argtypes = [wintypes.HWND, ctypes.c_int]
    u.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    u.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    u.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    return ctypes, wintypes, u, k


def process_pairs():
    """[(pid, ppid)] for every process, via a Toolhelp snapshot."""
    ctypes, wintypes, _u, k = _win32()

    class PE(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
                    ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_void_p),
                    ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
                    ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
                    ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260)]
    k.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    snap = k.CreateToolhelp32Snapshot(0x2, 0)
    pairs = []
    try:
        pe = PE()
        pe.dwSize = ctypes.sizeof(PE)
        ok = k.Process32FirstW(snap, ctypes.byref(pe))
        while ok:
            pairs.append((pe.th32ProcessID, pe.th32ParentProcessID))
            ok = k.Process32NextW(snap, ctypes.byref(pe))
    finally:
        k.CloseHandle(snap)
    return pairs


def sample_once(test_pids, t):
    ctypes, wintypes, u, _k = _win32()

    def pid_of(hwnd):
        p = wintypes.DWORD(0)
        u.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        return p.value

    fg = u.GetForegroundWindow()
    fg_pid = pid_of(fg) if fg else None
    windows = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, _lp):
        pid = pid_of(hwnd)
        if pid in test_pids and u.IsWindowVisible(hwnd):
            ex = u.GetWindowLongW(hwnd, -20) & 0xFFFFFFFF
            owner = bool(u.GetWindow(hwnd, 4))
            r = wintypes.RECT()
            u.GetWindowRect(hwnd, ctypes.byref(r))
            title = ctypes.create_unicode_buffer(256)
            cls = ctypes.create_unicode_buffer(256)
            u.GetWindowTextW(hwnd, title, 256)
            u.GetClassNameW(hwnd, cls, 256)
            windows.append({"hwnd": int(hwnd), "pid": pid, "title": title.value,
                            "class": cls.value, "ex_style": ex,
                            "taskbar": is_taskbar_window(True, ex, owner),
                            "rect": [r.left, r.top, r.right, r.bottom]})
        return True
    u.EnumWindows(cb, 0)
    fg_class = ""
    fg_ex = 0
    if fg and fg_pid in test_pids:
        cls = ctypes.create_unicode_buffer(256)
        u.GetClassNameW(fg, cls, 256)
        fg_class = cls.value
        fg_ex = u.GetWindowLongW(fg, -20) & 0xFFFFFFFF
    return {"t": round(t, 3), "fg_pid": fg_pid, "fg_class": fg_class, "fg_ex": fg_ex,
            "fg_is_test": bool(fg_pid and fg_pid in test_pids), "windows": windows}


def watch(command, interval_ms=50, cwd=None):
    """Run command and sample until it exits. -> (returncode, summary)."""
    if os.name != "nt":
        raise SystemExit("focus_watch: Windows only")
    proc = subprocess.Popen(command, cwd=cwd, **no_window())
    parents = {}
    pids = {proc.pid}
    samples = []
    t0 = time.monotonic()
    last_tree = -1.0
    while proc.poll() is None:
        now = time.monotonic()
        if now - last_tree >= TREE_REFRESH_S:  # the snapshot is slow; the window poll is not
            last_tree = now
            try:
                alive = update_tree(parents, process_pairs())
                pids = test_pids(parents, alive, proc.pid) | {proc.pid}
            except OSError:
                pass
        samples.append(sample_once(pids, time.monotonic() - t0))
        time.sleep(interval_ms / 1000.0)
    return proc.returncode, summarize(samples, interval_ms)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--interval-ms", type=int, default=50)
    ap.add_argument("--json", help="write the full summary here")
    ap.add_argument("--cwd")
    ap.add_argument("--strict", action="store_true",
                    help="exit 3 if any focus steal or taskbar window was seen")
    ap.add_argument("command", nargs=argparse.REMAINDER)
    args = ap.parse_args(argv)
    cmd = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not cmd:
        ap.error("no command given (put it after --)")
    rc, summ = watch(cmd, args.interval_ms, args.cwd)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(summ, f, indent=1)
    print("focus_watch: %d samples @ %d ms; foreground-in-test-process samples: %d; "
          "test windows seen: %d; taskbar windows: %d (%d samples)"
          % (summ["samples"], summ["interval_ms"], summ["foreground_samples"],
             summ["test_windows_seen"], summ["taskbar_windows"], summ["taskbar_samples"]))
    for w in summ["windows"]:
        print("  window %s pid=%d class=%s title=%r taskbar=%s rect=%s"
              % (hex(w["hwnd"]), w["pid"], w["class"], w["title"], w["taskbar"], w["rect"]))
    if args.strict and (summ["foreground_samples"] or summ["taskbar_windows"]):
        return 3
    return rc


if __name__ == "__main__":
    sys.exit(main())
